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

/*
 * HERE BE DRAGONS! (and bad class design)
 */

#include "precompiled_headers.hpp"

#include "server.hpp"
#include "connection.hpp"
#include "startup_config.hpp"
#include "http_client.hpp"
#include "native_command.hpp"
#include "logger_thread.hpp"
#include "lua_chat.hpp"
#include "lua_channel.hpp"
#include "lua_connection.hpp"
#include "lua_constants.hpp"
#include "lua_http.hpp"
#include "lua_testing.hpp"
#include "server_state.hpp"
#include "send_threads.hpp"
#include "md5.hpp"

#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <gperftools/malloc_extension.h>

struct ev_loop* Server::server_loop = nullptr;
ev_async* Server::server_async = nullptr;
ev_async* Server::http_async = nullptr;
ev_timer* Server::server_timer = nullptr;
ev_io* Server::server_listen = nullptr;
ev_io* Server::rtb_listen = nullptr;
ev_prepare* Server::server_prepare = nullptr;
ChatLogThread* Server::chatLogger = nullptr;
SendThreads* Server::sendThreads = nullptr;

lua_State* Server::sL = nullptr;
ev_tstamp Server::luaTimer = 0;
double Server::luaTimeout = 0;
double Server::luaRepeatTimeout = 0;
bool Server::luaInTimeout = false;
bool Server::luaCanTimeout = true;

unsigned long long Server::statAcceptedConnections = 0;
unsigned long long Server::statStartTime = 0;

// 15 seconds
#define BIND_RETRY_TIMEOUT 15000000
#define MAX_WAITING_FOR_ACCEPT 20
#define CONNECTION_TIMEOUT_PERIOD 120.
#define CONNECTION_TIMEOUT_PERIOD_IDENT 30.
#define CONNECTION_PING_TIME CONNECTION_TIMEOUT_PERIOD/4.
// This is 1MB
#define MAX_CONNECTION_READ_BUFFER 0x100000
// This is 8kB
#define MAX_HANDSHAKE_READ_BUFFER 0x2000
//This is the number of Lua instructions to run before checking for a timeout.
#define LUA_TIMEOUT_COUNT 5000000

// The two evil globals in the whole thing.
struct sockaddr_in client_addr;
struct sockaddr_in rtb_addr_allow;

static void sock_nonblock(int socket) {
    int flags = 0;
    flags = fcntl(socket, F_GETFL, 0);
    if (flags != -1)
        fcntl(socket, F_SETFL, flags | O_NONBLOCK);
    static const int dokeepalive = 1;
    setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &dokeepalive, sizeof(dokeepalive));
}

void Server::notifySend(ConnectionInstance* instance) {
    sendThreads->notify(instance);
}

void Server::connectionReadCallback(struct ev_loop* loop, ev_io* w, int revents) {
    auto con = static_cast<ConnectionInstance*> (w->data);

    if (con->closed)
        return;

    if (revents & EV_ERROR) {
        prepareShutdownConnection(con);
    } else if (revents & EV_READ) {
        if (con->readBuffer.size() > MAX_CONNECTION_READ_BUFFER) {
            LOG(WARNING) << "Connection " << inet_ntoa(con->clientAddress.sin_addr) << ":"
                         << ntohs(con->clientAddress.sin_port) <<
                         " exceeded the maximum read buffer size of " << (MAX_CONNECTION_READ_BUFFER / 1024)
                         << "kB and is being closed.";
            prepareShutdownConnection(con);
            return;
        }
        char recvbuffer[8192];
        bzero(&recvbuffer[0], sizeof(recvbuffer));
        int received = recv(w->fd, &recvbuffer[0], sizeof(recvbuffer), 0);
        if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return;
        } else if (received <= 0) {
            //DLOG(INFO) << "Closing connection because of socket error, or closed on other side.";
            prepareShutdownConnection(con);
            return;
        } else {
            con->lastActivity = ev_now(loop);
            con->readBuffer.append(&recvbuffer[0], received);

            string message;
            WebSocketResult ret = WS_RESULT_ERROR;
            int repeat = 10;
            while (--repeat && con->readBuffer.size()) {
                switch (con->protocol) {
                    case PROTOCOL_HYBI:
                        ret = Websocket::Hybi::receive(con, con->readBuffer, message);
                        break;
                    default:
                        break;
                }

                if (ret == WS_RESULT_INCOMPLETE || ret == WS_RESULT_PING_PONG)
                    return;
                else if (ret == WS_RESULT_ERROR || ret == WS_RESULT_CLOSE) {
                    //DLOG(INFO) << "Closing connection because it requested a WS close or caused a WS error.";
                    prepareShutdownConnection(con);
                    return;
                } else {
                    // Smallest valid message size is 3 characters long.
                    size_t message_size = message.size();
                    if (message_size < 3) {
                        //DLOG(INFO) << "Closing connection because it sent a message that was too short.";
                        prepareShutdownConnection(con);
                        return;
                    }

                    string command(message.substr(0, 3));
                    string payload;
                    if (message_size > 4)
                        payload = message.substr(4);

                    //DLOG(INFO) << "Command '" << command << "' payload'" << payload << "'";
                    FReturnCode errorcode = FERR_FATAL_INTERNAL;
                    if (command == "PIN") {
                        errorcode = FERR_OK;
                    } else if (command == "IDN") {
                        errorcode = NativeCommand::IdentCommand(con, payload);
                        if (errorcode != FERR_OK)
                            con->setDelayClose();
                    } else if (command == "FKS") {
                        errorcode = NativeCommand::SearchCommand(con, payload);
                    } else if (command == "ZZZ") {
                        errorcode = NativeCommand::DebugCommand(con, payload);
                    } else if (command == "VAR") {
                        errorcode = runLuaEvent(con, command, payload);
                    } else {
                        if (!con->identified) {
                            errorcode = FERR_REQUIRES_IDENT;
                        } else {
                            errorcode = runLuaEvent(con, command, payload);
                        }
                    }

                    if (errorcode == FERR_REQUIRES_IDENT || errorcode == FERR_FATAL_INTERNAL) {
                        //DLOG(INFO) << "Delay closing connection because it sent a command that requires ident or caused a fatal internal error.";
                        con->setDelayClose();
                        con->sendError(errorcode);
                    } else if (errorcode != FERR_OK) {
                        con->sendError(errorcode);
                    }
                }
            }
        }
    }
}

/*
 * This function does timeouts for connections. It is also used to close connections. This is done to
 * avoid a possible race condition where multiple events are queued for a single item, and the first
 * triggers a close. Deleting the object pointed to by w->data would cause a crash when the next event
 * handler tried to use it. This is accomplished by stopping a connections events, setting con->closed,
 * and setting a timer that expires almost immediately. By the time a connection gets here, the socket is
 * already closed if con->closed is set!
 *
 * The above applies even to the function itself if the connection is not marked closed, and must reschedule
 * a new trigger on the next loop to avoid the race condition.
 *
 * This should be the only way a connection can be closed and cleaned up.
 */
void Server::connectionTimerCallback(struct ev_loop* loop, ev_timer* w, int revents) {
    auto  con = static_cast<ConnectionInstance*> (w->data);
    if (con->closed) {
        DLOG(INFO) << "Shutting down a connection marked as preclosed.";
        //sendClosing(con);
        shutdownConnection(con);
        if (con->identified)
            ServerState::removeConnection(con->characterNameLower);
        else
            ServerState::removedUnidentified(con);

        return;
    } else if (con->delayClose) {
        DLOG(INFO) << "Closing a connection marked for delay close.";
        prepareShutdownConnection(con);
        return;
    }

    ev_tstamp now = ev_now(loop);
    ev_tstamp timeout = con->lastActivity + (con->protocol != PROTOCOL_UNKNOWN ? CONNECTION_TIMEOUT_PERIOD
                                                                               : CONNECTION_TIMEOUT_PERIOD_IDENT);
    if (now > timeout) {
        //HACK: Have to cheat to get the FD here.
        prepareShutdownConnection(con);
    } else {
        w->repeat = timeout - now;
        ev_timer_again(loop, w);
    }
}

void Server::handshakeCallback(struct ev_loop* loop, ev_io* w, int revents) {
    auto con = static_cast<ConnectionInstance*> (w->data);

    if (con->closed)
        return;

    if (revents & EV_ERROR) {
        prepareShutdownConnection(con);
    } else if (revents & EV_READ) {
        if (con->readBuffer.size() > MAX_HANDSHAKE_READ_BUFFER) {
            LOG(WARNING) << "Connection " << inet_ntoa(con->clientAddress.sin_addr) << ":"
                         << ntohs(con->clientAddress.sin_port) <<
                         " exceeded the maximum handshake buffer size of " << (MAX_HANDSHAKE_READ_BUFFER / 1024)
                         << "kB and is being closed.";
            prepareShutdownConnection(con);
            return;
        }
        char recvbuffer[8192];
        bzero(&recvbuffer[0], sizeof(recvbuffer));
        int received = recv(w->fd, &recvbuffer[0], sizeof(recvbuffer), 0);
        if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return;
        } else if (received <= 0) {
            prepareShutdownConnection(con);
            return;
        } else {
            con->lastActivity = ev_now(loop);
            con->readBuffer.append(&recvbuffer[0], received);

            string buffer;
            string ip;
            ProtocolVersion ver = Websocket::Acceptor::accept(con->readBuffer, buffer, ip);
            switch (ver) {
                case PROTOCOL_HYBI:
                    break;
                case PROTOCOL_INCOMPLETE:
                    return;
                case PROTOCOL_UNKNOWN:
                case PROTOCOL_BAD:
                default:
                    prepareShutdownConnection(con);
                    return;

            }
            // Only localhost is allowed to proxy for other clients.
            if (ntohl(con->clientAddress.sin_addr.s_addr) == 0x7f000001) {
                if (inet_pton(AF_INET, ip.c_str(), &con->clientAddress.sin_addr) != 1) {
                    LOG(WARNING) << "Could not determine the endpoint address from the TLS proxy.";
                    prepareShutdownConnection(con);
                    return;
                }
                LOG(INFO) << "Accepted connection from TLS proxy for endpoint: "
                          << inet_ntoa(con->clientAddress.sin_addr);
            }
            con->sendRaw(buffer);
            con->protocol = ver;
            con->readBuffer.clear();
            ev_io* read = new ev_io;
            ev_io_init(read, Server::connectionReadCallback, w->fd, EV_READ);
            read->data = con;
            ev_io_stop(loop, w);
            delete con->readEvent;
            ev_io_start(server_loop, read);
            con->readEvent = read;
        }
    }
}

void Server::listenCallback(struct ev_loop* loop, ev_io* w, int revents) {
    if (!(revents & EV_READ))
        return;

    //DLOG(INFO) << "Listen callback.";

    int socklen = sizeof(client_addr);
    int newfd = accept(w->fd, (sockaddr*) &client_addr, (socklen_t*) &socklen);
    if (newfd > 0) {
        ++statAcceptedConnections;
        {
            char ntopbuf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(client_addr.sin_addr), &ntopbuf[0], INET_ADDRSTRLEN);
            LOG(INFO) << "Incoming connection from: " << &ntopbuf[0] << ":" << ntohs(client_addr.sin_port);
        }
        sock_nonblock(newfd);
        auto newcon = new ConnectionInstance;
        ServerState::addUnidentified(newcon);
        memcpy(&(newcon->clientAddress), &client_addr, sizeof(client_addr));

        newcon->loop = loop;

        ev_timer* ping = new ev_timer;
        ev_timer_init(ping, Server::pingCallback, CONNECTION_PING_TIME, CONNECTION_PING_TIME);
        ping->data = newcon;
        newcon->pingEvent = ping;

        ev_timer* timeout = new ev_timer;
        ev_timer_init(timeout, Server::connectionTimerCallback, CONNECTION_TIMEOUT_PERIOD_IDENT,
                      CONNECTION_TIMEOUT_PERIOD_IDENT);
        timeout->data = newcon;
        ev_timer_start(server_loop, timeout);
        newcon->timerEvent = timeout;

        ev_io* read = new ev_io;
        ev_io_init(read, Server::handshakeCallback, newfd, EV_READ);
        read->data = newcon;
        ev_io_start(server_loop, read);
        newcon->readEvent = read;

        newcon->fileDescriptor = newfd;
        newcon->sendQueue = sendThreads->nextQueue();
    }
}

void Server::rtbCallback(struct ev_loop* loop, ev_io* w, int revents) {
    if (!(revents & EV_READ))
        return;

    static char buffer[1501];
    int addrlen = sizeof(client_addr);
    int recvd = recvfrom(w->fd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*) &client_addr, (socklen_t*) &addrlen);

    if (recvd < 37)
        return;

    buffer[recvd] = '\0';

    if ((client_addr.sin_addr.s_addr != rtb_addr_allow.sin_addr.s_addr) &&
        (client_addr.sin_addr.s_addr != 0x7f000001)) {
        char ntopbuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), &ntopbuf[0], INET_ADDRSTRLEN);
        LOG(INFO) << "Dropping RTB message from unauthorized address " << &ntopbuf[0];
        return;
    }

    string payload(&buffer[32], recvd - 32);
    string mac(&buffer[0], 32);
    string secretpayload(StartupConfig::getString("rtbsecret"));
    secretpayload.append(payload);
    string hash(thirdparty::MD5String(secretpayload));
    if (mac != hash) {
        char ntopbuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), &ntopbuf[0], INET_ADDRSTRLEN);
        LOG(INFO) << "Message hash failed from " << &ntopbuf[0] << " message: " << &buffer[0] << " expected: " << hash;
        return;
    }
    string message = payload.substr(3);
    string command = payload.substr(0, 3);
    runLuaRTB(command, message);
}

void Server::idleTasksCallback(struct ev_loop* loop, ev_timer* w, int revents) {
    DLOG(INFO) << "Idle task callback.";

    ServerState::saveBans();
    ServerState::saveOps();
    ServerState::removeUnusedChannels();
    ServerState::saveChannels();
    ServerState::cleanAltWatchList();
    ServerState::sendUserListToRedis();
    int inuse = lua_gc(sL, LUA_GCCOUNT, 0) * 1024 + lua_gc(sL, LUA_GCCOUNTB, 0);
    lua_gc(sL, LUA_GCCOLLECT, 0);
    int after = lua_gc(sL, LUA_GCCOUNT, 0) * 1024 + lua_gc(sL, LUA_GCCOUNTB, 0);
    DLOG(INFO) << "Garbage collected " << ((inuse - after) / 1024.) << "kB of memory. B: " << (inuse / 1024.) << " A: "
               << (after / 1024.);
    //Force TCMalloc to free up some system memory. We suffer a performance penalty for this, but it is small.
    MallocExtension::instance()->ReleaseFreeMemory();
    ev_timer_again(server_loop, server_timer);
}

void Server::prepareCallback(struct ev_loop* loop, ev_prepare* w, int revents) {
    luaInTimeout = false;
}

void Server::pingCallback(struct ev_loop* loop, ev_timer* w, int revents) {
    static string ping_command("PIN");
    auto con = static_cast<ConnectionInstance*> (w->data);
    MessagePtr outMessage(MessageBuffer::fromString(ping_command));
    con->send(outMessage);
    ev_timer_again(server_loop, w);
}

void Server::processWakeupCallback(struct ev_loop* loop, ev_async* w, int revents) {
    DLOG(INFO) << "Processing async wakeup.";

    LoginReply* reply = LoginEvHTTPClient::getReply();
    while (reply) {
        auto con = reply->connection.get();
        if (!con->closed) {
            FReturnCode code = processLogin(con, reply->message, reply->success);
            if (code != FERR_OK) {
                con->sendError(code);
                con->setDelayClose();
            }
        } else {
            DLOG(WARNING) << "Received login reply for closed connection.";
        }
        delete reply;
        reply = LoginEvHTTPClient::getReply();
    }
}

void Server::processHTTPWakeup(struct ev_loop* loop, ev_async* w, int revents) {
    DLOG(INFO) << "Processing http async wakeup.";

    HTTPReply* reply = HTTPClient::getReply();
    while (reply) {
        auto con = reply->connection().get();
        FReturnCode code = processHTTPReply(reply);
        if (con && !con->closed && code != FERR_OK) {
            con->sendError(code);
        }
        delete reply;
        reply = HTTPClient::getReply();
    }
}

/**
 * Callback format is (con, status, body, extras)
 * @param reply
 * @return
 */
FReturnCode Server::processHTTPReply(HTTPReply* reply) {
    FReturnCode ret = FERR_UNKNOWN;

    luaTimer = luaGetTime();
    json_t* n = json_loads(reply->body().c_str(), 0, 0);

    int top = lua_gettop(sL);
    lua_getglobal(sL, "on_error");
    lua_getglobal(sL, "httpcb");
    lua_getfield(sL, -1, reply->callback().c_str());
    ConnectionPtr con(reply->connection());
    if (con) {
        lua_pushlightuserdata(sL, con.get());
    } else {
        lua_pushnil(sL);
    }
    lua_pushinteger(sL, reply->status());
    if (n) {
        LuaChat::jsonToLua(sL, n);
    } else {
        lua_pushstring(sL, reply->body().c_str());
    }
    json_decref(n);
    lua_pushnil(sL);
    if (lua_pcall(sL, 4, 1, LUA_ABSINDEX(sL, -7))) {
        LOG(WARNING) << "Lua error while calling http_callback. Error returned was: " << lua_tostring(sL, -1);
        lua_pop(sL, 3);
        if (top != lua_gettop(sL)) {
            DLOG(FATAL) << "Did not return stack to its previous condition. O: " << top << " N: " << lua_gettop(sL);
        }
        return FERR_LUA;
    } else {
        ret = lua_tonumber(sL, -1);
        lua_pop(sL, 3);
        if (top != lua_gettop(sL)) {
            DLOG(FATAL) << "Did not return stack to its previous condition. O: " << top << " N: " << lua_gettop(sL);
        }
        return ret;
    }

    return ret;
}

void Server::prepareShutdownConnection(ConnectionInstance* instance) {
    if (instance->closed)
        return;

    if (instance->identified) {
        luaCanTimeout = false;
        int top = lua_gettop(sL);
        lua_getglobal(sL, "on_error");
        lua_getglobal(sL, "event");
        lua_getfield(sL, -1, "pre_disconnect");
        lua_pushlightuserdata(sL, instance);
        if (lua_pcall(sL, 1, 0, LUA_ABSINDEX(sL, -4))) {
            LOG(WARNING) << "Lua error while calling pre_disconnect. Error returned was: " << lua_tostring(sL, -1);
        }
        lua_pop(sL, 2);
        if (top != lua_gettop(sL)) {
            DLOG(FATAL) << "Did not return stack to its previous condition. O: " << top << " N: " << lua_gettop(sL);
        }
        luaCanTimeout = true;

        RedisRequest* req = new RedisRequest;
        req->key = Redis::onlineUsersKey;
        req->method = REDIS_SREM;
        req->updateContext = RCONTEXT_ONLINE;
        req->values.push(instance->characterName);
        if (!Redis::addRequest(req))
            delete req;
    }
    instance->closed = true;
    ev_io_stop(server_loop, instance->readEvent);
    ev_timer_stop(server_loop, instance->pingEvent);
    ev_timer_stop(server_loop, instance->timerEvent);
    ev_timer_set(instance->timerEvent, 0.001, 0.);
    ev_timer_start(server_loop, instance->timerEvent);
}

void Server::shutdownConnection(ConnectionInstance* instance) {
    ev_timer_stop(server_loop, instance->pingEvent);
    delete instance->pingEvent;
    instance->pingEvent = nullptr;

    ev_timer_stop(server_loop, instance->timerEvent);
    delete instance->timerEvent;
    instance->timerEvent = nullptr;

    ev_io_stop(server_loop, instance->readEvent);
    delete instance->readEvent;
    instance->readEvent = nullptr;

    delete instance->writeEvent2;
    instance->writeEvent2 = nullptr;
}

void Server::runTesting() {
    DLOG(INFO) << "Starting in testing mode.";
    initLua();

    luaCanTimeout = false;
    luaInTimeout = false;
    int ret = luaL_dofile(sL, "./script/tests.lua");
    if (ret) {
        LOG(DFATAL) << "Failed to load testing lua script: " << lua_tostring(sL, -1);
        return;
    }

    lua_getglobal(sL, "on_error");
    lua_getglobal(sL, "runTests");
    if (lua_type(sL, -1) == LUA_TFUNCTION) {
        luaTimer = luaGetTime();
        ret = lua_pcall(sL, 0, 0, LUA_ABSINDEX(sL, -2));
        if (ret) {
            LOG(WARNING) << "Error calling 'runTests'. Error returned was: " << lua_tostring(sL, -1);
        }
        luaInTimeout = false;
    } else {
        LOG(WARNING) << "Could not call 'runTests' function. Unexpected type for 'chat_init', expected function, got "
                     << lua_typename(sL, -1);
    }
    lua_pop(sL, 1);

    shutdownLua();
}

void Server::run() {
    statStartTime = time(NULL);
    DLOG(INFO) << "Server starting.";
    //This tells TCMalloc to release every 10 pages or so.
    //From TCMalloc this is (1000.0 / rate) * released_pages(minimum of one)
    MallocExtension::instance()->SetMemoryReleaseRate(100);

    ServerState::loadBans();
    ServerState::loadOps();
    ServerState::loadChannels();
    ServerState::rebuildChannelOpList();
    ServerState::sendUserListToRedis();
    sendThreads = new SendThreads(2);
    sendThreads->start();
    initLua();
    initAsyncLoop();
    initTimer();
    if (StartupConfig::getBool("log_start"))
        loggerStart();

    int listensock = bindAndListen();
    server_listen = new ev_io;
    ev_io_init(server_listen, Server::listenCallback, listensock, EV_READ);
    ev_io_start(server_loop, server_listen);

    if (StartupConfig::getBool("enablertb")) {
        int rtbsock = bindAndListenRTB();
        rtb_listen = new ev_io;
        ev_io_init(rtb_listen, Server::rtbCallback, rtbsock, EV_READ);
        ev_io_start(server_loop, rtb_listen);
    }

    ev_loop(server_loop, 0);

    DLOG(INFO) << "Server stopping.";

    ev_io_stop(server_loop, server_listen);
    delete server_listen;
    server_listen = 0;

    if (StartupConfig::getBool("enablertb")) {
        ev_io_stop(server_loop, rtb_listen);
        delete rtb_listen;
        rtb_listen = 0;
    }

    loggerStop();
    ServerState::saveChannels();
    ServerState::saveOps();
    ServerState::saveBans();
    ServerState::cleanupChannels();
    shutdownTimer();
    shutdownAsyncLoop();
    shutdownLua();
}

void Server::sendWakeup() {
    //DLOG(INFO) << "Sent server an async wakeup.";

    if (server_loop && server_async)
        ev_async_send(server_loop, server_async);
}

void Server::sendHTTPWakeup() {
    if (server_loop && http_async)
        ev_async_send(server_loop, http_async);
}

int Server::bindAndListen() {
    struct sockaddr_in server_addr;
    int re_use = 1;
    int listensock = socket(AF_INET, SOCK_STREAM, 0);
    if (listensock < 0)
        LOG(FATAL) << "Could not create a socket to listen on.";
    sock_nonblock(listensock);
    if (setsockopt(listensock, SOL_SOCKET, SO_REUSEADDR, &re_use, sizeof(re_use)) == -1)
        LOG(FATAL) << "Could not set REUSEADDR";
    if (setsockopt(listensock, SOL_SOCKET, SO_REUSEPORT, &re_use, sizeof(re_use)) == -1)
        LOG(FATAL) << "Could not set REUSEPORT";
    bzero(&server_addr, sizeof(server_addr));
    bzero(&client_addr, sizeof(client_addr));
    server_addr.sin_family = AF_INET;
    struct hostent* bindaddr = gethostbyname(StartupConfig::getString("bindaddr").c_str());
    memcpy(&server_addr.sin_addr.s_addr, bindaddr->h_addr, bindaddr->h_length);
    server_addr.sin_port = htons(static_cast<short> (StartupConfig::getDouble("port")));

    int bindcount = 5;
    while (bindcount--) {
        int ret = bind(listensock, reinterpret_cast<const sockaddr*> (&server_addr), sizeof(server_addr));
        if ((ret < 0) && bindcount) {
            LOG(WARNING) << "Could not bind to socket. Retrying.";
            usleep(BIND_RETRY_TIMEOUT);
        } else if (ret < 0) {
            LOG(FATAL) << "Could not bind to a socket after 5 tries. Giving up.";
        } else
            break;
    }

    if (listen(listensock, MAX_WAITING_FOR_ACCEPT) < 0)
        LOG(FATAL) << "Could not listen on socket.";

    LOG(INFO) << "Bound and listening on socket.";
    return listensock;
}

int Server::bindAndListenRTB() {
    struct hostent* restrict_addr = gethostbyname(StartupConfig::getString("rtbaddress").c_str());
    memcpy(&rtb_addr_allow.sin_addr.s_addr, restrict_addr->h_addr, restrict_addr->h_length);
    struct sockaddr_in server_addr;
    int re_use = 1;
    int listensock = socket(AF_INET, SOCK_DGRAM, 0);
    if (listensock < 0)
        LOG(FATAL) << "Could not create a RTB socket to listen on.";
    sock_nonblock(listensock);
    if (setsockopt(listensock, SOL_SOCKET, SO_REUSEADDR, &re_use, sizeof(re_use)) == -1)
        LOG(FATAL) << "Could not set REUSEADDR";
    if (setsockopt(listensock, SOL_SOCKET, SO_REUSEPORT, &re_use, sizeof(re_use)) == -1)
        LOG(FATAL) << "Could not set REUSEPORT";
    bzero(&server_addr, sizeof(server_addr));
    bzero(&client_addr, sizeof(client_addr));
    server_addr.sin_family = AF_INET;
    struct hostent* bindaddr = gethostbyname(StartupConfig::getString("bindaddr").c_str());
    memcpy(&server_addr.sin_addr.s_addr, bindaddr->h_addr, bindaddr->h_length);
    server_addr.sin_port = htons(static_cast<short> (StartupConfig::getDouble("port")) + 1);

    int bindcount = 5;
    while (bindcount--) {
        int ret = bind(listensock, reinterpret_cast<const sockaddr*> (&server_addr), sizeof(server_addr));
        if ((ret < 0) && bindcount) {
            LOG(WARNING) << "Could not bind to RTB socket. Retrying.";
            usleep(BIND_RETRY_TIMEOUT);
        } else if (ret < 0) {
            LOG(FATAL) << "Could not bind to a RTB socket after 5 tries. Giving up.";
        } else
            break;
    }

    LOG(INFO) << "Bound and listening on RTB socket.";
    return listensock;
}

FReturnCode Server::processLogin(ConnectionInstance* instance, string &message, bool success) {
    FReturnCode ret = FERR_FATAL_INTERNAL;
    if (!success)
        return FERR_LOGIN_TIMED_OUT;

    luaTimer = luaGetTime();
    json_t* n = json_loads(message.c_str(), 0, 0);
    if (!n) {
        return ret;
    }

    int top = lua_gettop(sL);
    lua_getglobal(sL, "on_error");
    lua_getglobal(sL, "event");
    lua_getfield(sL, -1, "ident_callback");
    lua_pushlightuserdata(sL, instance);
    LuaChat::jsonToLua(sL, n);
    json_decref(n);
    if (lua_pcall(sL, 2, 1, LUA_ABSINDEX(sL, -5))) {
        LOG(WARNING) << "Lua error while calling ident_callback. Error returned was: " << lua_tostring(sL, -1);
        lua_pop(sL, 3);
        if (top != lua_gettop(sL)) {
            DLOG(FATAL) << "Did not return stack to its previous condition. O: " << top << " N: " << lua_gettop(sL);
        }
        return FERR_LUA;
    } else {
        ret = lua_tonumber(sL, -1);
        lua_pop(sL, 3);
        if (top != lua_gettop(sL)) {
            DLOG(FATAL) << "Did not return stack to its previous condition. O: " << top << " N: " << lua_gettop(sL);
        }
        ev_timer_start(server_loop, instance->pingEvent);
        return ret;
    }

    return ret;
}

FReturnCode Server::runLuaEvent(ConnectionInstance* instance, string &event, string &payload) {
    luaTimer = luaGetTime();

    json_t* root = json_loads(payload.c_str(), 0, 0);
    if (!root && (payload.length() == 0)) {
        root = json_object();
    } else if (!root) {
        return FERR_BAD_SYNTAX;
    }

    lua_State* L = instance->debugL ? instance->debugL : sL;
    int top = lua_gettop(L);
    lua_getglobal(L, "on_error");
    lua_getglobal(L, "event");
    lua_getfield(L, -1, event.c_str());
    if (lua_type(L, -1) != LUA_TFUNCTION) {
        lua_pop(L, 3);
        if (top != lua_gettop(L)) {
            DLOG(FATAL) << "Did not return stack to its previous condition. O: " << top << " N: " << lua_gettop(L);
        }
        json_decref(root);
        return FERR_UNKNOWN_COMMAND;
    }
    lua_pushlightuserdata(L, instance);
    LuaChat::jsonToLua(L, root);
    json_decref(root);
    int ret = lua_pcall(L, 2, 1, LUA_ABSINDEX(L, -5));
    if (ret != 0) {
        LOG(WARNING) << "Lua error while calling command '" << event << "' with message '" << payload
                     << "'. Error return by Lua: \n" << lua_tostring(L, -1);
        //if(instance->admin)
        instance->sendError(FERR_LUA, lua_tostring(L, -1));
        lua_pop(L, 3);
        if (top != lua_gettop(L)) {
            DLOG(FATAL) << "Did not return stack to its previous condition. O: " << top << " N: " << lua_gettop(L);
        }
        return FERR_LUA;
    } else {
        int returncode = (int) lua_tointeger(L, -1);
        lua_pop(L, 3);
        if (top != lua_gettop(L)) {
            DLOG(FATAL) << "Did not return stack to its previous condition. O: " << top << " N: " << lua_gettop(L);
        }
        return returncode;
    }

    return FERR_FATAL_INTERNAL;
}

void Server::runLuaRTB(string &event, string &payload) {
    luaTimer = luaGetTime();

    json_t* root = json_loads(payload.c_str(), 0, 0);
    if (!root) {
        return;
    }

    lua_State* L = sL;
    int top = lua_gettop(L);
    lua_getglobal(L, "on_error");
    lua_getglobal(L, "rtb");
    lua_getfield(L, -1, event.c_str());
    if (lua_type(L, -1) != LUA_TFUNCTION) {
        lua_pop(L, 3);
        if (top != lua_gettop(L)) {
            DLOG(FATAL) << "Did not return stack to its previous condition. O: " << top << " N: " << lua_gettop(L);
        }
        json_decref(root);
        return;
    }

    LuaChat::jsonToLua(L, root);
    json_decref(root);
    int ret = lua_pcall(L, 1, 1, LUA_ABSINDEX(L, -4));
    if (ret != 0) {
        LOG(WARNING) << "Lua error while calling RTB command '" << event << "' with message '" << payload
                     << "'. Error return by Lua: \n" << lua_tostring(L, -1);
    }
    lua_pop(L, 3);
    if (top != lua_gettop(L)) {
        DLOG(FATAL) << "Did not return stack to its previous condition. O: " << top << " N: " << lua_gettop(L);
    }
}

#define LEVELS1    12    /* size of the first part of the stack */
#define LEVELS2    10    /* size of the second part of the stack */

int Server::luaOnError(lua_State* L1) {
    int level = 1;
    int firstpart = 1; /* still before eventual `...' */
    int arg = 0;
    lua_Debug ar;
    if (lua_gettop(L1) == arg)
        lua_pushliteral(L1, "");
    else if (!lua_isstring(L1, arg + 1)) return 1; /* message is not a string */
    else lua_pushliteral(L1, "\n");
    lua_pushliteral(L1, "stack traceback:");
    while (lua_getstack(L1, level++, &ar)) {
        if (level > LEVELS1 && firstpart) {
            /* no more than `LEVELS2' more levels? */
            if (!lua_getstack(L1, level + LEVELS2, &ar))
                level--; /* keep going */
            else {
                lua_pushliteral(L1, "\n\t..."); /* too many levels */
                while (lua_getstack(L1, level + LEVELS2, &ar)) /* find last levels */
                    level++;
            }
            firstpart = 0;
            continue;
        }
        lua_pushliteral(L1, "\n\t");
        lua_getinfo(L1, "Snl", &ar);
        lua_pushfstring(L1, "%s:", ar.short_src);
        if (ar.currentline > 0)
            lua_pushfstring(L1, "%d:", ar.currentline);
        if (*ar.namewhat != '\0') /* is there a name? */
            lua_pushfstring(L1, " in function "
        LUA_QS, ar.name);
        else {
            if (*ar.what == 'm') /* main? */
                lua_pushfstring(L1, " in main chunk");
            else if (*ar.what == 'C' || *ar.what == 't')
                lua_pushliteral(L1, " ?"); /* C function or tail call */
            else
                lua_pushfstring(L1, " in function <%s:%d>",
                                ar.short_src, ar.linedefined);
        }
        lua_concat(L1, lua_gettop(L1) - arg);
    }
    lua_concat(L1, lua_gettop(L1) - arg);
    return 1;
}

#undef LEVELS1
#undef LEVELS2

int Server::luaPrint(lua_State* L1) {
    int n = lua_gettop(L1);
    if (n != 1)
        return luaL_error(L1, "Print takes one argument.");
    int type = lua_type(L1, -1);
    if (type != LUA_TSTRING && type != LUA_TNUMBER)
        return luaL_error(L1, "Print only accepts numbers or strings.");
    LOG(INFO) << "[LUA] " << lua_tostring(L1, -1);
    lua_pop(L1, 1);
    return 0;
}

int Server::luaError(lua_State* L1) {
    int n = lua_gettop(L1);
    if (n != 1)
        return luaL_error(L1, "error takes one argument."); //You still got what you wanted though!
    if (lua_type(L1, -1) != LUA_TSTRING)
        return luaL_error(L1, "error takes a string.");
    luaL_where(L1, 1);
    lua_pushvalue(L1, 1);
    lua_concat(L1, 2);
    return lua_error(L1);
}

void Server::luaTimeHook(lua_State* L1, lua_Debug* db) {
    if (!luaCanTimeout)
        return;
    DLOG(INFO) << "Event timeout check.";
    double timeout = (luaInTimeout ? luaRepeatTimeout : luaTimeout);
    if (luaGetTime() > luaTimer + timeout) {
        luaInTimeout = true;
        luaL_error(L1, "Event timeout occurred. Event aborted.");
        return;
    }
    return;
}

double Server::luaGetTime() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

double Server::getEventTime() {
    return ev_now(server_loop);
}

void Server::loggerStart() {
    if (chatLogger)
        return;
    chatLogger = new ChatLogThread();
    chatLogger->startThread();
}

void Server::loggerStop() {
    if (!chatLogger)
        return;
    chatLogger->stopThread();
    delete chatLogger;
    chatLogger = 0;
}

FReturnCode Server::reloadLuaState(string &output) {
    lua_State* newstate = luaL_newstate();
    FReturnCode ret = loadLuaIntoState(newstate, output, false);
    if (ret == FERR_OK) {
        lua_close(sL);
        sL = newstate;
        LOG(WARNING) << "The global Lua state has been reloaded.";
    } else {
        lua_close(newstate);
    }

    return ret;
}

// HACK: This function is gross. It messes with the global timeout variables even when it is not modifying the global state.

FReturnCode Server::loadLuaIntoState(lua_State* tL, string &output, bool testing) {
    static const char* mainscript = "./script/main.lua";
    static const char* maintestscript = "./script/main_test.lua";
    DLOG(INFO) << "Loading Lua into an existing state.";

    lua_pushcfunction(tL, luaopen_base);
    lua_pushstring(tL, "");
    lua_pcall(tL, 1, 0, 0);

    lua_pushcfunction(tL, luaopen_math);
    lua_pushstring(tL, "math");
    lua_pcall(tL, 1, 0, 0);

    lua_pushcfunction(tL, luaopen_string);
    lua_pushstring(tL, "string");
    lua_pcall(tL, 1, 0, 0);

    lua_pushcfunction(tL, luaopen_table);
    lua_pushstring(tL, "table");
    lua_pcall(tL, 1, 0, 0);
    luaL_openlibs(tL);

    lua_pushcfunction(tL, LuaChat::openChatLib);
    lua_pushstring(tL, "s");
    lua_pcall(tL, 1, 0, 0);

    lua_pushcfunction(tL, LuaChannel::openChannelLib);
    lua_pushstring(tL, "c");
    lua_pcall(tL, 1, 0, 0);

    lua_pushcfunction(tL, LuaConnection::openConnectionLib);
    lua_pushstring(tL, "u");
    lua_pcall(tL, 1, 0, 0);

    lua_pushcfunction(tL, LuaConstants::openConstantsLib);
    lua_pushstring(tL, "const");
    lua_pcall(tL, 1, 0, 0);

    lua_pushcfunction(tL, LuaHTTP::openHTTPLib);
    lua_pushstring(tL, "http");
    lua_pcall(tL, 1, 0, 0);

#ifndef NDEBUG
    lua_pushcfunction(tL, LuaTesting::openTestingLib);
    lua_pushstring(tL, "testing");
    lua_pcall(tL, 1, 0, 0);
#endif

    lua_pushcfunction(tL, Server::luaPrint);
    lua_setglobal(tL, "print");

    lua_pushcfunction(tL, Server::luaError);
    lua_setglobal(tL, "error");

    lua_pushcfunction(tL, Server::luaOnError);
    lua_setglobal(tL, "on_error");

    if (StartupConfig::getBool("luaenabletimeout")) {
        lua_sethook(sL, Server::luaTimeHook, LUA_MASKCOUNT, LUA_TIMEOUT_COUNT);
    }

    luaTimer = luaGetTime();
    int ret = 0;

    if (testing)
        ret = luaL_dofile(tL, maintestscript);
    else
        ret = luaL_dofile(tL, mainscript);

    if (ret) {
        output = lua_tostring(tL, -1);
        return FERR_LUA;
    }
    luaInTimeout = false;

    lua_getglobal(tL, "on_error");
    if (lua_type(tL, -1) != LUA_TFUNCTION) {
        output = "on_error global is not of the right type. Needs to be a function.";
        return FERR_LUA;
    }
    lua_pop(tL, 1);

    lua_getglobal(tL, "on_error");
    lua_getglobal(tL, "chat_init");
    if (lua_type(tL, -1) == LUA_TFUNCTION) {
        luaTimer = luaGetTime();
        ret = lua_pcall(tL, 0, 0, LUA_ABSINDEX(tL, -2));
        if (ret) {
            luaInTimeout = false;
            output = "Call to chat_init failed with: ";
            output += lua_tostring(tL, -1);
            lua_pop(tL, 1);
            return FERR_LUA;
        }
        luaInTimeout = false;
    } else {
        output = "chat_init is not of the right type. Needs to be a function.";
        lua_pop(tL, 1);
        return FERR_LUA;
    }
    lua_pop(tL, 1);

    return FERR_OK;
}

void Server::initLua() {
    DLOG(INFO) << "Initializing Lua.";

    sL = luaL_newstate();

    lua_pushcfunction(sL, luaopen_base);
    lua_pushstring(sL, "");
    lua_pcall(sL, 1, 0, 0);

    lua_pushcfunction(sL, luaopen_math);
    lua_pushstring(sL, "math");
    lua_pcall(sL, 1, 0, 0);

    lua_pushcfunction(sL, luaopen_string);
    lua_pushstring(sL, "string");
    lua_pcall(sL, 1, 0, 0);

    lua_pushcfunction(sL, luaopen_table);
    lua_pushstring(sL, "table");
    lua_pcall(sL, 1, 0, 0);
    luaL_openlibs(sL);

    lua_pushcfunction(sL, LuaChat::openChatLib);
    lua_pushstring(sL, "s");
    lua_pcall(sL, 1, 0, 0);

    lua_pushcfunction(sL, LuaChannel::openChannelLib);
    lua_pushstring(sL, "c");
    lua_pcall(sL, 1, 0, 0);

    lua_pushcfunction(sL, LuaConnection::openConnectionLib);
    lua_pushstring(sL, "u");
    lua_pcall(sL, 1, 0, 0);

    lua_pushcfunction(sL, LuaConstants::openConstantsLib);
    lua_pushstring(sL, "const");
    lua_pcall(sL, 1, 0, 0);

    lua_pushcfunction(sL, LuaHTTP::openHTTPLib);
    lua_pushstring(sL, "http");
    lua_pcall(sL, 1, 0, 0);

#ifndef NDEBUG
    lua_pushcfunction(sL, LuaTesting::openTestingLib);
    lua_pushstring(sL, "testing");
    lua_pcall(sL, 1, 0, 0);
#endif

    lua_pushcfunction(sL, Server::luaPrint);
    lua_setglobal(sL, "print");

    lua_pushcfunction(sL, Server::luaError);
    lua_setglobal(sL, "error");

    lua_pushcfunction(sL, Server::luaOnError);
    lua_setglobal(sL, "on_error");

    if (StartupConfig::getBool("luaenabletimeout")) {
        luaTimeout = StartupConfig::getDouble("luatimeout");
        luaRepeatTimeout = StartupConfig::getDouble("luarepeattimeout");
        lua_sethook(sL, Server::luaTimeHook, LUA_MASKCOUNT, LUA_TIMEOUT_COUNT);
    }

    luaTimer = luaGetTime();
    int ret = luaL_dofile(sL, "./script/main.lua");
    if (ret) {
        LOG(DFATAL) << "Failed to load chat server lua script: " << lua_tostring(sL, -1);
        return;
    }
    luaInTimeout = false;

    lua_getglobal(sL, "on_error");
    if (lua_type(sL, -1) != LUA_TFUNCTION) {
        LOG(DFATAL) << "Could not find 'on_error' function. Unexpected type for 'on_error', expected function, got "
                    << lua_typename(sL, -1);
    }
    lua_pop(sL, 1);

    lua_getglobal(sL, "on_error");
    lua_getglobal(sL, "chat_init");
    if (lua_type(sL, -1) == LUA_TFUNCTION) {
        luaTimer = luaGetTime();
        ret = lua_pcall(sL, 0, 0, LUA_ABSINDEX(sL, -2));
        if (ret) {
            LOG(WARNING) << "Error calling 'chat_init'. Error returned was: " << lua_tostring(sL, -1);
        }
        luaInTimeout = false;
    } else {
        LOG(WARNING) << "Could not call 'chat_init' function. Unexpected type for 'chat_init', expected function, got "
                     << lua_typename(sL, -1);
    }
    lua_pop(sL, 1);
}

void Server::shutdownLua() {
    DLOG(INFO) << "Shutting down Lua.";
    lua_close(sL);
    sL = 0;
}

void Server::initTimer() {
    DLOG(INFO) << "Initializing idle timer.";
    server_timer = new ev_timer;
    ev_timer_init(server_timer, Server::idleTasksCallback, StartupConfig::getDouble("saveinterval"),
                  StartupConfig::getDouble("saveinterval"));
    ev_timer_start(server_loop, server_timer);
}

void Server::shutdownTimer() {
    DLOG(INFO) << "Shutting down idle timer.";
    ev_timer_stop(server_loop, server_timer);
    delete server_timer;
    server_timer = 0;
}

void Server::initAsyncLoop() {
    DLOG(INFO) << "Initializing event loop and async wakeup.";
    server_loop = ev_default_loop(EVFLAG_AUTO);
    server_prepare = new ev_prepare;
    ev_prepare_init(server_prepare, Server::prepareCallback);
    ev_prepare_start(server_loop, server_prepare);
    server_async = new ev_async;
    ev_async_init(server_async, Server::processWakeupCallback);
    ev_async_start(server_loop, server_async);
    http_async = new ev_async;
    ev_async_init(http_async, Server::processHTTPWakeup);
    ev_async_start(server_loop, http_async);
}

void Server::shutdownAsyncLoop() {
    DLOG(INFO) << "Shutting down event loop and async wakeup.";

    ev_async_stop(server_loop, http_async);
    delete http_async;
    http_async = nullptr;

    ev_async_stop(server_loop, server_async);
    delete server_async;
    server_async = nullptr;

    ev_prepare_stop(server_loop, server_prepare);
    delete server_prepare;
    server_prepare = nullptr;

    //ev_loop_destroy(server_loop);
    server_loop = 0;
}

// TODO: Make this some kind of countdown + graceful disconnect.

void Server::startShutdown() {
    DLOG(INFO) << "Starting a graceful shutdown.";
    ev_unloop(server_loop, EVUNLOOP_ONE);
}
