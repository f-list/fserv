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

#ifndef SERVER_H
#define SERVER_H

#include <ev.h>

#include "logging.hpp"
#include "flua.hpp"
#include "fthread.hpp"
#include "login_evhttp.hpp"
#include "logger_thread.hpp"
#include "redis.hpp"
#include "ferror.hpp"

#include <string>

class SendThreads;
class ConnectionInstance;
class HTTPReply;

using std::string;

class Server {
public:
    static void run();
    static void runTesting();

    static void sendWakeup();
    static void sendHTTPWakeup();
    static FReturnCode loadLuaIntoState(lua_State* tL, string& output, bool testing);
    static FReturnCode reloadLuaState(string& output);
    static void startShutdown();
    static double getEventTime();

    static void notifySend(ConnectionInstance* instance);

    static unsigned long long getAcceptedConnections() {
        return statAcceptedConnections;
    }

    static unsigned long long getStartTime() {
        return statStartTime;
    }

    static inline ChatLogThread* logger() {
        return chatLogger;
    }
    static void loggerStart();

    static void loggerStop();

private:

    Server() { }

    ~Server() { }
    static void processWakeupCallback(struct ev_loop* loop, ev_async* w, int revents);
    static void processHTTPWakeup(struct ev_loop* loop, ev_async* w, int revents);
    static void idleTasksCallback(struct ev_loop* loop, ev_timer* w, int revents);
    static void listenCallback(struct ev_loop* loop, ev_io* w, int revents);
    static void rtbCallback(struct ev_loop* loop, ev_io* w, int revents);
    static void handshakeCallback(struct ev_loop* loop, ev_io* w, int revents);
    static void connectionReadCallback(struct ev_loop* loop, ev_io* w, int revents);
    static void connectionTimerCallback(struct ev_loop* loop, ev_timer* w, int revents);
    static void prepareCallback(struct ev_loop* loop, ev_prepare* w, int revents);
    static void pingCallback(struct ev_loop* loop, ev_timer* w, int revents);

    static void prepareShutdownConnection(ConnectionInstance* instance);
    static void shutdownConnection(ConnectionInstance* instance);

    static int bindAndListen();
    static int bindAndListenRTB();
    static void initTimer();
    static void shutdownTimer();
    static void initLua();
    static void shutdownLua();
    static void initAsyncLoop();
    static void shutdownAsyncLoop();

    static FReturnCode runLuaEvent(ConnectionInstance* instance, string& event, string& payload);
    static void runLuaRTB(string& event, string& payload);
    static int luaOnError(lua_State* L1);
    static int luaError(lua_State* L1);
    static int luaPrint(lua_State* L1);
    static void luaTimeHook(lua_State* L1, lua_Debug* db);
    static double luaGetTime();

    static FReturnCode processLogin(ConnectionInstance* instance, string& message, bool success);
    static FReturnCode processHTTPReply(HTTPReply* reply);

    static struct ev_loop* server_loop;
    static ev_async* server_async;
    static ev_async* http_async;
    static ev_timer* server_timer;
    static ev_io* server_listen;
    static ev_io* rtb_listen;
    static ev_prepare* server_prepare;

    static lua_State* sL;
    static ev_tstamp luaTimer;
    static double luaTimeout;
    static double luaRepeatTimeout;
    static bool luaInTimeout;
    static bool luaCanTimeout;

    static ChatLogThread* chatLogger;
    static SendThreads* sendThreads;

    // Stats
    static unsigned long long statAcceptedConnections;
    static unsigned long long statStartTime;
};

#endif //SERVER_H
