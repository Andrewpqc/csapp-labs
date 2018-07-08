# Makefile for Proxy Lab 
#
# You may modify this file any way you like (except for the handin
# rule). You instructor will type "make" on your specific Makefile to
# build your proxy from sources.

CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lpthread

all: proxy

sbuf.o: ./sbuf/sbuf.c ./sbuf/sbuf.h
	$(CC) $(CFLAGS) -c ./sbuf/sbuf.c $(LDFLAGS)

lru_cache_impl.o: ./lrucache/lru_cache_impl.c ./lrucache/lru_cache_impl.h ./lrucache/lru_cache.h 
	$(CC) $(CFLAGS) -c ./lrucache/lru_cache_impl.c ./lrucache/lru_cache_impl.h ./lrucache/lru_cache.h 

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c csapp.h

proxy.o: proxy.c csapp.h 
	$(CC) $(CFLAGS) -c proxy.c

proxy: proxy.o csapp.o sbuf.o lru_cache_impl.o
	$(CC) $(CFLAGS) proxy.o csapp.o sbuf.o lru_cache_impl.o -o proxy $(LDFLAGS)

# Creates a tarball in ../proxylab-handin.tar that you can then
# hand in. DO NOT MODIFY THIS!
handin:
	(make clean; cd ..; tar cvf $(USER)-proxylab-handin.tar proxylab-handout --exclude tiny --exclude nop-server.py --exclude proxy --exclude driver.sh --exclude port-for-user.pl --exclude free-port.sh --exclude ".*")

clean:
	rm -f *~ *.o proxy core *.tar *.zip *.gzip *.bzip *.gz *.gch ./lrucache/*.gch

