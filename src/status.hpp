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

#ifndef CHATD_STATUS_H
#define CHATD_STATUS_H

#include <queue>
#include <atomic>
#include <vector>
#include "fthread.hpp"

#include "messages.pb.h"
#include "connection.hpp"

class MessageOut;

class MessageIn;

using std::queue;
using std::atomic;

enum kStatusReplyType {
    TYPE_RESYNC,
    TYPE_MESSAGE,
};

class StatusRequest {
public:

    ~StatusRequest() {
        delete message;
    }

    StatusRequest(): type(TYPE_MESSAGE), resync(nullptr), message(nullptr) {}

    StatusRequest(MessageIn* request): type(TYPE_MESSAGE), resync(nullptr) {
        message = request;
    }

    kStatusReplyType type = TYPE_MESSAGE;
    std::vector<ConnectionPtr>* resync = nullptr;
    MessageIn* message = nullptr;
};

class StatusResponse {
public:

    ~StatusResponse() {
        delete message;
    }

    StatusResponse(): type(TYPE_MESSAGE), message(nullptr) {}

    StatusResponse(MessageOut* reply): type(TYPE_MESSAGE) {
        message = reply;
    }

    kStatusReplyType type = TYPE_MESSAGE;
    MessageOut* message = nullptr;
};

#define STATUS_MUTEX_TIMEOUT 250000000

class StatusClient {
public:
    static inline StatusClient* instance() {
        if (_instance != nullptr)
            return _instance;
        _instance = new StatusClient();
        return _instance;
    }


    void handleReply();

    void sendStatusTimeUpdate(ConnectionPtr con, bool disconnect = false);

    void sendStatusUpdate(ConnectionPtr con, uint64_t cookie = 0);

    void sendSubChange(ConnectionPtr con, uint32_t target, SubscriptionChangeIn_ChangeType type, uint64_t cookie);


    static void* runThread(void* param);

    bool addRequest(StatusRequest*);

    StatusRequest* getRequest();

    void addReply(StatusResponse*);

    StatusResponse* getReply();


    void startThread();

    void stopThread();

    void runner();

private:
    pthread_t _thread;

    queue<StatusRequest*> requestQueue;
    queue<StatusResponse*> replyQueue;
    pthread_cond_t requestCondition = PTHREAD_COND_INITIALIZER;
    pthread_mutex_t requestConditionMutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t requestMutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t replyMutex = PTHREAD_MUTEX_INITIALIZER;
    atomic<bool> doRun;
    atomic<bool> connected;
private:
    StatusClient() {
        startThread();
    }

    ~StatusClient() {
        stopThread();
    }

    void startResync();

    void handleReplyResync(StatusResponse*);

    static StatusClient* _instance;
};

#endif