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
static int msqid=-1;
static int segsize, blocksize;
static steque_t Q;
static pthread_mutex_t m=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t shm_available=PTHREAD_COND_INITIALIZER;

int shm_push(shm_item it){
    shm_p p=(shm_p)malloc(sizeof (shm_item));
    *p = it;
    pthread_mutex_lock(&m);
    steque_enqueue(&Q, p);
    pthread_mutex_unlock(&m);
    pthread_cond_signal(&shm_available);
    return 0;
}

shm_item shm_pop(){
    shm_p p;
    shm_item it;
    pthread_mutex_lock(&m);
    while(!steque_size(&Q)){
        pthread_cond_wait(&shm_available, &m);
    }
    p=steque_pop(&Q);
    pthread_mutex_unlock(&m);
    it=*p;
    free(p);
    return it;
}

void request_cache(char *path, char *data_dir, key_t key){
    req_msg msg;
    msg.mtype = 1;
    msg.req.key = key;
    msg.req.segsize=segsize;
//    strcpy(msg.req.path, data_dir);//As we are using cache
    strcpy(msg.req.path, path);
    if (msgsnd(msqid, &msg, sizeof(msg.req), 0) == -1) /* +1 for '\0' */
        perror("msgsnd");
}
void init_handlers(int seg_size){
    steque_init(&Q);
    msqid = getmsqid();
    segsize = seg_size;
    blocksize = data_length(segsize);
}
void stop_handlers(){
    steque_destroy(&Q);
    if(msqid != -1){
        destroy_msg(msqid);
    }
}
ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg){
    //单个线程，仅仅负责读取共享内存并发送信息
    //建立一个client socket，通过它发送文件请求，然后获得文件长度信息，
    //之后一段一段的
    ssize_t read_len, file_len, bytes_transferred, remain;
    ssize_t write_len;
    shm_item it = shm_pop();
    cache_p cblock;

    //Setup lock of shared memory
    cblock = (cache_p) shmat(it.shmid, (void *)0, 0);
    init_cache_block(cblock);
    printf("Got key %d and shmid %d from quene\n", it.key, it.shmid);
    request_cache(path, (char*)arg, it.key);

    pthread_mutex_lock(&cblock->meta.m);
    while(cblock->meta.status == WRITABLE){
        pthread_cond_wait(&cblock->meta.readable, &cblock->meta.m);
    }
    file_len = cblock->meta.filelen;
//    cblock->meta.status = WRITABLE;
    pthread_mutex_unlock(&cblock->meta.m);
//    pthread_cond_signal(&cblock->meta.writable);
    if(file_len < 0){
        printf("File does not exist in cache!\n");
        return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
    }
   gfs_sendheader(ctx, GF_OK, (size_t)file_len);
   bytes_transferred = 0;
   printf("Header sent!\n");

   while(bytes_transferred < file_len){
       pthread_mutex_lock(&cblock->meta.m);
       while(cblock->meta.status == WRITABLE){
           pthread_cond_wait(&cblock->meta.readable, &cblock->meta.m);
       }
       remain = file_len-bytes_transferred;
       read_len = (blocksize <= remain)?blocksize:remain;
       write_len = gfs_send(ctx, cblock->data, (size_t)read_len);
//       printf("Sent: %.40s...\n", cblock->data);
       if (write_len != read_len){
           fprintf(stderr, "handle_with_file write error");
           return SERVER_FAILURE;
       }
       bytes_transferred += write_len;
//       printf("Received %zd from cache, transfered %zu, filelen %zu\n", read_len, bytes_transferred, file_len);
       cblock->meta.status = WRITABLE;
       pthread_mutex_unlock(&cblock->meta.m);
       pthread_cond_signal(&cblock->meta.writable);
   }
   if(shmdt(cblock)<0){
       perror("shmdt");
   }
   else{
       printf("Detached shared memory\n");
   }
   shm_push(it);
   printf(">>> Successfully transfered file %s!\n", path);
   return (ssize_t)bytes_transferred;
}
