#include "zpointer.h"
#include "zthread.h"
#include "zmutex.h"
#include "zlock.h"
#include "zworkqueue.h"
#include "zpoolallocator.h"
#include "zwrapallocator.h"
using namespace LibChaos;

#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/mman.h>

struct Job {
    int id;
    bool exit;
};

struct Share {
    ZMutex *lock;
    ZWorkQueue<Job> *queue;

    int total;
    int arate;
    int srate;
};

void runProducer(zu64 i, Share *share){
    int atime = share->arate;
    LOG("Producer " << i << " start");
    while(true){
        share->lock->lock();
        if(share->total == 0){
            share->lock->unlock();
            break;
        }

        Job j = { share->total, share->total == 1 };
        LOG("Queue " << j.id);
        share->queue->addWork(j);
        share->total--;
        share->lock->unlock();

        if(share->total == 0)
            break;
        ZThread::msleep(atime);
    }
    LOG("Producer " << i << " done");
}

void runConsumer(zu64 i, Share *share){
    int stime = share->srate;
    LOG("Consumer " << i << " start");
    while(true){
        Job j = share->queue->getWork();
        LOG("Job " << j.id);
        ZThread::msleep(stime);
        if(j.exit)
            break;
    }
    LOG("Consumer " << i << " done");
}

int main(int argc, char **argv){
    ZLog::logLevelStdOut(ZLog::INFO, "[%clock%] %ppid% %pid% N %log%");
    ZLog::logLevelStdErr(ZLog::ERRORS, "\x1b[31m[%clock%] %ppid% %pid% E %log%\x1b[m");

    int nproducer = 4;
    int nconsumer = 2;

    int requests = 100;
    int arate = 10;
    int srate = 10;

    const zu64 psize = 100 * 1024 * 1024;
    void *pool = mmap(NULL, psize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if(pool == MAP_FAILED){
        ELOG("map failed");
        return -1;
    }

    ZMutex *lock = new ZMutex(ZMutex::PSHARE);

    // pretty neat
    ZAllocator<zbyte> *alloc = new ZPoolAllocator<zbyte>(pool, psize);

    // oh god i'm sorry
    ZAllocator<ZWorkQueue<Job>> *qalloc = new ZWrapAllocator<ZWorkQueue<Job>>(alloc);
    ZAllocator<typename ZList<Job>::Node>*jalloc = new ZWrapAllocator<typename ZList<Job>::Node>(alloc);
    ZAllocator<Share> *salloc = new ZWrapAllocator<Share>(alloc);

    // actual black magic
    ZWorkQueue<Job> *queue = qalloc->construct(qalloc->alloc(), 1, jalloc, ZCondition::PSHARE);


    Share *share = salloc->construct(salloc->alloc(), 1);
    share->lock = lock;
    share->queue = queue;
    share->total = requests;
    share->arate = 1000 / arate;
    share->srate = 1000 / srate;

    LOG("Producers: " << nproducer << ", Consumers: " << nconsumer);
    LOG("Arrival Rate: " << share->arate << " ms, Service Rate: " << share->srate << " ms");

    // start producers
    for(int i = 0; i < nproducer; ++i){
        pid_t pid = fork();
        if(pid == 0){
            LOG("Start producer " << i);
            runProducer(i, share);
            return 0;
        } else if(pid == -1){
            ELOG("Fork error " << errno << " " << strerror(errno));
        }
    }
    // start consumers
    for(int i = 0; i < nconsumer; ++i){
        pid_t pid = fork();
        if(pid == 0){
            LOG("Start consumer  " << i);
            runConsumer(i, share);
            return 0;
        } else if(pid == -1){
            ELOG("Fork error " << errno << " " << strerror(errno));
        }
    }

    // wait for all child processes
    while(true){
        wait(NULL);
        if(errno == ECHILD)
            break;
    }

    LOG("Done Waiting");

    salloc->destroy(share);
    salloc->dealloc(share);

    delete lock;

    qalloc->destroy(queue);
    qalloc->dealloc(queue);

    delete jalloc;
    delete qalloc;

    return 0;
}
