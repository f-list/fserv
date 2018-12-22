
#include "precompiled_headers.hpp"
#include "send_threads.hpp"
#include "server.hpp"
#include "connection.hpp"

#include <atomic>
#include <thread>
#include "queue/readerwriterqueue.h"

using moodycamel::ReaderWriterQueue;

// This is a hack to avoid passing the state variable around inside other objects where it isn't relevant.
static thread_local SendThreadState* currentState = nullptr;

typedef struct SendThreadState {
    struct ev_loop* loop;
    unordered_set <ConnectionPtr> activeMembers;
    ev_async* async;
    ReaderWriterQueue<ConnectionPtr>* queue;
    ReaderWriterQueue<ConnectionPtr>* closeQueue;
} SendThreadState;

static void addActive(ConnectionInstance* con) {
    currentState->activeMembers.insert(con);
}

static void removeActive(ConnectionInstance* con) {
    currentState->activeMembers.erase(con);
}

static void disconnectConnection(ConnectionInstance* con) {
    con->socketClosed = true;
    if (con->writeEvent) {
        ev_io_stop(currentState->loop, con->writeEvent);
        delete con->writeEvent;
        con->writeEvent = nullptr;
    }
    close(con->fileDescriptor);
    Server::sendCloseWakeup(con);
    removeActive(con);
}


static void writeCallback(struct ev_loop* loop, ev_io* w, int wevents) {
    auto con = static_cast<ConnectionInstance*> (w->data);

    if (con->closed) {
        DLOG(INFO) << "Writing callback on closed con " << con;
        return;
    }

    //DLOG(INFO) << "Write event.";
    if (wevents & EV_ERROR) {
        disconnectConnection(con);
        return;
    } else if (wevents & EV_WRITE) {
        int maxSends = 50;
        MessagePtr* outMessagePtr = con->writeQueue.peek();
        while (outMessagePtr) {
            MessagePtr outMessage = *outMessagePtr;
            int len = outMessage->length() - con->writePosition;
            int sent = send(w->fd, outMessage->buffer() + con->writePosition, len, 0);
            if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                return;
            } else if (sent <= 0) {
                disconnectConnection(con);
                return;
            } else if (sent != len) {
                // We've properly filled the buffer, come back later.
                con->writePosition += sent;
                return;
            } else {
                con->writeQueue.pop();
                con->writePosition = 0;
            }
            if(--maxSends <= 0)
                return; // We've focused on this connection for too long.
            outMessagePtr = con->writeQueue.peek();
        }
        ev_io_stop(loop, w);
    }
}

static void asyncCallback(struct ev_loop* loop, ev_async* w, int events) {
    ConnectionPtr con;

    size_t count = 0;
    while (currentState->queue->try_dequeue(con)) {
        if(con->closed || con->socketClosed)
            continue;
        if (!con->writeEvent) {
            auto event = new ev_io;
            ev_io_init(event, writeCallback, con->fileDescriptor, EV_WRITE);
            event->data = con.get();
            con->writeEvent = event;
        }
        ev_io_start(currentState->loop, con->writeEvent);
        con->writeNotified = false;
        addActive(con.get());
        ++count;
    }

    while (currentState->closeQueue->try_dequeue(con)) {
        disconnectConnection(con.get());
    }
}

void SendThreads::notify(ConnectionInstance* con) {
    auto queueID = con->sendQueueID;
    if (queueID < 0 || (queueID >= threadCount)) {
        DLOG(INFO) << "Invalid write queue picked for write notification. " << queueID;
        return;
    }

    if(!con->writeNotified) {
        con->writeNotified = true;
        auto state = threads[queueID];
        state->queue->emplace(con);
        ev_async_send(state->loop, state->async);
    }
}

void SendThreads::notifyClose(ConnectionInstance* con) {
    auto queueID = con->sendQueueID;
    if (queueID < 0 || queueID >= threadCount) {
        DLOG(INFO) << "Invalid write queue picked for close notification. " << queueID;
        return;
    }

    auto state = threads[queueID];
    state->closeQueue->emplace(con);
    ev_async_send(state->loop, state->async);
}

static void threadRun(SendThreadState* state) {
    __sync_synchronize(); // Prevents "race" with queue pointers.
    currentState = state;
    ev_loop(state->loop, 0);
}

SendThreads::SendThreads(int count) :
        threads(nullptr),
        runningThreads(nullptr),
        threadCount(count),
        queueID(0) {}

void SendThreads::start() {
    threads = new SendThreadState* [threadCount];
    runningThreads = new std::thread* [threadCount];
    for (int i = 0; i < threadCount; i++) {
        auto newState = new SendThreadState;
        newState->queue = new ReaderWriterQueue<ConnectionPtr>(5000);
        newState->closeQueue = new ReaderWriterQueue<ConnectionPtr>(5000);
        newState->loop = ev_loop_new(EVFLAG_AUTO);
        newState->async = new ev_async;
        ev_async_init(newState->async, asyncCallback);
        ev_async_start(newState->loop, newState->async);
        threads[i] = newState;
        __sync_synchronize(); // Prevents race with queue pointers.
        auto newThread = new std::thread(threadRun, newState);
        auto handle = newThread->native_handle();
        pthread_setname_np(handle, "send_thread");
        runningThreads[i] = newThread;
    }
}

SendThreads::~SendThreads() {
    // TODO: cleanup threads
    delete[] threads;
    delete[] runningThreads;
}