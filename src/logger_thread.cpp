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

#include "logger_thread.hpp"
#include "logging.hpp"
#include "fjson.hpp"
#include "startup_config.hpp"

#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string>

using std::string;

static void sock_nonblock(int socket) {
    int flags = 0;
    flags = fcntl(socket, F_GETFL, 0);
    if (flags != -1)
        fcntl(socket, F_SETFL, flags | O_NONBLOCK);
}

const char* LogEntry::asJSON() {
    if (jsonCopy.length())
        return jsonCopy.c_str();
    json_t* root = json_object();
    json_object_set_new_nocheck(root, "message_type", json_string_nocheck(messageType.c_str()));
    json_object_set_new_nocheck(root, "from_character", json_string_nocheck(fromCharacter.c_str()));
    json_object_set_new_nocheck(root, "to_character",
                                toCharacter.length() ? json_string_nocheck(toCharacter.c_str()) : json_null());
    json_object_set_new_nocheck(root, "body",
                                messageBody.length() ? json_string_nocheck(messageBody.c_str()) : json_null());
    json_object_set_new_nocheck(root, "to_character_id",
                                toCharacterID != -1 ? json_integer(toCharacterID) : json_null());
    json_object_set_new_nocheck(root, "to_account_id", toAccountID != -1 ? json_integer(toAccountID) : json_null());
    json_object_set_new_nocheck(root, "from_character_id", json_integer(fromCharacterID));
    json_object_set_new_nocheck(root, "from_account_id", json_integer(fromAccountID));
    json_object_set_new_nocheck(root, "channel",
                                toChannel.length() ? json_string_nocheck(toChannel.c_str()) : json_null());
    json_object_set_new_nocheck(root, "channel_title",
                                toChannelTitle.length() ? json_string_nocheck(toChannelTitle.c_str()) : json_null());

    char date_buffer[512];
    int marker = snprintf(date_buffer, sizeof(date_buffer), "%llu", time);
    date_buffer[marker] = 0;
    json_object_set_new_nocheck(root, "date", json_string_nocheck(date_buffer));

    jsonCopy = json_dumps(root, JSON_COMPACT);
    json_decref(root);
    jsonCopy.append("\n");
    return jsonCopy.c_str();
}

LoggerConnection::LoggerConnection(int fd, struct ev_loop* loop, ChatLogThread* instance) :
        loop(loop),
        writer_io(0),
        fd(fd),
        writeOffset(0),
        logger(instance) {
    writer_io = new ev_io;
    ev_io_init(writer_io, LoggerConnection::writeCallback, fd, EV_WRITE);
    writer_io->data = this;
    logger->addConnection(this);
}

LoggerConnection::~LoggerConnection() {
    ev_io_stop(loop, writer_io);
    delete writer_io;
    close(fd);
    logger->removeConnection(this);
}

void LoggerConnection::sendEntry(LogEntryPtr entry) {
    entryQueue.push(entry);
    ev_io_start(loop, writer_io);
}

void LoggerConnection::writeCallback(struct ev_loop* loop, ev_io* w, int events) {
    LoggerConnection* con = (LoggerConnection*) w->data;

    if (events & EV_ERROR) {
        delete con;
        return;
    }

    if (events & EV_WRITE) {
        while (con->entryQueue.size()) {
            LogEntryPtr entry = con->entryQueue.front();
            const char* entryValue = entry->asJSON();
            int len = (int) strlen(entryValue) - con->writeOffset;
            int sent = send(w->fd, entryValue + con->writeOffset, len, 0);
            if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                return;
            } else if (sent <= 0) {
                close(w->fd);
                return;
            } else if (sent != len) {
                con->writeOffset += sent;
                return;
            } else {
                con->entryQueue.pop();
                con->writeOffset = 0;
            }
        }
        ev_io_stop(loop, w);
    }
}

void ChatLogThread::addConnection(LoggerConnection* connection) {
    connectionList.push_back(connection);
}

void ChatLogThread::removeConnection(LoggerConnection* connection) {
    connectionList.remove(connection);
}

void ChatLogThread::startThread() {
    doRun = true;
    pthread_attr_t loggingAttr;
    pthread_attr_init(&loggingAttr);
    pthread_attr_setdetachstate(&loggingAttr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&logThread, &loggingAttr, &ChatLogThread::runThread, this);
}

void* ChatLogThread::runThread(void* params) {
    ChatLogThread* instance = (ChatLogThread*) params;
    instance->runner();
    pthread_exit(NULL);
}

void ChatLogThread::stopThread() {
    DLOG(INFO) << "Stopping logging thread.";
    list<LoggerConnection*> list_copy;

    for(auto it = connectionList.begin(); it != connectionList.end(); it++) {
        list_copy.push_back(*it);
    }
    for(auto it = list_copy.begin(); it != list_copy.end(); it++) {
        delete *it;
    }
    connectionList.clear();
    doRun = false;
    ev_async_send(logger_loop, logger_async);
    pthread_join(logThread, 0);
}

void ChatLogThread::runner() {
    DLOG(INFO) << "Starting chat log thread.";

    int listen_socket = createSocket();
    if (!listen_socket) {
        LOG(ERROR) << "Failed to create logging socket.";
        doRun = false;
        pthread_exit(NULL);
    }

    logger_loop = ev_loop_new(EVFLAG_AUTO);

    logger_async = new ev_async;
    ev_async_init(logger_async, ChatLogThread::processQueue);
    logger_async->data = this;
    ev_async_start(logger_loop, logger_async);

    logger_accept_io = new ev_io;
    ev_io_init(logger_accept_io, ChatLogThread::acceptCallback, listen_socket, EV_READ);
    logger_accept_io->data = this;
    ev_io_start(logger_loop, logger_accept_io);
    ev_loop(logger_loop, 0);

    ev_io_stop(logger_loop, logger_accept_io);
    delete logger_accept_io;
    logger_accept_io = 0;

    ev_async_stop(logger_loop, logger_async);
    delete logger_async;
    logger_async = 0;

    close(listen_socket);

    ev_loop_destroy(logger_loop);
    logger_loop = 0;
    DLOG(INFO) << "Exiting chat log thread.";
}

void ChatLogThread::acceptCallback(struct ev_loop* loop, ev_io* w, int events) {
    if (!(events & EV_READ))
        return;

    ChatLogThread* instance = (ChatLogThread*) w->data;
    LOG(INFO) << "Accepting connection for logging thread.";
    struct sockaddr_un remote_addr;
    int socklen = sizeof(remote_addr);
    int newfd = accept(w->fd, (sockaddr*) &remote_addr, (socklen_t*) &socklen);
    if (newfd < 0)
        return;

    sock_nonblock(newfd);
    new LoggerConnection(newfd, loop, instance);
}

int ChatLogThread::createSocket() {
    string path = StartupConfig::getString("log_socket_path");
    struct sockaddr_un address;
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, path.c_str(), path.length());
    address.sun_path[path.length()] = 0;

    int listen_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    sock_nonblock(listen_socket);
    unlink(path.c_str());
    int ret = bind(listen_socket, (sockaddr*) (&address), sizeof(address));
    if (ret < 0) {
        LOG(WARNING) << "Failed to bind to logging socket. Errno: " << errno << " Path: " << path;
        close(listen_socket);
        return 0;
    }
    ret = listen(listen_socket, 128);
    if (ret < 0)
        LOG(WARNING) << "Failed to listen on logging socket.";

    return listen_socket;
}

void ChatLogThread::processEntry(LogEntry* entry) {
    // Ownership of the log entry is taken here. Automatic reference counting will clean it up after this point.
    LogEntryPtr log_entry(entry);
    for (list<LoggerConnection*>::iterator itr = connectionList.begin(); itr != connectionList.end(); itr++) {
        LoggerConnection* con = *itr;
        con->sendEntry(log_entry);
    }
}

void ChatLogThread::processQueue(struct ev_loop* loop, ev_async* w, int revents) {
    DLOG(INFO) << "Process Queue";
    ChatLogThread* instance = (ChatLogThread*)w->data;
    if (!instance->doRun) {
        DLOG(INFO) << "Unlooping from process queue";
        ev_unloop(loop, EVUNLOOP_ONE);
        return;
    }

    LogEntry* entry = instance->getQueueEntry();
    while (entry) {
        instance->processEntry(entry);
        entry = instance->getQueueEntry();
    }
}

LogEntry* ChatLogThread::getQueueEntry() {
    LogEntry* ret = 0;
    MUT_LOCK(requestMutex);
    if (requestQueue.size() > 0) {
        ret = requestQueue.front();
        requestQueue.pop();
    }
    MUT_UNLOCK(requestMutex);
    return ret;
}

bool ChatLogThread::addLogEntry(LogEntry* newEntry) {
    if (!doRun)
        return false;
    DLOG(INFO) << "Adding log entry;";
    struct timespec abs_time;
    clock_gettime(CLOCK_REALTIME, &abs_time);
    abs_time.tv_nsec += LOGGER_MUTEX_TIMEOUT;
    if (MUT_TIMEDLOCK(requestMutex, abs_time)) {
        DLOG(INFO) << "Failed to get lock to add queue entry.";
        return false;
    }
    if (requestQueue.size() >= MAX_PENDING_LOG_ENTRIES) {
        MUT_UNLOCK(requestMutex);
        DLOG(WARNING) << "Dropping log message because queue overflowed.";
        return false;
    }
    requestQueue.push(newEntry);
    MUT_UNLOCK(requestMutex);
    return true;
}

void ChatLogThread::sendWakeup() {
    if (doRun && logger_loop && logger_async)
        ev_async_send(logger_loop, logger_async);
}

