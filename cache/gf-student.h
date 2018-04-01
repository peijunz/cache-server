/*
 *  This file is for use by students to define anything they wish.  It is used by both the gf server and client implementations
 */
#ifndef __GF_STUDENT_H__
#define __GF_STUDENT_H__

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#define HEADER_SIZE 200
#define MAXLINE 1024


ssize_t rio_writen(int fd, char* buf, size_t n);

ssize_t rio_readn(int fd, char* buf, size_t n);

int readheader(int fd, char* buf);

#endif // __GF_STUDENT_H__
