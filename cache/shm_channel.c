//In case you want to implement the shared memory IPC as a library...
#include "shm_channel.h"

int getmsqid(){
    int msg_qid;
    key_t key;
    if ((key = ftok("webproxy.c", 'b')) == -1) {
        perror("ftok");
        exit(1);
    }
    if ((msg_qid = msgget(key, 0644 | IPC_CREAT)) == -1) {
        perror("msgget");
        exit(1);
    }
    return msg_qid;
}

int destroy_msg(int msg_qid){
    if(msgctl(msg_qid, IPC_RMID, NULL)==-1){
        perror("msgctl");
    }
}

//初始化共享内存块
void init_cache_block(cache_p cblock){
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&cblock->meta.m, &mutex_attr);

    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&cblock->meta.readable, &cond_attr);
    pthread_cond_init(&cblock->meta.writable, &cond_attr);
    cblock->meta.status = WRITABLE;
    cblock->meta.filelen=0;
}
ssize_t data_length(int segs){
    return (ssize_t)(segs-sizeof (metadata));
}
