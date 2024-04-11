    all : CacheFS.o
	@ar rcs CacheFS.a CacheFS.o
	@ranlib CacheFS.a
	
CC = g++

CFLAGS = -std=c++11 -Wall -lboost_system -lboost_filesystem -g -D_FILE_OFFSET_BITS=64

CacheFS.o : CacheFS.cpp
	$(CC) $(CFLAGS) -c -I. CacheFS.cpp CacheFS.h 
	
tar : 
	tar cvf ex4.tar Makefile CacheFS.cpp Answers.pdf README
	
clean : 
	rm -rf *.o main tar *.a *.tar
