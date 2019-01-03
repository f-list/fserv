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
#include "http_client.hpp"

pthread_mutex_t LoginEvHTTPClient::requestMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t LoginEvHTTPClient::replyMutex = PTHREAD_MUTEX_INITIALIZER;
queue<LoginRequest*> LoginEvHTTPClient::requestQueue;
queue<LoginReply*> LoginEvHTTPClient::replyQueue;
unsigned int LoginEvHTTPClient::maxLoginSlots = 30;
std::atomic<bool> LoginEvHTTPClient::doRun(true);
std::atomic<struct ev_loop*> LoginEvHTTPClient::login_loop(nullptr);
std::atomic<ev_async*> LoginEvHTTPClient::login_async(nullptr);

void LoginEvHTTPClient::responseCallback(HTTPReply* reply) {
    LoginReply* loginReply = new LoginReply();
    loginReply->connection = reply->connection();
    loginReply->success = reply->success() && (reply->status() == 200);
    loginReply->message = reply->body();
    addReply(loginReply);
    delete reply;
    reply = nullptr;
}

void LoginEvHTTPClient::processLogin(LoginRequest* request) {
    string url = StartupConfig::getString("login_url");
    HTTPRequest* httpRequest = new HTTPRequest();
    httpRequest->url(url);
    httpRequest->connection(request->connection);
    httpRequest->method("POST");
    httpRequest->postField("method", "ticket");
    httpRequest->postField("account", request->account);
    httpRequest->postField("ticket", request->ticket);
    httpRequest->postField("char", request->characterName);
    httpRequest->postField("client", request->clientName);
    httpRequest->postField("client_version", request->clientVersion);
    httpRequest->codeCallback(LoginEvHTTPClient::responseCallback);
    HTTPClient::addRequest(httpRequest);
}

void LoginEvHTTPClient::processQueue(struct ev_loop* loop, ev_async* w, int revents) {
    if (!doRun) {
        ev_unloop(login_loop.load(std::memory_order_acquire), EVUNLOOP_ONE);
        return;
    }

    LoginRequest* req = getRequest();
    while (req) {
        processLogin(req);
        delete req;
        req = getRequest();
    }
}

void* LoginEvHTTPClient::runThread(void* param) {
    DLOG(INFO) << "Loginservice: Starting";

    auto loop = ev_loop_new(EVFLAG_AUTO);
    auto async = new ev_async;
    ev_async_init(async, LoginEvHTTPClient::processQueue);
    ev_async_start(loop, async);
    login_async.store(async, std::memory_order_release);
    login_loop.store(loop, std::memory_order_release);

    ev_loop(login_loop, 0);

    //Cleanup
    login_async.store(nullptr, std::memory_order_release);
    delete async;
    ev_async_stop(loop, async);

    login_loop.store(nullptr, std::memory_order_release);
    ev_loop_destroy(loop);
    DLOG(INFO) << "Loginservice: Ending.";
    pthread_exit(NULL);
}

void LoginEvHTTPClient::sendWakeup() {
    if (login_loop.load(std::memory_order_acquire) && login_async.load(std::memory_order_acquire)) {
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


