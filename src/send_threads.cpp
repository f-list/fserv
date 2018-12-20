
#include "precompiled_headers.hpp"
#include "send_threads.hpp"

#include <atomic>
#include <thread>


typedef struct {
    struct ev_loop* loop;
    unordered_set <ConnectionPtr> activeMembers;
    ev_async* async;
    atomic<bool> run;
} SendThreadState;

void SendThreads::notify(ConnectionPtr &con) {
    if (con->sendQueue < 0 || (con->sendQueue >= threadCount)) {
        // TODO: Error reporting.
        return;
    }

    queues[con->sendQueue].enqueue(con);
    auto state = threads[con->sendQueue];
    ev_async_send(state->loop, state->async);
}

void SendThread::thread(void *statePtr) {
    auto state = static_cast<SendThreadState*>(statePtr);

    ev_async_init(state->async, SendThread::asyncCallback);
    ev_async_start(state->loop, state->async);
    ev_loop(state->loop, 0);
}

void SendThread::asyncCallback(struct ev_loop* loop, ev_async* w, int events) {
    // TODO: Consume whole queue and activate all write events on this loop.
    // Unset writeNotified in connection so that it can be queued in the future.
}

void SendThread::writeCallback(struct ev_loop* loop, ev_io* w, int wevents) {
    ConnectionPtr con(static_cast<ConnectionInstance*> (w->data));

    if (con->closed)
        return;

    //DLOG(INFO) << "Write event.";
    if (wevents & EV_ERROR) {
        // TODO: Push message to queue about this.
//        prepareShutdownConnection(con.get());
//        close(w->fd);
    } else if (wevents & EV_WRITE) {
        MessagePtr* outMessagePtr = con->writeQueue.peek();
        while (outMessagePtr) {
            MessagePtr outMessage = *outMessagePtr;
            int len = outMessage->length() - con->writePosition;
            int sent = send(w->fd, outMessage->buffer() + con->writePosition, len, 0);
            if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                return;
            } else if (sent <= 0) {
                // TODO: Push message to queue about this.
//                prepareShutdownConnection(con.get());
//                close(w->fd);
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

SendThreads::SendThreads(int count) {
}

SendThreads::~SendThreads() {
    // TODO: cleanup threads
}