CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -O0
BUILD_DIR = build

.PHONY: all clean

all:
	make client
	make server

client: client.c
	mkdir -p ${BUILD_DIR}
	${CC} ${CFLAGS} $< -lpthread -o ${BUILD_DIR}/$@

server: server.c
	mkdir -p ${BUILD_DIR}
	${CC} ${CFLAGS} $< -lpthread -o ${BUILD_DIR}/$@

# Run by hand

clean:
	rm -rf ${BUILD_DIR}