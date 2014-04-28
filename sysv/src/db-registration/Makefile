CC = gcc
RM = rm -f
CP = cp -f
CFLAGS = -Wall -I../common
LFLAGS =
OBJDIR = obj
OBJECTS = main.o dbaccess.o file.o
TARGET = register

${TARGET}: ${OBJECTS}
	mkdir ../../bin -p
	${CC} -o ../../bin/${TARGET} ${OBJECTS} ${LFLAGS}

main.o: main.c
	${CC} -c main.c ${CFLAGS}

file.o: ../common/file.c ../common/file.h
	${CC} -c ../common/file.c ${CFLAGS}

dbaccess.o: ../common/dbaccess.c ../common/dbaccess.h
	${CC} -c ../common/dbaccess.c ${CFLAGS}

protocol.o: ../common/protocol.c ../common/protocol.h
	${CC} -c ../common/protocol.c ${CFLAGS}

clean:
	${RM} ${OBJECTS} ${TARGET}
