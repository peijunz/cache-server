/*
 *  This file is for use by students to define anything they wish.  It is used by both the gf server and client implementations
 */
#ifndef __GF_STUDENT_H__
#define __GF_STUDENT_H__

#include <unistd.h>
#include <errno.h>
#define HEADER_SIZE 200
#define MAXLINE 1024


ssize_t rio_writen(int fd, char* buf, size_t n){
    size_t rem = n;
    ssize_t nwrite=0;
    while (rem>0){
        if ((nwrite = write(fd, buf, rem))<=0){//EINTER
            if (errno == EINTR)
                nwrite = 0;
            else{
                perror("write");
                return -1;
            }
        }
        rem -= (size_t)nwrite;
        buf += nwrite;
    }
    return (ssize_t)n;
}

ssize_t rio_readn(int fd, char* buf, size_t n){
    size_t rem = n;
    ssize_t nread;
    while (rem>0){
        if((nread=read(fd, buf, rem))<0){
            if (errno == EINTR)
                nread = 0;
            else
                return -1;
        }
        else if (nread ==0 )
            break;
        rem -= (size_t)nread;
        buf += nread;
    }
    return (ssize_t)(n-rem);
}
int readheader(int fd, char* buf){
    char delim[]="\r\n\r\n";
    int next=0;
    for(int i=0;i < HEADER_SIZE-1;i++){
        if(rio_readn(fd, buf+i, 1)<=0) return -2;
        if(buf[i]==delim[next]){
            next++;
            if(next>=4){
                buf[i+1]='\0';
                return i+1;//Header length
            }
        }
        else next=(buf[i]=='r');
    }
    printf("wrong header");
    return -1;
}

#endif // __GF_STUDENT_H__
