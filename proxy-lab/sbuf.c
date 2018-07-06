#include "csapp.h"
#include "sbuf.h"


void sbuf_init(sbuf_t *sp,int n){
    sp->buf=Calloc(n,sizeof(int));
    sp->n=n;
    sp->front=sp->rear=0;
    Sem_init(&sp->mutex,0,1);
    Sem_init(&sp->slots,0,n);
    Sem_init(&sp->items,0,0);
}

void sbuf_deinit(sbuf_t *sp){
    Free(sp->buf);
}

void sbuf_insert(sbuf_t *sp,int item){
    P(&sp->slots);
    P(&sp->mutex);
    sp->buf[(++sp->rear)%(sp->n)]=item;
    V(&sp->mutex);
    V(&sp->items);
}

int sbuf_remove(sbuf_t *sp){
    int item;
    P(&sp->items);
    P(&sp->mutex);
    item=sp->buf[(++sp->front)%(sp->n)];
    V(&sp->mutex);
    V(&sp->slots);
    return item;
}