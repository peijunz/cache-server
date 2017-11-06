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
#include <sys/shm.h>

#include "gfserver.h"
// #include "proxy-student.h"
#include "shm_channel.h"

#define BUFSIZE (8803)

/// Message queue ID
static int msqid = -1;

/// Segment size of each block
static ssize_t segsize;

/// Queue for shared memory blocks
static steque_t Q;

/// Mutex for Q
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

/// Shared memory block available
static pthread_cond_t shm_available = PTHREAD_COND_INITIALIZER;

/**
 * @brief shm_push Add shared memory block into queue
 * @param shmid id of block to add
 */
void shm_push(int shmid) {
    int* p = (int*)malloc(sizeof(int));
    *p = shmid;
    pthread_mutex_lock(&m);
    steque_enqueue(&Q, p);
    pthread_mutex_unlock(&m);
    pthread_cond_signal(&shm_available);
}

/**
 * @brief shm_pop Get a shared memory block from queue
 * @return shmid id of shared memory block
 */
int shm_pop() {
    int* p;
    int key;
    pthread_mutex_lock(&m);
    while(!steque_size(&Q)) {
        pthread_cond_wait(&shm_available, &m);
    }
    p = steque_pop(&Q);
    pthread_mutex_unlock(&m);
    key = *p;
    free(p);
    return key;
}

/**
 * @brief request_cache Send cache request message to cache server
 * @param path  Path of requested file
 * @param data_dir Unused
 * @param shmid Shared memory id across processes
 */
void request_cache(char *path, char *data_dir, int shmid) {
    req_msg msg;
    msg.mtype = 1;
    msg.req.shmid = shmid;
    msg.req.segsize = segsize;
    strcpy(msg.req.path, path);
    if(msgsnd(msqid, &msg, sizeof(msg.req), 0) == -1) {
        perror("msgsnd");
        exit(1);
    }
}

/**
 * @brief init_cache_handlers Initialization
 * @param seg_size  Segment size for each block
 * @param nsegments Number of segments
 *
 * "shm-file-%d" files are created for shared memory.
 */
void init_cache_handlers(int seg_size, int nsegments) {
    int shmid;
    key_t key;
    char buffer[200];
    msqid = getmsqid();
    segsize = seg_size;
    steque_init(&Q);
    for(int i = 0; i < nsegments; i++) {
        sprintf(buffer, "shm-file-%d", i);
        if(open(buffer, O_CREAT, 0777) == -1) {
            perror("open");
            exit(1);
        }
        if((key = ftok(buffer, 'R')) == -1) {
            perror("ftok");
            exit(1);
        }
        if((shmid = shmget(key, (size_t)segsize, 0644 | IPC_CREAT)) == -1) {
            perror("shmget");
            exit(1);
        }
        shm_push(shmid);
    }
}

/**
 * @brief clean_cache_handlers Cleaning all ipc resources for handlers
 * @todo Clean tmp files
 */
void clean_cache_handlers() {
    int shmid;
    if(msqid != -1) {
        destroy_msg(msqid);
        msqid = -1;
    }
    while(steque_size(&Q)) {
        shmid = shm_pop();
        shmctl(shmid, IPC_RMID, 0);
    }
    steque_destroy(&Q);
}

/**
 * @brief handle_with_cache Handler to serve file
 * @param ctx
 * @param path Path of requested file
 * @param arg Data directory
 * @return Transfer length
 *
 * This is one thread for gfserver. It is responsible to read shared memory
 * written by cache server and then send data stream to client.
 */
ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg) {
    //单个线程，仅仅负责读取共享内存并发送信息
    //建立一个client socket，通过它发送文件请求，然后获得文件长度信息，
    //之后一段一段的
    /// Initialization
    ssize_t filelen, transferred=0, writelen;
    int shmid = shm_pop();
    cache* cp;
    cp = init_cache_block(shmid, segsize);
    request_cache(path, (char*)arg, shmid);
    printf(">>> File request %s with shmid %d\n", path, shmid);

    /// Get file length and send header
    pthread_mutex_lock(&cp->m);
    while(cp->status == WRITABLE) {
        pthread_cond_wait(&cp->readable, &cp->m);
    }
    filelen = cp->filelen;
    pthread_mutex_unlock(&cp->m);
    if(filelen < 0) {
        gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
    } else {
        gfs_sendheader(ctx, GF_OK, (size_t)filelen);
    }

    /// Read cache and transfer data until finished
    while(transferred < filelen) {
        pthread_mutex_lock(&cp->m);
        while(cp->status == WRITABLE) {
            pthread_cond_wait(&cp->readable, &cp->m);
        }
        writelen = gfs_send(ctx, cp->data, (size_t)cp->readlen);
        if(writelen != cp->readlen) {
            fprintf(stderr, "handle_with_file write error");
            return SERVER_FAILURE;
        }
        transferred += writelen;
        cp->status = WRITABLE;
        pthread_mutex_unlock(&cp->m);
        pthread_cond_signal(&cp->writable);
    }
    printf("<<< Successfully transfered file %s!\n", path);

    ///Cleaning
    if(shmdt(cp) < 0) {
        perror("shmdt:");
    }
    shm_push(shmid);
    return (ssize_t)transferred;
}
