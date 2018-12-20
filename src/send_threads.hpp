

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
        if (queueID >= threadCount)
            queueID = 0;
        return id;
    }

private:
    SendThreadState** threads;
    std::thread** runningThreads;
    int threadCount;
    int queueID;
};