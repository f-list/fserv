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
#include "status.hpp"

#include "messagebuffer.hpp"
#include "logging.hpp"
#include "server.hpp"
#include "server_state.hpp"

#include <grpc/grpc.h>
#include <grpc/support/time.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "messages.grpc.pb.h"

#include <thread>

StatusClient* StatusClient::_instance = nullptr;

void sendToTargets(MessagePtr message, const ::google::protobuf::RepeatedField<::google::protobuf::uint32> &targets) {
    for (auto itr = targets.begin(); itr != targets.end(); ++itr) {
        auto id = (uint32_t) *itr;
        DLOG(INFO) << "Trying to send last message to id: " << id;
        auto con = ServerState::getConnectionById(id);
        if (con)
            con->send(message);
    }
}

void handleReplyResync(StatusResponse* reply) {
    auto connections = ServerState::getConnections();
    auto request = new StatusRequest();
    request->type = TYPE_RESYNC;
    request->resync = new std::vector<ConnectionPtr>();
    for (const auto &itr : connections) {
        DLOG(INFO) << "Adding connection: " << itr.second;
        request->resync->push_back(itr.second);
    }
    StatusClient::instance()->addRequest(request);
}

void handleReplyMessageRaw(const RawOut &message) {
    DLOG(INFO) << "Dispatching message: " << message.body();
    auto messageOut = new MessageBuffer();
    MessagePtr messagePtr(messageOut);
    messageOut->set(message.body().data(), message.body().length());
    sendToTargets(messagePtr, message.targets());
}

void handleReplyMessage(StatusResponse* reply) {
    auto message = reply->message;
    DLOG(INFO) << "Processing reply of type: " << message->OutMessage_case();
    switch (message->OutMessage_case()) {
        case MessageOut::kRaw:
            handleReplyMessageRaw(message->raw());
            break;
        case MessageOut::kError:
            break;
        default:
            DLOG(INFO) << "Unknown reply type: " << message->OutMessage_case();
    }
}

void StatusClient::handleReply() {
    auto reply = getReply();
    while (reply) {
        switch (reply->type) {
            case TYPE_RESYNC:
                handleReplyResync(reply);
                break;
            case TYPE_MESSAGE:
                handleReplyMessage(reply);
                break;
        }
        delete reply;
        reply = getReply();
    }
}

void StatusClient::sendStatusTimeUpdate(ConnectionPtr con, bool disconnect) {
    auto updateMessage = new MessageIn();
    auto request = new StatusRequest(updateMessage);
    auto timeMessage = updateMessage->mutable_timeupdate();
    timeMessage->set_name(con->characterName);
    timeMessage->set_characterid(con->characterID);
    timeMessage->set_killsession(disconnect);
    timeMessage->set_sessionid(1ULL);
    timeMessage->set_timestamp(time(nullptr));
    addRequest(request);
}

void StatusClient::sendStatusUpdate(ConnectionPtr con, uint64_t cookie) {
    auto statusMessage = new MessageIn();
    auto request = new StatusRequest(statusMessage);
    auto innerMessage = statusMessage->mutable_statusupdate();
    innerMessage->set_characterid(con->characterID);
    innerMessage->set_status(con->status);
    innerMessage->set_statustext(con->statusMessage);
    innerMessage->set_cookie(cookie);
    innerMessage->set_sessionid(1ULL);
    innerMessage->set_timestamp(time(nullptr));
    addRequest(request);
}

void
StatusClient::sendSubChange(ConnectionPtr con, uint32_t target, SubscriptionChangeIn_ChangeType type, uint64_t cookie) {
    auto subMessage = new MessageIn();
    auto request = new StatusRequest(subMessage);
    auto innerMessage = subMessage->mutable_subscription();
    innerMessage->set_action(type);
    innerMessage->set_sourceid(con->characterID);
    innerMessage->set_targetid(target);
    innerMessage->set_cookie(cookie);
    addRequest(request);
}

void StatusClient::startResync() {
    inResync = true;
    MUT_LOCK(requestMutex);
    while (auto request = requestQueue.front()) {
        delete request;
        requestQueue.pop();
    }
    MUT_UNLOCK(requestMutex);
    auto reply = new StatusResponse();
    reply->type = TYPE_RESYNC;
    addReply(reply);
    inResync = false;
}

// Status Thread
bool StatusClient::addRequest(StatusRequest* request) {
    if (!doRun)
        return false;
    if (inResync && request->type != TYPE_RESYNC) {
        DLOG(INFO) << "Not adding request because in resync and not the right request type.";
        return false;
    }
    struct timespec abs_time;
    clock_gettime(CLOCK_MONOTONIC, &abs_time);
    abs_time.tv_nsec += STATUS_MUTEX_TIMEOUT;
    if (MUT_LOCK(requestMutex)) {
        DLOG(INFO) << "Failed to get lock to add to queue entry.";
        return false;
    }
    requestQueue.push(request);
    MUT_UNLOCK(requestMutex);
    pthread_cond_signal(&requestCondition);
    return true;
}

StatusRequest* StatusClient::getRequest() {
    StatusRequest* message = nullptr;
    MUT_LOCK(requestMutex);
    if (requestQueue.size() > 0) {
        message = requestQueue.front();
        requestQueue.pop();
    }
    MUT_UNLOCK(requestMutex);
    return message;
}

void StatusClient::addReply(StatusResponse* response) {
    if (!doRun)
        return;
    MUT_LOCK(replyMutex);
    replyQueue.push(response);
    MUT_UNLOCK(replyMutex);
    Server::sendStatusWakeup();
}

StatusResponse* StatusClient::getReply() {
    StatusResponse* reply = nullptr;
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

void StatusClient::stopThread() {
    DLOG(INFO) << "Stopping status thread.";
    doRun = false;
    pthread_cond_signal(&requestCondition);
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

void* StatusClient::runThread(void* param) {
    auto client = (StatusClient*) param;
    client->runner();
    pthread_exit(nullptr);
}

MessageIn* generateFill(StatusRequest* request) {
    if (request->resync->empty())
        return nullptr;
    auto message = new MessageIn();
    auto fillMessage = message->mutable_fill();
    int remainingFill = 100;
    while (remainingFill-- && request->resync->back()) {
        auto character = request->resync->back();
        auto fillCharacter = fillMessage->add_characters();
        fillCharacter->set_characterid(character->characterID);
        fillCharacter->set_status(character->status);
        fillCharacter->set_name(character->characterName);
        fillCharacter->set_statustext(character->statusMessage);
        request->resync->pop_back();
    }
    fillMessage->set_final(request->resync->empty());
    return message;
}

void runResync(StatusRequest* request, std::shared_ptr<grpc::ClientReaderWriter<MessageIn, MessageOut> > stream,
               atomic<bool> &restart) {
    DLOG(INFO) << "Starting the resync process";
    if (request->resync->empty())
        return;
    MessageIn resyncMessage;
    resyncMessage.mutable_resync();
    if (!stream->Write(resyncMessage)) {
        restart = true;
    }
    while (auto fillMessage = generateFill(request)) {
        if (!stream->Write(*fillMessage)) {
            delete fillMessage;
            DLOG(INFO) << "Failed to write message to status server in resync.";
            restart = true;
            break;
        }
        delete fillMessage;
    }
    DLOG(INFO) << "Finished sending the resync messages.";
}

void StatusClient::runner() {
    atomic<bool> needs_restart;
    while (doRun) {
        DLOG(INFO) << "Starting status context with server.";
        auto channel = grpc::CreateChannel("statusd:5555", grpc::InsecureChannelCredentials());
        auto stub = StatusSystem::NewStub(channel);
        grpc::ClientContext context;

        std::shared_ptr<grpc::ClientReaderWriter<MessageIn, MessageOut> > stream(stub->StatusSystem(&context));

        if(!channel->WaitForConnected(std::chrono::system_clock::now() + std::chrono::seconds(5))) {
            DLOG(INFO) << "Couldn't ensure that channel connected successfully.";
            continue;
        }

        startResync();

        std::thread writer([&]() {
            MUT_LOCK(requestConditionMutex);
            while (doRun && !needs_restart) {
                pthread_cond_wait(&requestCondition, &requestConditionMutex);
                auto request = getRequest();
                while (doRun && request) {
                    if (request->type == TYPE_MESSAGE) {
                        if (stream->Write(*request->message)) {
                            DLOG(INFO) << "Wrote status client message to server successfully.";
                        } else {
                            DLOG(INFO) << "Failed to write message to status server.";
                            needs_restart = true;
                        }
                    } else if (request->type == TYPE_RESYNC) {
                        runResync(request, stream, needs_restart);
                    }
                    delete request;
                    if (needs_restart || !doRun)
                        break;
                    request = getRequest();
                }
            }
            MUT_UNLOCK(requestConditionMutex);
            stream->WritesDone();
        });

        {
            auto outMessage = new MessageOut();
            auto reply = new StatusResponse(outMessage);
            while (doRun && stream->Read(outMessage)) {
                DLOG(INFO) << "Received message from status system.";
                addReply(reply);
                outMessage = new MessageOut();
                reply = new StatusResponse(outMessage);
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
        }
    }
}

