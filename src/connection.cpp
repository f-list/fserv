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

#include "connection.hpp"
#include "logging.hpp"
#include "ferror.hpp"
#include "fjson.hpp"
#include "flua.hpp"
#include "server.hpp"
#include "lua_constants.hpp"
#include "channel.hpp"

#define MAX_SEND_QUEUE_ITEMS 150
// This sets the size at which long messages are split into multiple pieces.
// No more than this amount will ever be sent to a single send() call at once.
#define MAX_SEND_QUEUE_ITEM_SIZE 8192

ConnectionInstance::ConnectionInstance()
:
LBase(),
accountID(0),
authStarted(false),
identified(false),
admin(false),
globalModerator(false),
protocol(PROTOCOL_UNKNOWN),
closed(false),
delayClose(false),
status("online"),
gender("None"),
writePosition(0),
loop(0),
pingEvent(0),
timerEvent(0),
readEvent(0),
writeEvent(0),
debugL(0),
refCount(0) {
}

ConnectionInstance::~ConnectionInstance() {
    if (closed != true) {
        LOG(FATAL) << "[BUG] Deleting a connection not marked as closed.";
    }
    if (debugL) {
        lua_close(debugL);
    }
}

bool ConnectionInstance::send(MessagePtr message) {
    // This will disconnect the client as a side effect.
    if (closed || writeQueue.size() > MAX_SEND_QUEUE_ITEMS)
        return false;

    writeQueue.push_back(message);
    ev_io_start(loop, writeEvent);
    return true;
}

bool ConnectionInstance::sendRaw(string& message) {
    if (closed)
        return false;

    MessageBuffer* buffer = new MessageBuffer();
    MessagePtr outMessage(buffer);
    buffer->set(message.data(), message.length());

    writeQueue.push_back(outMessage);
    ev_io_start(loop, writeEvent);
    return true;
}

void ConnectionInstance::sendError(int error) {
    string outstr("ERR ");
    json_t* topnode = json_object();
    json_object_set_new_nocheck(topnode, "number",
            json_integer(error)
            );
    json_object_set_new_nocheck(topnode, "message",
            json_string_nocheck(LuaConstants::getErrorMessage(error).c_str())
            );
    const char* errstr = json_dumps(topnode, JSON_COMPACT);
    outstr += errstr;
    MessagePtr message(MessageBuffer::fromString(outstr));
    send(message);
    free((void*) errstr);
    json_decref(topnode);
    DLOG(INFO) << "Sending error to connection: " << outstr;
}

void ConnectionInstance::sendError(int error, string message) {
    string outstr("ERR ");
    json_t* topnode = json_object();
    json_object_set_new_nocheck(topnode, "number",
            json_integer(error)
            );
    json_t* messagenode = json_string(message.c_str());
    if (!messagenode)
        messagenode = json_string_nocheck("There was an error encoding this error message. Please report this to Kira.");
    json_object_set_new_nocheck(topnode, "message", messagenode);
    const char* errstr = json_dumps(topnode, JSON_COMPACT);
    outstr += errstr;
    MessagePtr outMessage(MessageBuffer::fromString(outstr));
    send(outMessage);
    free((void*) errstr);
    json_decref(topnode);
    DLOG(INFO) << "Sending custom error to connection: " << outstr;
}

void ConnectionInstance::sendDebugReply(string message) {
    string outstr("ZZZ ");
    json_t* topnode = json_object();
    json_t* messagenode = json_string(message.c_str());
    if (!messagenode)
        messagenode = json_string_nocheck("Failed to parse the debug reply as a valid UTF-8 string.");
    json_object_set_new_nocheck(topnode, "message", messagenode);
    const char* replystr = json_dumps(topnode, JSON_COMPACT);
    outstr += replystr;
    free((void*) replystr);
    json_decref(topnode);
    MessagePtr outMessage(MessageBuffer::fromString(outstr));
    send(outMessage);
}

void ConnectionInstance::setDelayClose() {
    delayClose = true;
    ev_io_stop(loop, readEvent);
    ev_timer_stop(loop, timerEvent);
    ev_timer_set(timerEvent, 1., 1.);
    ev_timer_start(loop, timerEvent);
}

void ConnectionInstance::joinChannel(Channel* channel) {
    ChannelPtr chan(channel);
    channelList.push_back(chan);
}

void ConnectionInstance::leaveChannel(Channel* channel) {
    ChannelPtr chan(channel);
    channelList.remove(chan);
}

FReturnCode ConnectionInstance::reloadIsolation(string& output) {
    lua_State* newstate = luaL_newstate();
    FReturnCode ret = Server::loadLuaIntoState(newstate, output, true);
    if (ret == FERR_OK) {
        lua_close(debugL);
        debugL = newstate;
    } else {
        lua_close(newstate);
    }

    return ret;
}

FReturnCode ConnectionInstance::isolateLua(string& output) {
    lua_State* newstate = luaL_newstate();
    FReturnCode ret = Server::loadLuaIntoState(newstate, output, true);
    if (ret == FERR_OK)
        debugL = newstate;
    else
        lua_close(newstate);

    return ret;
}

void ConnectionInstance::deisolateLua() {
    lua_close(debugL);
    debugL = 0;
}
