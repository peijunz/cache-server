#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <ctype.h>

#include "gf-student.h"
#include "gfserver.h"

/* 
 * Modify this file to implement the interface specified in
 * gfserver.h.
 */
#include <pthread.h>

#define BUFFER_SIZE 8803

typedef struct item{
    char* path;
    gfcontext_t *ctx;
} item;

typedef struct thread_arg_t{
    gfserver_t *gfs;
    int id;
} thread_arg_t;

void gfs_abort(gfcontext_t *ctx){
    close(ctx->connfd);
    free(ctx);
}

void *thread(void* arg){//Worker
    //Unpack arguments
    thread_arg_t *targ = arg;
    gfserver_t *gfs = targ->gfs;
    int id = targ->id;
    free(targ);
    printf("Thread %d started with gfs argument at address %p\n", id, gfs->args[id]);

    //Start tasks
    item *it;
    pthread_detach(pthread_self());
    while(1){
        pthread_mutex_lock(&gfs->m);
        while(steque_isempty(&gfs->Q)){
            pthread_cond_wait(&gfs->nonempty, &gfs->m);
        }
        it=steque_pop(&gfs->Q);
        pthread_mutex_unlock(&gfs->m);
        gfs->handler(it->ctx, it->path, gfs->args[id]);
        gfs_abort(it->ctx);
        free(it->path);
        free(it);
    }
}

void spawn_workers(gfserver_t *gfs){
    pthread_t tid;
    thread_arg_t *targ=NULL;
    for (int i=0;i<gfs->nthreads;i++){
        targ = (thread_arg_t *)malloc(sizeof(thread_arg_t));
        targ->gfs = gfs;
        targ->id = i;
        pthread_create(&tid, NULL, thread, targ);
    }
}

void gfserver_init(gfserver_t *gfs, int nthreads){
    steque_init(&gfs->Q);
    pthread_mutex_init(&gfs->m, NULL);
    pthread_cond_init(&gfs->nonempty, NULL);
    gfs->nthreads=nthreads;
    gfs->args=(void**)malloc(sizeof (void*)*nthreads);
    gfs->maxnpending=0;
    gfs->port=0;
}

ssize_t add_task(gfserver_t *gfs, gfcontext_t *ctx, char *path){//BOSS
    item *it=(item*)malloc(sizeof (item));
    char *path_t=(char *)malloc(strlen(path)+1);//Path for thread
    strcpy(path_t, path);
    it->ctx=ctx;
    it->path=path_t;
    pthread_mutex_lock(&gfs->m);
    steque_enqueue(&gfs->Q, it);
    pthread_mutex_unlock(&gfs->m);

    pthread_cond_signal(&gfs->nonempty);
    return 0;
}

int parse_head(char *head, char *path){
    int length=strlen("GETFILE GET /");
    for(int i=0;i<length;i++){
        head[i] = toupper(head[i]);
    }
    if(strncmp("GETFILE GET /", head, length)==0){
        if(sscanf(&head[length-1], "%s \r\n\r\n", path)!=EOF){
            return 0;
        }
    }
    return -1;
}
void gfserver_serve(gfserver_t *gfs){
    int listenfd, optval=1;
    struct sockaddr_in client_addr, server_addr;
    unsigned int clientlen;
    gfcontext_t *ctx;
    char path[HEADER_SIZE], request[HEADER_SIZE];
    if (((listenfd = socket(AF_INET, SOCK_STREAM, 0))<0)){
        fprintf(stderr, "%s @ %d: socket error\n", __FILE__, __LINE__);
        return;
    }
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int)) < 0){
        fprintf(stderr, "%s @ %d: socket error\n", __FILE__, __LINE__);
        return;
    }
    bzero((char *) &server_addr, sizeof(server_addr));//Clear server addr
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(gfs->port);
    if (bind(listenfd, (void *)&server_addr, sizeof(server_addr)) <0){
        fprintf(stderr, "%s @ %d: socket error\n", __FILE__, __LINE__);
        return;
    }
    if (listen(listenfd, gfs->maxnpending)<0){
        fprintf(stderr, "%s @ %d: socket error\n", __FILE__, __LINE__);
        return;
    }

    spawn_workers(gfs);
    printf("Workers spawned. Listening localhost:%d...\n", gfs->port);
    while (1) {
        clientlen = sizeof(client_addr);
        if((ctx = (gfcontext_t *)malloc(sizeof(gfcontext_t)))==NULL){
            fprintf(stderr, "%s @ %d: Memory Error\n", __FILE__, __LINE__);
            return;
        }
        ctx->connfd = accept(listenfd, (struct sockaddr *)&client_addr, &clientlen);

        if ((readheader(ctx->connfd, request)<=0)||parse_head(request, path)){
            fprintf(stderr, "%s @ %d: Malformed request: %s\n", __FILE__, __LINE__, request);
            gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
            gfs_abort(ctx);
        }
        else{
            printf("Received request %s\n", path);
            add_task(gfs, ctx, path);
        }
    }
}

ssize_t gfs_send(gfcontext_t *ctx, void *data, size_t len){
    return rio_writen(ctx->connfd, data, len);
}

ssize_t gfs_sendheader(gfcontext_t *ctx, gfstatus_t status, size_t file_len){
    char buf[HEADER_SIZE];
    if (status == GF_FILE_NOT_FOUND){
        sprintf(buf, "GETFILE FILE_NOT_FOUND 0\r\n\r\n");
    }
    else if (status == GF_OK){
        sprintf(buf, "GETFILE OK %ld\r\n\r\n", file_len);
    }
    else if (status == GF_INVALID){
        sprintf(buf, "GETFILE INVALID 0\r\n\r\n");
    }
    else{
        sprintf(buf, "GETFILE ERROR 0\r\n\r\n");
        fprintf(stderr, "%s @ %d: ERROR Status Code! %d\n", __FILE__, __LINE__, status);
    }
    return gfs_send(ctx, buf, strlen(buf));
}

void gfserver_stop(gfserver_t *gfs){
    item* it;
    pthread_mutex_lock(&gfs->m);
    while(steque_size(&gfs->Q)){
        it=steque_pop(&gfs->Q);
        gfs_abort(it->ctx);
        free(it->path);
        free(it);
    }
    pthread_mutex_unlock(&gfs->m);
    free(gfs->args);
}


void gfserver_setopt(gfserver_t *gfs, gfserver_option_t option, ...){
    va_list vl;
    int i;
    va_start(vl, option);
    switch (option) {
    case GFS_PORT:
        gfs->port = va_arg(vl, unsigned);
        break;
    case GFS_MAXNPENDING:
        gfs->maxnpending = va_arg(vl, int);
        break;
    case GFS_WORKER_FUNC:
        gfs->handler = va_arg(vl, handler_t);
        break;
    case GFS_WORKER_ARG:
        i = va_arg(vl, int);
        gfs->args[i] = va_arg(vl, void*);
        break;
    }
    va_end(vl);
}
