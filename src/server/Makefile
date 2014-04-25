CC = gcc
RM = rm -f
CP = cp -f
CFLAGS = -Wall -I../common -g
LFLAGS = -lpthread -lrt
OBJDIR = obj
OBJECTS = main.o dbaccess.o server-local.o protocol.o chatroom-local.o
TARGET = server

${TARGET}: ${OBJECTS}
	mkdir ../../bin -p
	${CC} -o ../../bin/${TARGET} ${OBJECTS} ${LFLAGS}

main.o: main.c
	${CC} -c main.c ${CFLAGS}

server-local.o: server-local.c server-local.h
	${CC} -c server-local.c ${CFLAGS}

dbaccess.o: ../common/dbaccess.c ../common/dbaccess.h
	${CC} -c ../common/dbaccess.c ${CFLAGS}

protocol.o: ../common/protocol.c ../common/protocol.h
	${CC} -c ../common/protocol.c ${CFLAGS}

chatroom-local.o: chatroom-local.c chatroom-local.h
	${CC} -c chatroom-local.c ${CFLAGS}

clean:
	${RM} ${OBJECTS} ${TARGET}
