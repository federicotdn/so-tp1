CC = gcc
RM = rm -f
CP = cp -f
CFLAGS = -Wall -I../common -g
LFLAGS = -lpthread -lncurses -lrt
OBJDIR = obj
OBJECTS = main.o dbaccess.o client-local.o protocol.o file.o
TARGET = client

${TARGET}: ${OBJECTS}
	mkdir ../../bin -p
	${CC} -o ../../bin/${TARGET} ${OBJECTS} ${LFLAGS}

main.o: main.c
	${CC} -c main.c ${CFLAGS}

client-local.o: client-local.c client-local.h
	${CC} -c client-local.c ${CFLAGS}

dbaccess.o: ../common/dbaccess.c ../common/dbaccess.h
	${CC} -c ../common/dbaccess.c ${CFLAGS}

protocol.o: ../common/protocol.c ../common/protocol.h
	${CC} -c ../common/protocol.c ${CFLAGS}

file.o: ../common/file.c ../common/file.h
	${CC} -c ../common/file.c ${CFLAGS}

clean:
	${RM} ${OBJECTS} ${TARGET}
