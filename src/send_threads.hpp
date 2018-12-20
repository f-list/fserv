
#include <vector>
#include "connection.hpp"
#include "queue/readerwriterqueue.h"

using std::vector;
using moodycamel::ReaderWriterQueue;

struct SendThreadState;
class ConnectionInstance;

namespace std {
    class thread;
}

class SendThreads {
public:
    SendThreads(int count);
    ~SendThreads();

    void start();

    void notify(ConnectionInstance* con);

    void notifyClose(ConnectionInstance* con);

    const int nextQueue() {
        int id = queueID++;
        if (queueID >= (threadCount - 1))
            queueID = 0;
        return id;
    }

private:
    vector<ReaderWriterQueue<ConnectionPtr>* > queues;
    vector<SendThreadState*> threads;
    vector<std::thread*> runningThreads;
    int threadCount;
    int queueID;
};