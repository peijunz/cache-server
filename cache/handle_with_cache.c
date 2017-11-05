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
#include "proxy-student.h"
#include "shm_channel.h"

#define BUFSIZE (8803)

//Replace with an implementation of handle_with_cache and any other
//functions you may need.
static int msqid = -1;
static int segsize, blocksize;
static steque_t Q;
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t shm_available = PTHREAD_COND_INITIALIZER;

int shm_push(int key) {
    int* p = (int*)malloc(sizeof(int));
    *p = key;
    pthread_mutex_lock(&m);
    steque_enqueue(&Q, p);
    pthread_mutex_unlock(&m);
    pthread_cond_signal(&shm_available);
    return 0;
}

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

void init_cache_handlers(int seg_size, int nsegments) {
    int shmid;
    key_t key;
    char buffer[200];
    msqid = getmsqid();
    segsize = seg_size;
    blocksize = data_length(segsize);
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
        if((shmid = shmget(key, segsize, 0644 | IPC_CREAT)) == -1) {
            perror("shmget");
            exit(1);
        }
        shm_push(shmid);
    }
}
void stop_cache_handlers() {
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

ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg) {
    //单个线程，仅仅负责读取共享内存并发送信息
    //建立一个client socket，通过它发送文件请求，然后获得文件长度信息，
    //之后一段一段的
    ssize_t read_len, file_len, bytes_transferred, remain;
    ssize_t write_len;
    int shmid = shm_pop();
    cache_p cblock;

    cblock = (cache_p) shmat(shmid, (void *)0, 0);
    init_cache_block(cblock);
    printf(">>> File request %s with shmid %d\n", path, shmid);
    request_cache(path, (char*)arg, shmid);

    pthread_mutex_lock(&cblock->meta.m);
    while(cblock->meta.status == WRITABLE) {
        pthread_cond_wait(&cblock->meta.readable, &cblock->meta.m);
    }
    file_len = cblock->meta.filelen;
    pthread_mutex_unlock(&cblock->meta.m);
    if(file_len < 0) {
        gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
    } else {
        gfs_sendheader(ctx, GF_OK, (size_t)file_len);
    }
    bytes_transferred = 0;

    while(bytes_transferred < file_len) {
        pthread_mutex_lock(&cblock->meta.m);
        while(cblock->meta.status == WRITABLE) {
            pthread_cond_wait(&cblock->meta.readable, &cblock->meta.m);
        }
        remain = file_len - bytes_transferred;
        read_len = (blocksize <= remain) ? blocksize : remain;
        write_len = gfs_send(ctx, cblock->data, (size_t)read_len);
        if(write_len != read_len) {
            fprintf(stderr, "handle_with_file write error");
            return SERVER_FAILURE;
        }
        bytes_transferred += write_len;
        cblock->meta.status = WRITABLE;
        pthread_mutex_unlock(&cblock->meta.m);
        pthread_cond_signal(&cblock->meta.writable);
    }
    if(shmdt(cblock) < 0) {
        perror("shmdt:");
    }
    shm_push(shmid);
    printf("<<< Successfully transfered file %s!\n", path);
    return (ssize_t)bytes_transferred;
}
