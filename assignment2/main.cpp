#include "zpointer.h"
#include "zthread.h"
#include "zmutex.h"
#include "zlock.h"
#include "zworkqueue.h"
using namespace LibChaos;

struct Job {
    int id;
};

struct Share {
    ZMutex lock;
    int total;
    ZWorkQueue<Job> queue;

    int arate;
    int srate;
};

class Producer : public ZThread::ZThreadContainer {
public:
    void *run(void *arg){
        Share *share = (Share *)arg;
        int atime = share->arate;
        LOG("Producer start");
        while(!stop()){
            share->lock.lock();
            if(share->total == 0){
                share->lock.unlock();
                break;
            }
            Job j = { share->total };
            LOG("Queue " << j.id);
            share->queue.addWork(j);
            share->total--;
            share->lock.unlock();
            ZThread::msleep(atime);
        }
        LOG("Producer done");
        return nullptr;
    }
};

class Consumer : public ZThread::ZThreadContainer {
public:
    void *run(void *arg){
        Share *share = (Share *)arg;
        int stime = share->srate;
        LOG("Consumer start");
        while(!stop()){
            Job j = share->queue.getWork();
            LOG("Job " << j.id);
            ZThread::msleep(stime);
        }
        LOG("Consumer done");
        return nullptr;
    }
};

int main(int argc, char **argv){
    ZLog::logLevelStdOut(ZLog::INFO, "[%clock%] %thread% N %log%");
    ZLog::logLevelStdErr(ZLog::ERRORS, "\x1b[31m[%clock%] %thread% E %log%\x1b[m");

    int nproducer = 4;
    int nconsumer = 2;

    int requests = 100;
    int arate = 10;
    int srate = 10;

    Share share;
    share.total = requests;
    share.arate = 1000 / arate;
    share.srate = 1000 / srate;

    ZArray<ZPointer<ZThread>> produce;
    ZArray<ZPointer<ZThread>> consume;

    LOG("Producers: " << nproducer << ", Consumers: " << nconsumer);
    LOG("Arrival Rate: " << share.arate << " ms, Service Rate: " << share.srate << " ms");

    // start producers
    for(int i = 0; i < nproducer; ++i){
        ZPointer<ZThread> thr = new ZThread(new Producer);
        thr->exec(&share);
        produce.push(thr);
    }
    // start consumers
    for(int i = 0; i < nconsumer; ++i){
        ZPointer<ZThread> thr = new ZThread(new Consumer);
        thr->exec(&share);
        consume.push(thr);
    }

    // wait for producers
    for(zu64 i = 0; i < produce.size(); ++i){
        produce[i]->join();
    }
    // stop consumers
    for(zu64 i = 0; i < consume.size(); ++i){
        consume[i]->stop();
    }
    // wait for consumers
    for(zu64 i = 0; i < consume.size(); ++i){
        consume[i]->join();
    }

    return 0;
}
