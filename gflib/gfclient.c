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

#include "gf-student.h"
#include "gfclient.h"

typedef struct gfcrequest_t {
    void (*headerfunc)(void*, size_t, void *);
    void *headerarg;
    void (*write)(void*, size_t, void *);
    void *fp;
    char* server;
    char* path;
    size_t len;//length of content
    size_t actual;//actual received content
    gfstatus_t status;
    int clientfd;
    unsigned short port;
    char _dummy_padding[6];
} gfcrequest_t;

void gfc_cleanup(gfcrequest_t *gfr) {
    close(gfr->clientfd);
    free(gfr);
}

gfcrequest_t *gfc_create() {
    gfcrequest_t *p = (gfcrequest_t *)malloc(sizeof(gfcrequest_t));
    if(p == NULL) {
        fprintf(stderr, "%s @ %d: Memory ERROR!\n", __FILE__, __LINE__);
        exit(-1);
    }
    p->path = NULL;
    p->port = 0;
    p->server = NULL;
    p->fp = NULL;
    p->write = NULL;
    p->headerfunc = NULL;
    p->headerarg = NULL;
    p->len = 0;
    p->actual = 0;
    p->status = 0;
    return p;
}

size_t gfc_get_bytesreceived(gfcrequest_t *gfr) {
    return gfr->actual;
}

size_t gfc_get_filelen(gfcrequest_t *gfr) {
    return gfr->len;
}

gfstatus_t gfc_get_status(gfcrequest_t *gfr) {
    return gfr->status;
}

void gfc_global_init() {
}

void gfc_global_cleanup() {
}

static char *sstatus[] = {"OK", "FILE_NOT_FOUND", "ERROR", "INVALID"};
static gfstatus_t gfstatus[] = {GF_OK, GF_FILE_NOT_FOUND, GF_ERROR, GF_INVALID};

gfstatus_t getstatus(char *status) {
    for(int i = 0; i < 4; i++) {
        if(strcmp(sstatus[i], status) == 0)
            return gfstatus[i];
    }
    fprintf(stderr, "%s @ %d: Invalid status code %s\n", __FILE__, __LINE__, status);
    return GF_INVALID;
}
size_t recvfile(int clientfd, gfcrequest_t *gfr) {
    ssize_t n;
    size_t rem = gfr->len;
    char buf[MAXLINE];
    while((n = rio_readn(clientfd, buf, (MAXLINE < rem) ? MAXLINE : rem)) > 0) {
        gfr->write(buf, (size_t)n, gfr->fp);
        rem -= (size_t)n;
        if(rem == 0) {
            break;
        }
    }
    gfr->actual = gfr->len - rem;
    return n;
}

int gfc_perform(gfcrequest_t *gfr) {
    struct sockaddr_in server_addr;
    struct hostent * hep;
    char buf[HEADER_SIZE], status[HEADER_SIZE];
    int head_len = 0;
    if(((gfr->clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)) {
        fprintf(stderr, "%s @ %d: socket error\n", __FILE__, __LINE__);
        gfr->status = GF_ERROR;
        return -1;
    }
    if((hep = gethostbyname(gfr->server)) == NULL) {
        fprintf(stderr, "%s @ %d: socket error\n", __FILE__, __LINE__);
        gfr->status = GF_ERROR;
        return -2;
    }
    bzero((char *) &server_addr, sizeof(server_addr));//Clear server addr
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(gfr->port);
    bcopy((char *)(hep->h_addr_list[0]),
          (char *)&server_addr.sin_addr.s_addr, hep->h_length);
    if(connect(gfr->clientfd, (void *)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "%s @ %d: Unable to connect %s:%d\n",
                __FILE__, __LINE__, gfr->server, gfr->port);
        gfr->status = GF_ERROR;
        return -3;
    }
    printf("Connected to server\n");
    sprintf(buf, "GETFILE GET %s \r\n\r\n", gfr->path);
    rio_writen(gfr->clientfd, buf, strlen(buf));
    printf("Send Header: %s", buf);
    if(((head_len = readheader(gfr->clientfd, buf)) <= 0) || \
            strncmp(buf, "GETFILE ", strlen("GETFILE ")) || \
            (sscanf(buf, "GETFILE %s %ld \r\n\r\n", status, &gfr->len) == EOF)) {
        fprintf(stderr, "%s @ %d: Error in header: %s\n", __FILE__, __LINE__, buf);
        gfr->status = GF_INVALID;
        return -1;
    }
    if(gfr->headerfunc != NULL) {
        gfr->headerfunc(buf, (size_t)head_len, gfr->headerarg);//Header Hook
    }
    gfr->status = getstatus(status);
    recvfile(gfr->clientfd, gfr);
    if(gfr->actual < gfr->len) {
        fprintf(stderr, "%s @ %d: Prematurely closed socket!\n", __FILE__, __LINE__);
        return -1;
    }
    printf("File received\n");
    return 0;
}


void gfc_set_headerarg(gfcrequest_t *gfr, void *headerarg) {
    gfr->headerarg = headerarg;
}

void gfc_set_headerfunc(gfcrequest_t *gfr, void (*headerfunc)(void*, size_t, void *)) {
    gfr->headerfunc = headerfunc;
}

void gfc_set_path(gfcrequest_t *gfr, char* path) {
    gfr->path = path;
}

void gfc_set_port(gfcrequest_t *gfr, unsigned short port) {
    if(port < 1025) {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, port);
        exit(1);
    }
    gfr->port = port;
}

void gfc_set_server(gfcrequest_t *gfr, char* server) {
    gfr->server = server;
}

void gfc_set_writearg(gfcrequest_t *gfr, void *writearg) {
    gfr->fp = writearg;
}

void gfc_set_writefunc(gfcrequest_t *gfr, void (*writefunc)(void*, size_t, void *)) {
    gfr->write = writefunc;
}

char* gfc_strstatus(gfstatus_t status) {
    for(int i = 0; i < 4; i++) {
        if(gfstatus[i] == status)
            return sstatus[i];
    }
    return (char *)NULL;
}

