//线程安全的缓存对列包



#include "csapp.h"
#include <stdbool.h>

#ifndef SBUF_H
#define SBUF_H

typedef struct{
    int *buf;
    int n;
    int front;
    int rear;
    sem_t mutex;
    sem_t slots;
    sem_t items;
} sbuf_t;

void sbuf_init(sbuf_t *sp,int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp,int item);
int sbuf_remove(sbuf_t *sp);

#endif SBUF_H


