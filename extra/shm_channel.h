//In case you want to implement the shared memory IPC as a library...
#ifndef __SHM_CHANNEL_H__
#define __SHM_CHANNEL_H__

#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <pthread.h>

///Message to request cache
typedef struct {
    long mtype;
    struct {            ///< Request Information
        ssize_t segsize;///< Shared memory segment size
        int shmid;      ///< Shared memory id
        char path[200]; ///< Path for requested file
    } req;
} req_msg;

#define WRITABLE 0
#define READABLE 1

typedef struct {
    pthread_mutex_t  m;
    pthread_cond_t  writable;
    pthread_cond_t  readable;
    ssize_t datalen;///< Data area length
    ssize_t filelen;///< File length
    ssize_t readlen;///< Data length to read this time(from data head)
    int status;     ///< Readable or writable
    char data[];    ///< Flexible array member used to transfer data
} cache;

/**
 * @brief getmsqid Get message quene id
 * @return Message quene id
 */
int getmsqid(void);

/**
 * @brief init_cache_block Initialize shared memory block
 * @param shmid Shared memory id
 * It mappes menory and then sets up mutex and conditional variables for
 * syncronization. Also initialize cacheblock metadata.
 */
cache* init_cache_block(int shmid, ssize_t segsize);

/**
 * @brief destroy_msg Destroy message quene
 * @param msqid Message quene id
 */
void destroy_msg(int msg_qid);

#endif // __SHM_CHANNEL_H__
