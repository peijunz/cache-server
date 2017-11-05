#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <sys/signal.h>
#include <printf.h>
#include <curl/curl.h>
#include <sys/msg.h>

#include "gfserver.h"
#include "proxy-student.h"
#include "shm_channel.h"
#include "simplecache.h"

#if !defined(CACHE_FAILURE)
#define CACHE_FAILURE (-1)
#endif // CACHE_FAILURE

#define MAX_CACHE_REQUEST_LEN 8803
static int msqid;
/**
 * @brief _sig_handler Destroy message queue and simple cache
 * @param signo
 */
static void _sig_handler(int signo) {
    if(signo == SIGINT || signo == SIGTERM) {
        /* Unlink IPC mechanisms here*/
        if(msqid != -1) {
            destroy_msg(msqid);
            msqid = -1;
        }
        printf("Cache killed by signal %d\n", signo);
        simplecache_destroy();
        exit(signo);
    }
}

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  simplecached [options]\n"                                                  \
"options:\n"                                                                  \
"  -c [cachedir]       Path to static files (Default: ./)\n"                  \
"  -t [thread_count]   Thread count for work queue (Default: 2, Range: 1-8803)\n"      \
"  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"cachedir",           required_argument,      NULL,           'c'},
    {"nthreads",           required_argument,      NULL,           't'},
    {"help",               no_argument,            NULL,           'h'},
    {"hidden",             no_argument,            NULL,           'i'}, /* server side */
    {NULL,                 0,                      NULL,             0}
};

void Usage() {
    fprintf(stdout, "%s", USAGE);
}

/// Message queue ID
static int msqid = -1;

void* serve_cache(void* arg) {
    req_msg msg;
    cache* cp;
    int fd;
    ssize_t filelen, transfered;
    while(1) {
        /// Wait message queue for cache request
        if(msgrcv(msqid, &msg, sizeof(msg.req), 1, 0) == -1) {
            perror("msgrcv");
            exit(1);
        }
        printf(">>> Cache request %s with shmid %d\n", msg.req.path, msg.req.shmid);

        /// Initialization of file and shared memory
        if((fd = simplecache_get(msg.req.path)) < 0) {
            filelen = -1;
            printf("File does not exist in cache!\n");
        } else {
            filelen = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);
        }
        cp = (cache*) shmat(msg.req.shmid, (void *)0, 0);
        transfered = 0;

        /// Write file length into shared memory block
        pthread_mutex_lock(&cp->m);
        while(cp->status == READABLE) {
            pthread_cond_wait(&cp->writable, &cp->m);
        }
        cp->filelen = filelen;
        if(filelen <= 0) {
            cp->status = READABLE;
        }
        pthread_mutex_unlock(&cp->m);
        if(filelen <= 0) {
            pthread_cond_signal(&cp->readable);
        }

        /// Send cache to proxy
        while(transfered < filelen) {
            pthread_mutex_lock(&cp->m);
            while(cp->status == READABLE) {
                pthread_cond_wait(&cp->writable, &cp->m);
            }
            cp->readlen = pread(fd, cp->data, (size_t)cp->datalen, transfered);
            if(cp->readlen < 0) {
                fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu", cp->readlen, transfered, filelen);
                perror("pread: ");
                return NULL;
            }
            transfered += cp->readlen;
            cp->status = READABLE;
            pthread_mutex_unlock(&cp->m);
            pthread_cond_signal(&cp->readable);
        }
        printf("<<< Successfully sent cached %s!\n", msg.req.path);

        /// Detach shared memorys
        if(shmdt(cp) < 0) {
            perror("shmdt:");
        }
    }
}

/**
 * @brief spawn Spawn all threads
 * @param n Number of children threads
 */
void spawn(int n) {
    pthread_t tid[n];
    for(int i = 0; i < n; i++) {
        pthread_create(&tid[i], NULL, serve_cache, NULL);
    }
    printf("Threads spawned!\n");
    for(int i = 0; i < n; i++)
        pthread_join(tid[i], NULL);
}

int main(int argc, char **argv) {
    int nthreads = 2;
    char *cachedir = "locals.txt";
    char option_char;

    /* disable buffering to stdout */
    setbuf(stdout, NULL);

    while((option_char = getopt_long(argc, argv, "ic:ht:", gLongOptions, NULL)) != -1) {
        switch(option_char) {
            case 'c': //cache directory
                cachedir = optarg;
                break;
            case 'h': // help
                Usage();
                exit(0);
                break;
            case 't': // thread-count
                nthreads = atoi(optarg);
                break;
            case 'i': // server side usage
                break;
            default:
                Usage();
                exit(1);
        }
    }

    if((nthreads > 8803) || (nthreads < 1)) {
        fprintf(stderr, "Invalid number of threads\n");
        exit(__LINE__);
    }

    if(SIG_ERR == signal(SIGINT, _sig_handler)) {
        fprintf(stderr, "Unable to catch SIGINT...exiting.\n");
        exit(CACHE_FAILURE);
    }

    if(SIG_ERR == signal(SIGTERM, _sig_handler)) {
        fprintf(stderr, "Unable to catch SIGTERM...exiting.\n");
        exit(CACHE_FAILURE);
    }

    /* Cache initialization */
    simplecache_init(cachedir);
    msqid = getmsqid();
    spawn(nthreads);

    /* this code probably won't execute */
    simplecache_destroy();
    return 0;
}
