CC = gcc
RM = rm -f
CP = cp -f
CFLAGS = -Wall -I../common
LFLAGS =
OBJDIR = obj
OBJECTS = main.o dbaccess.o
TARGET = server

${TARGET}: ${OBJECTS}
	mkdir ../../bin || true
	${CC} -o ../../bin/${TARGET} ${OBJECTS} ${LFLAGS}
	
main.o: main.c
	${CC} -c main.c ${CFLAGS}
	
dbaccess.o: ../common/dbaccess.c ../common/dbaccess.h
	${CC} -c ../common/dbaccess.c ${CFLAGS}
	
clean:
	${RM} ${OBJECTS} ${TARGET}
