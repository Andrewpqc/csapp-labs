#ifndef LRUCACHE_H
#define LRUCACHE_H

/*定义LRU缓存的缓存单元*/
typedef struct cacheEntryS{
    char key;   /* 数据的key */
    char data;  /* 数据的data */
    
    struct cacheEntryS *hashListPrev;   /* 缓存哈希表指针， 指向哈希链表的前一个元素 */
    struct cacheEntryS *hashListNext;   /* 缓存哈希表指针， 指向哈希链表的后一个元素 */
    
    struct cacheEntryS *lruListPrev;    /* 缓存双向链表指针， 指向链表的前一个元素 */
    struct cacheEntryS *lruListNext;    /* 缓存双向链表指针， 指向链表后一个元素 */
}cacheEntryS;


/* 定义LRU缓存 */
typedef struct LRUCacheS{
    int cacheCapacity;  /*缓存的容量*/
    cacheEntryS **hashMap;  /*缓存的哈希表*/
    
    cacheEntryS *lruListHead;   /*缓存的双向链表表头*/
    cacheEntryS *lruListTail;   /*缓存的双向链表表尾*/
    int lruListSize;    /*缓存的双向链表节点个数*/
}LRUCacheS;


/*创建LRU缓存*/
int LRUCacheCreate(int capacity, void **lruCache);
/*销毁LRU缓存*/
int LRUCacheDestory(void *lruCache);
/*将数据放入LRU缓存中*/
int LRUCacheSet(void *lruCache, char key, char data);
/*从缓存中获取数据*/
char LRUCacheGet(void *lruCache, char key);
/*打印缓存中的数据， 按访问时间从新到旧的顺序输出*/
void LRUCachePrint(void *lruCache);


#endif 
