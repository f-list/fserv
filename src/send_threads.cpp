
#include "precompiled_headers.hpp"
#include "send_threads.hpp"
#include "server.hpp"

#include <atomic>
#include <thread>

// This is a hack to avoid passing the state variable around inside other objects where it isn't relevant.
static thread_local SendThreadState* currentState = nullptr;

typedef struct SendThreadState {
    struct ev_loop* loop;
    unordered_set <ConnectionPtr> activeMembers;
    ev_async* async;
    atomic<bool> run;
    ReaderWriterQueue<ConnectionPtr>* queue;
} SendThreadState;


static void writeCallback(struct ev_loop* loop, ev_io* w, int wevents) {
    ConnectionPtr con(static_cast<ConnectionInstance*> (w->data));

    if (con->closed)
        return;

    //DLOG(INFO) << "Write event.";
    if (wevents & EV_ERROR) {
        ev_io_stop(loop, w);
//        Server::sendDisconnectWakeup(con.get());
        close(w->fd);
        currentState->activeMembers.erase(con);
        return;
    } else if (wevents & EV_WRITE) {
        MessagePtr* outMessagePtr = con->writeQueue.peek();
        while (outMessagePtr) {
            MessagePtr outMessage = *outMessagePtr;
            int len = outMessage->length() - con->writePosition;
            int sent = send(w->fd, outMessage->buffer() + con->writePosition, len, 0);
            if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                return;
            } else if (sent <= 0) {
                ev_io_stop(loop, w);
//                Server::sendDisconnectWakeup(con.get());
                close(w->fd);
                currentState->activeMembers.erase(con);
                return;
            } else if (sent != len) {
                // We've properly filled the buffer, come back later.
                con->writePosition += sent;
                return;
            } else {
                con->writeQueue.pop();
                con->writePosition = 0;
            }
            outMessagePtr = con->writeQueue.peek();
        }
        ev_io_stop(loop, w);
    }
}

static void asyncCallback(struct ev_loop* loop, ev_async* w, int events) {
    ConnectionPtr con;

    while (currentState->queue->try_dequeue(con)) {
        if (!con->writeEvent2) {
            auto event = new ev_io;
            ev_io_init(event, writeCallback, con->fileDescriptor, EV_WRITE);
            event->data = con.get();
            con->writeEvent2 = event;
        }
        ev_io_start(currentState->loop, con->writeEvent2);
        con->writeNotified = false;
        currentState->activeMembers.insert(con);
    }
}

void SendThreads::notify(ConnectionPtr &con) {
    if (con->sendQueue < 0 || (con->sendQueue >= threadCount)) {
        // TODO: Error reporting.
        return;
    }

    queues[con->sendQueue]->enqueue(con);
    auto state = threads[con->sendQueue];
    ev_async_send(state->loop, state->async);
}

static void threadRun(SendThreadState* state) {
    currentState = state;
    ev_loop(state->loop, 0);
}

SendThreads::SendThreads(int count) :
        threadCount(count),
        queueID(0) {}

void SendThreads::start() {
    queues.resize(threadCount);
    threads.resize(threadCount);
    for (int i = 0; i < threadCount; i++) {
        auto newState = new SendThreadState;
        newState->run = true;
        newState->queue = new ReaderWriterQueue<ConnectionPtr>(5000);
        newState->loop = ev_loop_new(EVFLAG_AUTO);
        newState->async = new ev_async;
        ev_async_init(newState->async, asyncCallback);
        ev_async_start(newState->loop, newState->async);
        queues[i] = newState->queue;
        threads[i] = newState;
        auto newThread = new std::thread(threadRun, newState);
        auto handle = newThread->native_handle();
        pthread_setname_np(handle, "send_thread");
        runningThreads.push_back(newThread);
    }
}

SendThreads::~SendThreads() {
    // TODO: cleanup threads
}