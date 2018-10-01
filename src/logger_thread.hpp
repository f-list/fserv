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

#ifndef FSERV_LOGGER_THREAD_H
#define FSERV_LOGGER_THREAD_H

#include <string>
#include <queue>
#include <list>
#include <atomic>
#include "fthread.hpp"
#include <ev.h>
#include <boost/intrusive_ptr.hpp>
#include <time.h>

using std::queue;
using std::list;
using std::string;
using boost::intrusive_ptr;

#define LOGGER_MUTEX_TIMEOUT 250000000
#define MAX_PENDING_LOG_ENTRIES 5000

class ChatLogThread;

class LogEntry {
public:
    LogEntry() :
            messageType("invalid"),
            time(0),
            fromCharacterID(-1),
            toCharacterID(-1),
            fromAccountID(-1),
            toAccountID(-1),
            refCount(0) {}

    ~LogEntry() {}

    void toJSON();

    string messageType;
    string messageBody;
    string fromCharacter;
    string toCharacter;
    string toChannel;
    string toChannelTitle;
    unsigned long long time;
    long fromCharacterID;
    long toCharacterID;
    long fromAccountID;
    long toAccountID;
    string jsonCopy;
protected:
    int refCount;

    friend inline void intrusive_ptr_release(LogEntry* p) {
        if (__sync_sub_and_fetch(&p->refCount, 1) <= 0) {
            delete p;
        }
    }

    friend inline void intrusive_ptr_add_ref(LogEntry* p) { __sync_fetch_and_add(&p->refCount, 1); }
};

typedef intrusive_ptr <LogEntry> LogEntryPtr;

class LoggerConnection {
public:
    LoggerConnection(int fd, struct ev_loop* loop, ChatLogThread* instance);

    ~LoggerConnection();

    void sendEntry(LogEntryPtr entry);

private:
    static void writeCallback(struct ev_loop* loop, ev_io* w, int events);

    queue<LogEntryPtr> entryQueue;
    struct ev_loop* loop;
    ev_io* writer_io;
    int fd;
    size_t writeOffset;
    ChatLogThread* logger;
};

class ChatLogThread {
public:
    ChatLogThread() : doRun(false), logger_loop(nullptr), logger_async(nullptr), logger_accept_io(nullptr),
                      logThread() {}

    ~ChatLogThread() {}

    static void* runThread(void* params);

    bool addLogEntry(LogEntry* newEntry);

    void sendWakeup();

    void runner();

    void addConnection(LoggerConnection* connection);

    void removeConnection(LoggerConnection* connection);

    void startThread();

    void stopThread();

private:

    static void processQueue(struct ev_loop* loop, ev_async* w, int events);

    static void acceptCallback(struct ev_loop* loop, ev_io* w, int events);

    int createSocket();

    LogEntry* getQueueEntry();

    void processEntry(LogEntry* entry);

    queue<LogEntry*> requestQueue;
    list<LoggerConnection*> connectionList;
    std::atomic<bool> doRun;
    struct ev_loop* logger_loop;
    ev_async* logger_async;
    ev_io* logger_accept_io;

    pthread_mutex_t requestMutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_t logThread;
};

#endif //FSERV_LOGGER_THREAD_H
