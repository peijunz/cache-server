//In case you want to implement the shared memory IPC as a library...
#include "shm_channel.h"

int getmsqid(){
    int msqid;
    key_t key;
    if ((key = ftok("webproxy.c", 'b')) == -1) {
        perror("ftok");
        exit(1);
    }
    if ((msqid = msgget(key, 0644 | IPC_CREAT)) == -1) {
        perror("msgget");
        exit(1);
    }
    return msqid;
}

void destroy_msg(int msqid){
    if(msgctl(msqid, IPC_RMID, NULL)==-1){
        perror("msgctl");
    }
}

cache* init_cache_block(int shmid, ssize_t segsize){
    cache* cp = (cache*) shmat(shmid, (void *)0, 0);
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&cp->m, &mutex_attr);

    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&cp->readable, &cond_attr);
    pthread_cond_init(&cp->writable, &cond_attr);
    cp->status = WRITABLE;
    cp->datalen=segsize-(ssize_t)sizeof (cache);
    cp->filelen=0;
    cp->readlen=0;
    return cp;
}
