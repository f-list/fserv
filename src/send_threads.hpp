
#include <vector>
#include "connection.hpp"
#include "queue/readerwriterqueue.h"

using std::vector;
using moodycamel::ReaderWriterQueue;

struct SendThreadState;

class SendThreads {
public:
    SendThreads(int count);
    ~SendThreads();

    ~SendThreads();

    void notify(ConnectionPtr &con);

    const int nextQueue() {
        int id = queueID++;
        if (queueID >= (threadCount - 1))
            queueID = 0;
        return id;
    }

private:
    vector<ReaderWriterQueue> queues;
    vector<SendThreadState*> threads;
    int threadCount;
    int queueID;
};