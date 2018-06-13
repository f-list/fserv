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

#include "grpc.hpp"

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

void StatusClient::runner() {
    atomic<bool> needs_restart;
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
            MUT_LOCK(requestConditionMutex);
            while (doRun && !needs_restart) {
                pthread_cond_wait(&requestCondition, &requestConditionMutex);
                auto request = getRequest();
                while (doRun && request) {
                    if (stream->Write(*request->message)) {
                        DLOG(INFO) << "Wrote message to server";
                    } else {
                        DLOG(INFO) << "Failed to write message to server.";
                        needs_restart = true;
                        break;
                    }
                    delete request;
                    request = getRequest();
                }
            }
            MUT_UNLOCK(requestConditionMutex);
            stream->WritesDone();
        });

        auto outMessage = new MessageOut();
        auto reply = new StatusResponse();
        while (doRun && stream->Read(outMessage)) {
            DLOG(INFO) << "Received message from server";
            reply->message = outMessage;
            addReply(reply);
            reply = new StatusResponse();
            outMessage = new MessageOut();
        }
        delete outMessage;
        auto status = stream->Finish();
        if (!status.ok()) {
            LOG(WARNING) << "gRPC stream failed with " << status.error_message();
        }
        needs_restart = true;
        pthread_cond_signal(&requestCondition);
        writer.join();
        needs_restart = false;
        sleep(5);
    }
}

