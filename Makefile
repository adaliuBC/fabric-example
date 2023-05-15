.PHONY: all clean

CFLAGS  := -Wall -g -ggdb
LDLIBS  := ${LDLIBS} -lrdmacm -libverbs -lpthread -lfabric

APPS    := server client

all: server client

common.o:
	gcc -c common.c common.h

server: common.o
	gcc -o server common.o server.c ${CFLAGS} ${LDLIBS} 

client: common.o
	gcc -o client common.o client.c ${CFLAGS} ${LDLIBS}

clean:
	rm -rf server client common.o