//In case you want to implement the shared memory IPC as a library...
#ifndef __SHM_CHANNEL_H__
#define __SHM_CHANNEL_H__

#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <pthread.h>

typedef struct{
    long mtype;
    struct {
        key_t key;
        int segsize;
        char path[200];
    } req;
} req_msg;

typedef struct{
    int shmid;
    key_t key;
} shm_item, *shm_p;

#define WRITABLE 0
#define READABLE 1
typedef struct{
    pthread_mutex_t  m;
    pthread_cond_t  writable;
    pthread_cond_t  readable;
    ssize_t filelen;
    size_t readlen;
    int status;
}metadata;

typedef struct{
    metadata meta;
    char data[1024];
} cache_block, *cache_p;

int getmsqid(void);
void init_cache_block(cache_p cblock);
int destroy_msg(int msg_qid);
ssize_t data_length(int segs);
#endif // __SHM_CHANNEL_H__
