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

void gfs_abort(gfcontext_t *ctx){
    close(ctx->connfd);
    free(ctx);
}

void *thread(void* arg){//Worker
    gfserver_t *gfh = arg;
    item *it;
    pthread_detach(pthread_self());
    while(1){
        pthread_mutex_lock(&gfh->m);
        while(steque_isempty(&gfh->Q)){
            pthread_cond_wait(&gfh->nonempty, &gfh->m);
        }
        it=steque_pop(&gfh->Q);
        pthread_mutex_unlock(&gfh->m);
        gfh->handler(it->ctx, it->path, gfh->args[0]);
        gfs_abort(it->ctx);
        free(it->path);
        free(it);
    }
}

void spawn_workers(gfserver_t *gfh){
    pthread_t tid;
    for (int i=0;i<gfh->nthreads;i++){
        pthread_create(&tid, NULL, thread, gfh);
    }
}

void gfserver_init(gfserver_t *gfh, int nthreads){
    steque_init(&gfh->Q);
    pthread_mutex_init(&gfh->m, NULL);
    pthread_cond_init(&gfh->nonempty, NULL);
    gfh->nthreads=nthreads;
    gfh->args=(void**)malloc(sizeof (void*)*nthreads);
    gfh->maxnpending=0;
    gfh->port=0;
}

ssize_t add_task(gfserver_t *gfh, gfcontext_t *ctx, char *path){//BOSS
    item *it=(item*)malloc(sizeof (item));
    char *path_t=(char *)malloc(strlen(path)+1);//Path for thread
    strcpy(path_t, path);
    it->ctx=ctx;
    it->path=path_t;
    pthread_mutex_lock(&gfh->m);
    steque_enqueue(&gfh->Q, it);
    pthread_mutex_unlock(&gfh->m);

    pthread_cond_signal(&gfh->nonempty);
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
void gfserver_serve(gfserver_t *gfh){
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
    server_addr.sin_port = htons(gfh->port);
    if (bind(listenfd, (void *)&server_addr, sizeof(server_addr)) <0){
        fprintf(stderr, "%s @ %d: socket error\n", __FILE__, __LINE__);
        return;
    }
    if (listen(listenfd, gfh->maxnpending)<0){
        fprintf(stderr, "%s @ %d: socket error\n", __FILE__, __LINE__);
        return;
    }

    spawn_workers(gfh);
    printf("Workers spawned. Listening localhost:%d...\n", gfh->port);
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
            add_task(gfh, ctx, path);
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

void gfserver_stop(gfserver_t *gfh){
    item* it;
    pthread_mutex_lock(&gfh->m);
    while(steque_size(&gfh->Q)){
        it=steque_pop(&gfh->Q);
        gfs_abort(it->ctx);
        free(it->path);
        free(it);
    }
    pthread_mutex_unlock(&gfh->m);
    free(gfh->args);
}


void gfserver_setopt(gfserver_t *gfh, gfserver_option_t option, ...){
    va_list vl;
    int i;
    va_start(vl, option);
    switch (option) {
    case GFS_PORT:
        gfh->port = va_arg(vl, unsigned);
        break;
    case GFS_MAXNPENDING:
        gfh->maxnpending = va_arg(vl, int);
        break;
    case GFS_WORKER_FUNC:
        gfh->handler = va_arg(vl, handler_t);
        break;
    case GFS_WORKER_ARG:
        i = va_arg(vl, int);
        gfh->args[i] = va_arg(vl, void*);
        break;
    }
    va_end(vl);
}
