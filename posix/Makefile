#Makefile para armar las partes del proyecto

RM = rm -f

error:
	@echo "Uso: make [ server | client | db-registration ]"
	@exit 1

server:
	make -C src/server

client:
	make -C src/client

db-registration:
	make -C src/db-registration

clean:
	$(RM) bin/*fifo*
	$(RM) bin/*semaphore*
	$(RM) bin/server bin/register bin/client
	make -C src/server clean
	make -C src/client clean
	make -C src/db-registration clean

all: server client db-registration
