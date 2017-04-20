/*
 * Copyright (c) 2013, "Kira"
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "precompiled_headers.hpp"

#include "http_client.hpp"
#include "server.hpp"
#include <sstream>

using std::stringstream;

#define HTTP_MUTEX_TIMEOUT 250000000

pthread_mutex_t HTTPClient::replyMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t HTTPClient::requestMutex = PTHREAD_MUTEX_INITIALIZER;
queue<HTTPRequest*> HTTPClient::requestQueue;
queue<HTTPReply*> HTTPClient::replyQueue;
struct ev_loop* HTTPClient::loop = nullptr;
ev_timer* HTTPClient::timer = nullptr;
ev_async* HTTPClient::async = nullptr;
CURLM* HTTPClient::multiHandle = nullptr;
CURL* HTTPClient::escapeHandle = nullptr;
bool HTTPClient::doRun = true;

string &HTTPRequest::postString() {
    if (materializedPost.length())
        return materializedPost;
    stringstream builder;
    bool first = true;
    for (auto itr = _postValues.begin(); itr != _postValues.end(); itr++) {
        if (!first)
            builder << "&";
        builder << itr->first << "=" << itr->second;
        first = false;
    }

    materializedPost.assign(builder.str());
    return materializedPost;
}

void HTTPRequest::postField(string name, string value) {
    string _value = value;
    HTTPClient::escapeSegment(_value);
    _postValues[name] = _value;
}

int HTTPClient::curlSocketCallback(CURL* easy, curl_socket_t socket, int what, HTTPRequest* socketRequest,
                                   HTTPRequest* request) {
    DLOG(INFO) << "Socket callback w:" << what << " socket: " << socket;

    if (what == CURL_POLL_REMOVE) {
        if (request == nullptr) {
            LOG(ERROR) << "No request assigned to curl socket.";
            return 0;
        }
        if (request) {
            DLOG(INFO) << "Destroying IO on request.";
            request->stopIO();
        }
    } else {
        int ev_want = (what & CURL_POLL_IN ? EV_READ : 0) | (what & CURL_POLL_OUT ? EV_WRITE : 0);
        if (!request) {
            curl_easy_getinfo(easy, CURLINFO_PRIVATE, &request);
            ev_io* io = new ev_io;
            io->data = request;
            request->setIO(loop, io);
            ev_io_init(io, HTTPClient::readwriteCallback, socket, ev_want);
            ev_io_start(loop, io);
            curl_multi_assign(multiHandle, socket, request);
        } else {
            curl_multi_assign(multiHandle, socket, request);
            request->setIOWant(socket, ev_want);
        }
    }
    return 0;
}

void HTTPClient::readwriteCallback(struct ev_loop* loop, ev_io* w, int events) {
    CURLMcode err;
    int running = 0;
    int curl_what = (events & EV_READ ? CURL_POLL_IN : 0) | (events & EV_WRITE ? CURL_POLL_OUT : 0);
    err = curl_multi_socket_action(multiHandle, w->fd, curl_what, &running);
    if (err != CURLM_OK) {

    }
    // TODO handle curl error, but how?
    if (running <= 0)
        ev_timer_stop(loop, timer);
    checkForFinishedRequests();
}

void HTTPClient::checkForFinishedRequests() {
    int messages = 0;
    CURLMsg* message = NULL;
    CURL* easy = NULL;
    while ((message = curl_multi_info_read(multiHandle, &messages)) != NULL) {
        if (message->msg == CURLMSG_DONE) {
            easy = message->easy_handle;
            curl_multi_remove_handle(multiHandle, easy);
            HTTPRequest* request = NULL;
            curl_easy_getinfo(easy, CURLINFO_PRIVATE, &request);
            if (request) {
                processResponseDone(request);
            }
            curl_easy_cleanup(easy);
        }
    }
}

void HTTPClient::processResponseDone(HTTPRequest* request) {
    DLOG(INFO) << "Request done.";
    int status = 499;
    curl_easy_getinfo(request->curlHandle(), CURLINFO_HTTP_CODE, &status);
    request->reply()->status(status);
    DLOG(INFO) << "Request done with status: " << status << " body: " << request->reply()->body();
    if (request->codeCallback()) {
        DLOG(INFO) << "Calling code callback.";
        request->codeCallback()(request->reply());
        delete request;
        return;
    }
    DLOG(INFO) << "Calling lua callback.";
    addReply(request->reply());
    delete request;
}

void HTTPClient::timeoutCallback(struct ev_loop* loop, ev_timer* w, int events) {
    DLOG(INFO) << "EV timeout callback.";
    int running = 0;
    curl_multi_socket_action(multiHandle, CURL_SOCKET_TIMEOUT, 0, &running);
    checkForFinishedRequests();
}

int HTTPClient::curlTimerCallback(CURLM* multi, long timeoutMS, void* userp) {
    DLOG(INFO) << "CURL timer callback with timeout of: " << timeoutMS << "ms.";
    ev_timer_stop(loop, timer);
    if (timeoutMS > 0) {
        double timeout = timeoutMS / 1000.0;
        ev_timer_set(timer, timeout, 0.);
        ev_timer_start(loop, timer);
    } else {
        timeoutCallback(loop, timer, 0);
    }
    return 0;
}

void HTTPClient::processRequest(HTTPRequest* request) {
    DLOG(INFO) << "Processing new HTTP request for url: " << request->url();
    CURL* easy = curl_easy_init();
    CURLMcode err;
    if (!easy) {
        LOG(WARNING) << "Failed to create easy curl handle for request with url: " << request->url();
        delete request;
        return;
    }
    request->curlHandle(easy);
    curl_easy_setopt(easy, CURLOPT_URL, request->url().c_str());
    curl_easy_setopt(easy, CURLOPT_PRIVATE, request);
    curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, HTTPClient::curlWriteCallback);
    curl_easy_setopt(easy, CURLOPT_WRITEDATA, request);
    curl_easy_setopt(easy, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(easy, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(easy, CURLOPT_USERAGENT, "fserv 0.9 (by Kira)");
    if (request->method() == "POST") {
        curl_easy_setopt(easy, CURLOPT_POST, 1L);
        curl_easy_setopt(easy, CURLOPT_POSTFIELDS, request->postString().c_str());
    }
    err = curl_multi_add_handle(multiHandle, easy);
    if (err != CURLM_OK) {
        LOG(WARNING) << "Failed to add easy to multi with url: " << request->url();
        curl_easy_cleanup(easy);
        delete request;
        return;
    }
}

void HTTPClient::processQueue(struct ev_loop* loop, ev_async* w, int events) {
    if (!doRun) {
        DLOG(INFO) << "Stopping thread.";
        ev_unloop(loop, EVUNLOOP_ONE);
        return;
    }

    HTTPRequest* request = getRequest();
    while (request != nullptr) {
        processRequest(request);
        request = getRequest();
    }
}

size_t HTTPClient::curlWriteCallback(void* data, size_t size, size_t count, HTTPRequest* request) {
    request->reply()->append((const char*) data, size * count);
    return size * count;
}


void HTTPClient::init() {
    escapeHandle = curl_easy_init();
    multiHandle = curl_multi_init();
    curl_multi_setopt(multiHandle, CURLMOPT_SOCKETFUNCTION, HTTPClient::curlSocketCallback);
    curl_multi_setopt(multiHandle, CURLMOPT_TIMERFUNCTION, HTTPClient::curlTimerCallback);
    curl_multi_setopt(multiHandle, CURLMOPT_MAX_HOST_CONNECTIONS, 15L);
    curl_multi_setopt(multiHandle, CURLMOPT_MAX_TOTAL_CONNECTIONS, 30L);
    curl_multi_setopt(multiHandle, CURLMOPT_PIPELINING, 1L);
    curl_multi_setopt(multiHandle, CURLMOPT_MAX_PIPELINE_LENGTH, 2L);

    loop = ev_loop_new(EVFLAG_AUTO);
    timer = new ev_timer;
    ev_timer_init(timer, HTTPClient::timeoutCallback, 0., 0.);
    async = new ev_async;
    ev_async_init(async, HTTPClient::processQueue);
    ev_async_start(loop, async);
}

void* HTTPClient::runThread(void* param) {
    init();

    ev_loop(loop, 0);

    ev_timer_stop(loop, timer);
    delete timer;
    timer = 0;
    ev_loop_destroy(loop);
    loop = 0;

    // This is almost certain to leak memory.
    curl_multi_cleanup(multiHandle);

    pthread_exit(NULL);
}

void HTTPClient::sendWakeup() {
    if (doRun && loop) {
        ev_async_send(loop, async);
    }
}

bool HTTPClient::addRequest(HTTPRequest* request) {
    struct timespec abs_time;
    clock_gettime(CLOCK_REALTIME, &abs_time);
    abs_time.tv_nsec += HTTP_MUTEX_TIMEOUT;
    if (MUT_TIMEDLOCK(requestMutex, abs_time))
        return false;
    requestQueue.push(request);
    MUT_UNLOCK(requestMutex);
    sendWakeup();
    return true;
}

HTTPRequest* HTTPClient::getRequest() {
    HTTPRequest* ret = nullptr;
    MUT_LOCK(requestMutex);
    if (requestQueue.size() > 0) {
        ret = requestQueue.front();
        requestQueue.pop();
    }
    MUT_UNLOCK(requestMutex);

    return ret;
}

void HTTPClient::addReply(HTTPReply* reply) {
    MUT_LOCK(replyMutex);
    replyQueue.push(reply);
    MUT_UNLOCK(replyMutex);
    Server::sendHTTPWakeup();
}

HTTPReply* HTTPClient::getReply() {
    HTTPReply* reply = nullptr;
    MUT_LOCK(replyMutex);
    if (replyQueue.size() > 0) {
        reply = replyQueue.front();
        replyQueue.pop();
    }
    MUT_UNLOCK(replyMutex);

    return reply;
}