#include "zpointer.h"
#include "zthread.h"
#include "zmutex.h"
#include "zlock.h"
#include "zworkqueue.h"
#include "zoptions.h"
#include "zclock.h"
#include "zrandom.h"
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
    ZClock clock;
};

struct Share {
    zu32 arate;
    zu32 srate;

    ZWorkQueue<Job> *queue;

    // producer mutex and fields
    ZMutex *prlock;
    zu64 total;

    // consumer mutex and fields
    ZMutex *cslock;
    double time;
    zu64 weight;
};

void runProducer(int num, Share *share){
    zu32 atime = share->arate;
    LOG("Producer " <<  num << " start");

    ZRandom random;
    ZClock clock;

    zu64 count = 0;
    bool run = true;
    while(run){
        share->prlock->lock();

        // check remaining jobs
        if(share->total)
            share->total--;
        else
            run = false;

        int id = share->total;

        share->prlock->unlock();

        if(run){
            zu32 rtime = random.genzu(0, 2 * atime);
            DLOG("Queue " << id << ": " << rtime);
            // delay random time before adding each job
            ZThread::usleep(rtime);
            share->queue->addWork({ id, false, ZClock() });
            ++count;

            if(!id){
                // After the last job, add the exit job
                DLOG("Queue Exit");
                share->queue->addWork({ 0, true, ZClock() });
            }
        }
    }
    LOG("Producer " << num << " done: " << count << " jobs, " << clock.getSecs() << " seconds");
}

void runConsumer(int num, Share *share){
    zu32 stime = share->srate;
    LOG("Consumer " << num << " start");

    ZRandom random;
    ZClock clock;

    zu64 count = 0;
    bool run = true;
    while(run){
        Job j = share->queue->getWork();

        if(j.exit){
            DLOG("Exit Job");
            // Duplicate the exit job for other consumers
            share->queue->addWork(j);
            run = false;
        } else {
            zu32 rtime = random.genzu(0, 2 * stime);
            // do the job's "work"
            ZThread::usleep(rtime);
            ++count;
            j.clock.stop();
            DLOG("Job " << j.id << ": " << rtime << ", " << j.clock.str());

            double sec = j.clock.getSecs();

            share->cslock->lock();

            // update average job time
            share->time += sec;
//            share->time = ((share->time * share->weight) + sec) / (share->weight + 1);
            share->weight += 1;

            share->cslock->unlock();
        }
    }
    LOG("Consumer " << num << " done: " << count << " jobs, " << clock.getSecs() << " seconds");
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

    unsigned nproducer = options.getArgs()[0].toUint();
    unsigned nconsumer = options.getArgs()[1].toUint();

    zu64 requests = options.getArgs()[2].toUint();

    float arate = options.getArgs()[3].toFloat();
    float srate = options.getArgs()[4].toFloat();

    if(options.getOpts().contains(OPT_DBG)){
        ZLog::logLevelStdOut(ZLog::DEBUG, "[%clock%] %pid% D %log%");
    }

    LOG("Producers: " << nproducer << ", Consumers: " << nconsumer);
    LOG("Requests: " << requests);
    LOG("Arrival Rate: " << arate << " requests/second, Service Rate: " << srate << " requests/second");

    /* Allocate shared memory. This is cheap, and could be much larger than the needed size.
     * Pages in anonymous memory mappings are "initialized" to zero, but are not allocated until
     * the page is written.
     * Size is calculated from the expected allocations, then doubled to be safe.
     */
    const zu64 psize = (
                sizeof(Share) + 16 +
                sizeof(ZMutex) + 16 +
                sizeof(ZWorkQueue<Job>) + 16 +
                ((sizeof(ZList<Job>::Node) + 16) * requests) +
                16
                ) * 2;
    LOG("Allocate " << psize << " bytes shared memory");
    void *pool = mmap(NULL, psize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if(pool == MAP_FAILED){
        ELOG("map failed");
        return -1;
    }

    /* Pool allocator over shared memory pool. Allocation metadata is stored in the pool, so
     * this allocator structure can be safely copied by a fork() after this point.
     */
    ZAllocator<zbyte> *alloc = new ZPoolAllocator<zbyte>(pool, psize);

    /* Wrapper allocators use the pool byte allocator to allocate differently sized types.
     * Slightly better than multiple pool allocators on the same pool.
     */
    ZAllocator<Share> *salloc = new ZWrapAllocator<Share>(alloc);
    ZAllocator<ZMutex> *lalloc = new ZWrapAllocator<ZMutex>(alloc);
    ZAllocator<ZWorkQueue<Job>> *qalloc = new ZWrapAllocator<ZWorkQueue<Job>>(alloc);
    // job allocator for queue
    ZAllocator<typename ZList<Job>::Node> *jalloc = new ZWrapAllocator<typename ZList<Job>::Node>(alloc);

    // Allocate shared data structures on the shared memory pool
    Share *share = salloc->construct(salloc->alloc(), 1);
    share->queue = qalloc->construct(qalloc->alloc(), 1, jalloc, ZCondition::PSHARE);
    share->prlock = lalloc->construct(lalloc->alloc(), 1, ZMutex::PSHARE);
    share->cslock = lalloc->construct(lalloc->alloc(), 1, ZMutex::PSHARE);

    share->arate = (zu32)(1000000.0f / arate);
    share->srate = (zu32)(1000000.0f / srate);
    share->total = (zu64)requests;
    share->time = 0;
    share->weight = 0;

    ZClock clock;

    // start producers
    for(unsigned i = 0; i < nproducer; ++i){
        int num = (int)i;
        DLOG("Fork producer " << num);
        pid_t pid = fork();
        if(pid == 0){
            // In producer child process
            runProducer(num, share);

            // Allocators are copied, delete the child process copies
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
    for(unsigned i = 0; i < nconsumer; ++i){
        int num = (int)i;
        DLOG("Fork consumer " << num);
        pid_t pid = fork();
        if(pid == 0){
            // In consumer child process
            runConsumer(num, share);

            // Allocators are copied, delete the child process copies
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
    }

    LOG("Workers Finished: " << clock.getSecs() << " seconds");
    LOG("Total Request Time: " << share->time << " sec");
    LOG("Average Request Latency: " << share->time / share->weight << " sec");

    // lock allocator
    lalloc->destroy(share->cslock);
    lalloc->dealloc(share->cslock);
    lalloc->destroy(share->prlock);
    lalloc->dealloc(share->prlock);
    delete lalloc;

    // queue allocator
    qalloc->destroy(share->queue);
    qalloc->dealloc(share->queue);
    delete qalloc;

    salloc->destroy(share);
    salloc->dealloc(share);
    delete salloc;

    delete alloc;

    // optional, unmap shared pool
    munmap(pool, psize);

    return 0;
}
