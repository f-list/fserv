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
#include "server_state.hpp"

#include "logging.hpp"

void sendToTargets(MessagePtr message, const ::google::protobuf::RepeatedField< ::google::protobuf::uint32 >& targets) {
    for(auto itr = targets.begin(); itr != targets.end(); ++itr) {
        auto id = (uint32_t)*itr;
        DLOG(INFO) << "Trying to send last message to id: " << id;
        auto con = ServerState::getConnectionById(id);
        if(con)
            con->send(message);
    }
}

StatusSystem* StatusSystem::_instance = nullptr;

void StatusSystem::handleReply() {
    MessageOut* reply = _client->getReply();
    while(reply) {
        DLOG(INFO) << "Processing reply of type: " << reply->OutMessage_case();
        switch(reply->OutMessage_case()) {
            case MessageOut::kGeneric:
                handleGeneric(reply->generic());
                break;
            case MessageOut::kAck:
                handleAck(reply->ack());
                break;
            case MessageOut::kError:
                break;
            default:
                DLOG(INFO) << "Unknown reply type: " << reply->OutMessage_case();
        }
        delete reply;
        reply = _client->getReply();
    }
}

void StatusSystem::handleGeneric(const GenericOut& message) {
    auto messageOut = new MessageBuffer();
    MessagePtr messagePtr(messageOut);
    switch (message.type()) {
        case MessageType::STATUS:
        case MessageType::INITIAL:
        {
            string outMessage = "STA ";
            outMessage += message.body();
            DLOG(INFO) << "Sending message: " << outMessage;
            messageOut->set(outMessage.data(), outMessage.length());
            break;
        }
        default:
            DLOG(INFO) << "Unknown inner reply type: " << message.type();
            return;
    }
    sendToTargets(messagePtr, message.targets());
}

void StatusSystem::handleAck(const AckOut& message) {
    auto messageOut = new MessageBuffer();
    MessagePtr messagePtr(messageOut);
    string outMessage = "ACK ";
    json_t* root_node = json_object();
    json_object_set_new_nocheck(root_node, "cookie", json_integer(message.cookie()));
    const char* json = json_dumps(root_node, JSON_COMPACT);
    outMessage += json;
    DLOG(INFO) << "Sending message: " << outMessage;
    messageOut->set(outMessage.data(), outMessage.length());
    sendToTargets(messagePtr, message.targets());
    free((void*)json);
    json_decref(root_node);
}

void StatusSystem::sendStatusTimeUpdate(ConnectionPtr con, bool disconnect) {
    auto updateMessage = new MessageIn();
    auto timeMessage = updateMessage->mutable_timeupdate();
    timeMessage->set_name(con->characterName);
    timeMessage->set_characterid(con->characterID);
    timeMessage->set_killsession(disconnect);
    timeMessage->set_sessionid(1ULL);
    timeMessage->set_timestamp(time(nullptr));
    _client->addRequest(updateMessage);
}

void StatusSystem::sendStatusUpdate(ConnectionPtr con, uint64_t cookie) {
    auto statusMessage = new MessageIn();
    auto innerMessage = statusMessage->mutable_statusupdate();
    innerMessage->set_characterid(con->characterID);
    innerMessage->set_status(con->status);
    innerMessage->set_statustext(con->statusMessage);
    innerMessage->set_cookie(cookie);
    innerMessage->set_sessionid(1ULL);
    innerMessage->set_timestamp(time(nullptr));
    _client->addRequest(statusMessage);
}

void StatusSystem::sendSubChange(ConnectionPtr con, uint32_t target, SubscriptionChangeIn_ChangeType type, uint64_t cookie) {
    auto subMessage = new MessageIn();
    auto innerMessage = subMessage->mutable_subscription();
    innerMessage->set_action(type);
    innerMessage->set_sourceid(con->characterID);
    innerMessage->set_targetid(target);
    innerMessage->set_cookie(cookie);
    _client->addRequest(subMessage);
}

