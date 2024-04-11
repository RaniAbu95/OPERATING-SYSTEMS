CC=g++
FLAGS= -lpthread -std=c++11

TAR=tar
TARFLAGS=cvf
TARNAME=ex5.tar
TARSRCS=whatsappServer.cpp whatsappClient.cpp README Makefile 

all: whatsappServer whatsappClient

whatsappClient: whatsappClient.o 
	$(CC) $(FLAGS) whatsappClient.o -o whatsappClient

whatsappServer: whatsappServer.o 
	$(CC) $(FLAGS) whatsappServer.o -o whatsappServer

#======== OBJECTS ==========

%.o: %.cpp
	$(CC) $(FLAGS) -c $<


#======== MISC =============

tar: $(TARSRCS)
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)

.PHONY: all clean tar remake

clean:
	rm -f *.o whatsappServer whatsappClient

remake: clean all
