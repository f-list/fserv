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

#include "precompiled_headers.hpp"

#include "grpc.h"

#include "logging.hpp"
#include "server.hpp"

#include <grpc/grpc.h>
#include <grpc/support/time.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include "messages.grpc.pb.h"

#include <thread>

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReaderWriter;
using grpc::Status;

void* StatusClient::runThread(void* param) {
    auto client = (StatusClient*) param;
    client->runner();
    pthread_exit(nullptr);
}

bool StatusClient::addRequest(MessageIn* message) {
    if (!doRun)
        return false;
    struct timespec abs_time;
    clock_gettime(CLOCK_MONOTONIC, &abs_time);
    abs_time.tv_nsec += STATUS_MUTEX_TIMEOUT;
    if (MUT_LOCK(requestMutex)) {
        DLOG(INFO) << "Failed to get lock to add to queue entry.";
        return false;
    }
    requestQueue.push(message);
    MUT_UNLOCK(requestMutex);
    pthread_cond_signal(&requestCondition);
    return true;
}

MessageIn* StatusClient::getRequest() {
    MessageIn* message = nullptr;
    MUT_LOCK(requestMutex);
    if (requestQueue.size() > 0) {
        message = requestQueue.front();
        requestQueue.pop();
    }
    MUT_UNLOCK(requestMutex);
    return message;
}

void StatusClient::addReply(MessageOut* message) {
    if (!doRun)
        return;
    MUT_LOCK(replyMutex);
    replyQueue.push(message);
    MUT_UNLOCK(replyMutex);
    Server::sendStatusWakeup();
}

MessageOut* StatusClient::getReply() {
    MessageOut* reply = nullptr;
    struct timespec abs_time;
    clock_gettime(CLOCK_MONOTONIC, &abs_time);
    abs_time.tv_nsec += STATUS_MUTEX_TIMEOUT;
    if (MUT_TIMEDLOCK(replyMutex, abs_time)) {
        return nullptr;
    }
    if (replyQueue.size() > 0) {
        reply = replyQueue.front();
        replyQueue.pop();
    }
    MUT_UNLOCK(replyMutex);
    return reply;
}

void StatusClient::runner() {
    while (doRun) {
        // TODO: This logic and flow is really messy.
        // Right now it requires that a new message to send comes in before the main loop will time out and try to
        // connect again. The connection function can stall for long periods of time and there needs to be a timeout
        // on it. The system still accepts and processes messages during this period and that isn't ideal because they
        // queue and then all arrive at once, which would result in a flood of connected messages upon startup.
        DLOG(INFO) << "Starting context with server";
        _client = grpc::CreateChannel("statusd:5555", grpc::InsecureChannelCredentials());
        auto _stub = StatusSystem::NewStub(_client);
        ClientContext _context;

        auto stream(_stub->StatusSystem(&_context));

        std::thread writer([&]() {
            bool needs_restart = false;
            MUT_LOCK(requestConditionMutex);
            while (doRun && !needs_restart) {
                pthread_cond_wait(&requestCondition, &requestConditionMutex);
                auto request = getRequest();
                while (doRun && request) {
                    gpr_timespec timeout = gpr_time_from_micros(STATUS_MUTEX_TIMEOUT, GPR_TIMESPAN);
                    _client->WaitForConnected(timeout);
                    if (stream->Write(*request)) {
                        DLOG(INFO) << "Wrote message to server";
                    } else {
                        DLOG(INFO) << "Failed to write message to server.";
                        needs_restart = true;
                        break;
                    }
                    request = getRequest();
                }
            }
            MUT_UNLOCK(requestConditionMutex);
            stream->WritesDone();
        });

        auto outMessage = new MessageOut();
        while (doRun && stream->Read(outMessage)) {
            DLOG(INFO) << "Received message from server";
            addReply(outMessage);
        }
        auto status = stream->Finish();
        if (!status.ok()) {
            LOG(WARNING) << "gRPC stream failed with " << status.error_message();
        }
        writer.join();
    }
}

void StatusClient::stopThread() {
    DLOG(INFO) << "Stopping status thread.";
    doRun = false;
    pthread_join(_thread, nullptr);
}

void StatusClient::startThread() {
    if (_thread)
        return;
    doRun = true;
    pthread_attr_t threadAttrs;
    pthread_attr_init(&threadAttrs);
    pthread_attr_setdetachstate(&threadAttrs, PTHREAD_CREATE_JOINABLE);
    pthread_create(&_thread, &threadAttrs, &StatusClient::runThread, this);
}