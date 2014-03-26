/*
 * Copyright (c) 2013, "StormyDragon"
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

#include "login_evhttp.hpp"
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "logging.hpp"
#include "startup_config.hpp"
#include "server.hpp"
#include "connection.hpp"

pthread_mutex_t LoginEvHTTPClient::requestMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t LoginEvHTTPClient::replyMutex = PTHREAD_MUTEX_INITIALIZER;
queue<LoginRequest*> LoginEvHTTPClient::requestQueue;
queue<LoginReply*> LoginEvHTTPClient::replyQueue;
unsigned int LoginEvHTTPClient::maxLoginSlots = 30;
bool LoginEvHTTPClient::doRun = true;
struct ev_loop* LoginEvHTTPClient::login_loop = 0;
ev_async* LoginEvHTTPClient::login_async = 0;
ev_timer* LoginEvHTTPClient::login_timer = 0;
EvHttpClient* LoginEvHTTPClient::client;
map<string, string> LoginEvHTTPClient::login_headers;
CURL* LoginEvHTTPClient::curl_handle = 0;

bool LoginEvHTTPClient::curl_escape_string(string& to_escape) {
    bool res = false;
    char* output = curl_easy_escape(curl_handle, to_escape.c_str(), to_escape.length());
    if (output) {
        to_escape = output;
        curl_free(output);
        res = true;
    }
    return res;
}

bool LoginEvHTTPClient::setupCurlHandle() {
    curl_handle = curl_easy_init();
    return curl_handle != NULL;
}

void LoginEvHTTPClient::response_callback(ResponseInfo *response, void *requestData, void *clientData) {
    LoginReply* reply = static_cast<LoginReply*> (requestData);
    if (response == NULL) {
        DLOG(WARNING) << "Loginservice: Null response received.";
        addReply(reply);
        return;
    }

    if (response->timeout) {
        DLOG(WARNING) << "Loginservice: Request timed out.";
        addReply(reply);
        return;
    }

    DLOG(INFO) << "Loginservice: Received code: " << response->code << " time: " << response->latency;
    reply->message.append(response->response);
    reply->success = true;
    addReply(reply);
}

void LoginEvHTTPClient::processLogin(LoginRequest* request) {
    bool res = false;
    LoginReply* reply = new LoginReply;
    reply->connection = request->connection;
    reply->success = false;

    string url = StartupConfig::getString("loginpath");
    string post = "always200=1&searchData=1&secret=";
    string secret = StartupConfig::getString("loginsecret");
    switch (request->method) {
        case LOGIN_METHOD_TICKET:
            if (!curl_escape_string(secret))
                break;
            post += secret + "&ticket=";
            if (!curl_escape_string(request->ticket))
                break;
            post += request->ticket + "&name=";
            if (!curl_escape_string(request->characterName))
                break;
            post += request->characterName;
            res = true;
            break;
        default:
            DLOG(WARNING) << "Loginservice: Unhandled method used. Method: " << (int) request->method;
            addReply(reply);
            return;
    }

    if (!res) {
        DLOG(WARNING) << "Loginservice: Bad characters in request";
        addReply(reply);
        return;
    }

    DLOG(INFO) << "Sending request: " << url;
    if (client->makePost(LoginEvHTTPClient::response_callback, url, login_headers,
            post, (void*) reply) != 0) {
        DLOG(WARNING) << "Loginservice: Get failed for " << url;
        addReply(reply);
        return;
    }
    return;
}

void LoginEvHTTPClient::processQueue(struct ev_loop* loop, ev_async* w, int revents) {
    if (!doRun) {
        ev_unloop(login_loop, EVUNLOOP_ONE);
        return;
    }

    LoginRequest* req = getRequest();
    while (req) {
        processLogin(req);
        delete req;
        req = getRequest();
    }
}

void LoginEvHTTPClient::timeoutCallback(struct ev_loop* loop, ev_timer* w, int revents) {
    if (!doRun) {
        ev_unloop(login_loop, EVUNLOOP_ONE);
        return;
    }

    ev_timer_again(login_loop, w);
}

void* LoginEvHTTPClient::runThread(void* param) {
    DLOG(INFO) << "Loginservice: Starting";

    setupCurlHandle();
    login_headers.insert(pair<string, string>("User-Agent", StartupConfig::getString("version")));
    login_headers.insert(pair<string, string>("Content-Type","application/x-www-form-urlencoded"));

    login_loop = ev_loop_new(EVFLAG_AUTO);

    double timeout = StartupConfig::getDouble("logintimeout");
    int connection_pool = StartupConfig::getDouble("login_connection_pool");
    client = new EvHttpClient(login_loop, StartupConfig::getString("loginhost"), timeout, NULL, connection_pool);

    login_timer = new ev_timer;
    ev_timer_init(login_timer, LoginEvHTTPClient::timeoutCallback, 0, 5.);
    ev_timer_start(login_loop, login_timer);
    login_async = new ev_async;
    ev_async_init(login_async, LoginEvHTTPClient::processQueue);
    ev_async_start(login_loop, login_async);

    ev_loop(login_loop, 0);

    //Cleanup
    ev_async_stop(login_loop, login_async);
    delete login_async;
    login_async = 0;
    ev_timer_stop(login_loop, login_timer);
    delete login_timer;
    login_timer = 0;

    delete client;
    login_headers.clear();

    ev_loop_destroy(login_loop);
    login_loop = 0;
    DLOG(INFO) << "Loginservice: Ending.";
    pthread_exit(NULL);
}

void LoginEvHTTPClient::sendWakeup() {
    if (login_loop && login_async) {
        //DLOG(INFO) << "Sending a wakeup to the login thread.";
        ev_async_send(login_loop, login_async);
    }
}

LoginRequest* LoginEvHTTPClient::getRequest() {
    LoginRequest* ret = 0;
    //DLOG(INFO) << "Getting login request from queue.";
    MUT_LOCK(requestMutex);
    if (requestQueue.size() > 0) {
        ret = requestQueue.front();
        requestQueue.pop();
    }
    MUT_UNLOCK(requestMutex);
    //DLOG(INFO) << "Finished getting login request from queue.";
    return ret;
}

LoginReply* LoginEvHTTPClient::getReply() {
    LoginReply* ret = 0;
    //DLOG(INFO) << "Getting login reply from queue.";
    MUT_LOCK(replyMutex);
    if (replyQueue.size() > 0) {
        ret = replyQueue.front();
        replyQueue.pop();
    }
    MUT_UNLOCK(replyMutex);
    //DLOG(INFO) << "Finished getting login reply from queue.";
    return ret;
}

bool LoginEvHTTPClient::addReply(LoginReply* newReply) {
    //DLOG(INFO) << "Adding login reply to queue.";
    MUT_LOCK(replyMutex);
    replyQueue.push(newReply);
    MUT_UNLOCK(replyMutex);
    //DLOG(INFO) << "Finished adding login reply to queue.";
    Server::sendWakeup();
    return true;
}

bool LoginEvHTTPClient::addRequest(LoginRequest* newRequest) {
    //DLOG(INFO) << "Adding login request to queue.";
    struct timespec abs_time;
    clock_gettime(CLOCK_REALTIME, &abs_time);
    abs_time.tv_nsec += LOGIN_MUTEX_TIMEOUT;
    if (MUT_TIMEDLOCK(requestMutex, abs_time))
        return false;
    if (requestQueue.size() >= maxLoginSlots) {
        MUT_UNLOCK(requestMutex);
        return false;
    }
    requestQueue.push(newRequest);
    MUT_UNLOCK(requestMutex);
    //DLOG(INFO) << "Finished adding login request to queue.";
    return true;
}
