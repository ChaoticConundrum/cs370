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
#include <iostream>

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
    printf("producer %d\n", i);
//    LOG("Producer " << i << " start");
    bool run = true;
    while(run){
        share->lock->lock();

        if(share->total)
            share->total--;
        else
            run = false;

        int id = share->total;

        share->lock->unlock();

        if(run){
            Job j = { id, !id };
//            LOG("Queue " << j.id);
            printf("%d queue %d\n", i, j.id);
            share->queue->addWork(j);

            ZThread::msleep(atime);
        }
    }
//    LOG("Producer " << i << " done");
}

void runConsumer(zu64 i, Share *share){
    int stime = share->srate;
    printf("consumer %d\n", i);
//    LOG("Consumer " << i << " start");
    bool run = true;
    while(run){
        Job j = share->queue->getWork();
//        LOG("Job " << j.id);
        printf("%d job %d\n", i, j.id);

        if(j.exit){
            printf("stop\n");
            share->queue->addWork(j);
            run = false;
        }

        if(run)
            ZThread::msleep(stime);
    }
//    LOG("Consumer " << i << " done");
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

    // pretty neat
    ZAllocator<zbyte> *alloc = new ZPoolAllocator<zbyte>(pool, psize);


    // oh god i'm sorry
    ZAllocator<Share> *salloc = new ZWrapAllocator<Share>(alloc);
    ZAllocator<ZMutex> *lalloc = new ZWrapAllocator<ZMutex>(alloc);
    ZAllocator<ZWorkQueue<Job>> *qalloc = new ZWrapAllocator<ZWorkQueue<Job>>(alloc);
    ZAllocator<typename ZList<Job>::Node>*jalloc = new ZWrapAllocator<typename ZList<Job>::Node>(alloc);

    // actual black magic
    Share *share = salloc->construct(salloc->alloc(), 1);
    share->lock = lalloc->construct(lalloc->alloc(), 1, ZMutex::PSHARE);
    share->queue = qalloc->construct(qalloc->alloc(), 1, jalloc, ZCondition::PSHARE);

    share->total = requests;
    share->arate = 1000 / arate;
    share->srate = 1000 / srate;

    LOG("Producers: " << nproducer << ", Consumers: " << nconsumer);
    LOG("Arrival Rate: " << share->arate << " ms, Service Rate: " << share->srate << " ms");

    // start producers
    for(int i = 0; i < nproducer; ++i){
        LOG("Fork producer " << i);
        pid_t pid = fork();
        if(pid == 0){
            runProducer(i, share);
            return 0;
        } else if(pid == -1){
            ELOG("Fork error " << errno << " " << strerror(errno));
        }
    }
    // start consumers
    for(int i = 0; i < nconsumer; ++i){
        LOG("Fork consumer " << i);
        pid_t pid = fork();
        if(pid == 0){
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

    qalloc->destroy(share->queue);
    qalloc->dealloc(share->queue);

    lalloc->destroy(share->lock);
    lalloc->dealloc(share->lock);

    salloc->destroy(share);
    salloc->dealloc(share);

    delete salloc;
    delete jalloc;
    delete qalloc;
    delete lalloc;

    return 0;
}
