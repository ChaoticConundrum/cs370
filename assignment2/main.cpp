#include "zpointer.h"
#include "zthread.h"
#include "zmutex.h"
#include "zlock.h"
#include "zworkqueue.h"
#include "zoptions.h"
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

void runProducer(int num, Share *share){
    int atime = share->arate;
    LOG("Producer " <<  num << " start");
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
//            printf("%d queue %d\n", num, j.id);
            LOG("Queue " << j.id);
            share->queue->addWork(j);

            ZThread::msleep(atime);
        }
    }
//    printf("%d producer done\n", num);
    LOG("Producer " << num << " done");
}

void runConsumer(int num, Share *share){
    int stime = share->srate;
//    printf("%d consumer start\n", num);
    LOG("Consumer " << num << " start");
    bool run = true;
    while(run){
        Job j = share->queue->getWork();
        LOG("Job " << j.id);
//        printf("%d job %d\n", num, j.id);

        if(j.exit){
            share->queue->addWork(j);
            run = false;
        }

        if(run)
            ZThread::msleep(stime);
    }
//    printf("%d consumer done\n", num);
    LOG("Consumer " << num << " done");
}

#define OPT_DBG "debug"
const ZArray<ZOptions::OptDef> optdef = {
    { OPT_DBG,  'd', ZOptions::NONE },
};

int main(int argc, char **argv){
    ZLog::logLevelStdOut(ZLog::INFO, "[%clock%] %pid% N %log%");
    ZLog::logLevelStdErr(ZLog::ERRORS, "\x1b[31m[%clock%] %pid% E %log%\x1b[m");

    ZOptions options(optdef);
    if(!options.parse(argc, argv) || options.getArgs().size() != 5){
        LOG("Usage: assignemnt2 [-d|--debug] <num_producers> <num_consumers> <requests> <arrival_rate> <service_rate>");
        return EXIT_FAILURE;
    }

    int nproducer = options.getArgs()[0].tint();
    int nconsumer = options.getArgs()[1].tint();

    int requests = options.getArgs()[2].tint();
    int arate = options.getArgs()[3].tint();
    int srate = options.getArgs()[4].tint();

    LOG("Producers: " << nproducer << ", Consumers: " << nconsumer);
    LOG("Requests: " << requests);
    LOG("Arrival Rate: " << arate << " / s, Service Rate: " << srate << " / s");

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
    // job allocator for queue
    ZAllocator<typename ZList<Job>::Node> *jalloc = new ZWrapAllocator<typename ZList<Job>::Node>(alloc);

    // actual black magic
    Share *share = salloc->construct(salloc->alloc(), 1);
    share->lock = lalloc->construct(lalloc->alloc(), 1, ZMutex::PSHARE);
    share->queue = qalloc->construct(qalloc->alloc(), 1, jalloc, ZCondition::PSHARE);

    share->total = requests;
    share->arate = 1000 / arate;
    share->srate = 1000 / srate;

    // start producers
    for(int i = 0; i < nproducer; ++i){
        int num = (int)i;
        LOG("Fork producer " << num);
        pid_t pid = fork();
        if(pid == 0){
            runProducer(num, share);

            delete lalloc;
            delete qalloc;
            delete salloc;
            delete jalloc;
            delete alloc;

            return EXIT_SUCCESS;

        } else if(pid == -1){
            ELOG("Fork error " << errno << " " << strerror(errno));
        }
    }
    // start consumers
    for(int i = 0; i < nconsumer; ++i){
        int num = (int)i;
        LOG("Fork consumer " << num);
        pid_t pid = fork();
        if(pid == 0){
            runConsumer(num, share);

            delete lalloc;
            delete qalloc;
            delete salloc;
            delete jalloc;
            delete alloc;

            return EXIT_SUCCESS;

        } else if(pid == -1){
            ELOG("Fork error " << errno << " " << strerror(errno));
        }
    }

    // wait for all child processes
    while(true){
        wait(NULL);
        if(errno == ECHILD)
            break;
        LOG("Child Finished");
    }

    LOG("Done Waiting");

    // lock allocator
    lalloc->destroy(share->lock);
    lalloc->dealloc(share->lock);
    delete lalloc;

    // queue allocator
    qalloc->destroy(share->queue);
    qalloc->dealloc(share->queue);
    delete qalloc;
//    delete jalloc;

    salloc->destroy(share);
    salloc->dealloc(share);
    delete salloc;

    delete alloc;

    return 0;
}
