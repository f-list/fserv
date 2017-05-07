/*
 * Copyright (c) 2011-2013, "Kira"
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

#ifndef LOGIN_EV_HTTP_H
#define LOGIN_EV_HTTP_H

#include <queue>
#include "login_common.hpp"
#include "fthread.hpp"
#include <ev.h>

using std::queue;

class HTTPReply;

class LoginEvHTTPClient {
public:
    static void* runThread(void* param);
    static bool addRequest(LoginRequest* newRequest);
    static LoginReply* getReply();

    static void responseCallback(HTTPReply* reply);

    static void setMaxLoginSlots(unsigned int slots) {
        maxLoginSlots = slots;
    }

    static unsigned int getMaxLoginSlots() {
        return maxLoginSlots;
    }

    static void stopThread() {
        doRun = false;
    }
    static void sendWakeup();

    static pthread_mutex_t requestMutex;
    static pthread_mutex_t replyMutex;
    
private:
    static bool addReply(LoginReply* newReply);
    static void processLogin(LoginRequest* request);
    static LoginRequest* getRequest();

    static void processQueue(struct ev_loop* loop, ev_async* w, int revents);

    static queue<LoginReply*> replyQueue;
    static queue<LoginRequest*> requestQueue;
    static unsigned int maxLoginSlots;
    static bool doRun;
    static struct ev_loop* login_loop;
    static ev_async* login_async;
};
#endif //LOGIN_EV_HTTP_H
