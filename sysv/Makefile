#Makefile para armar las partes del proyecto

RM = rm -f

error:
	@echo "Uso: make [ server | client | db-registration ]"
	@exit 1

server:
	make -C ../src/server

client:
	make -C ../src/client

db-registration:
	make -C ../src/db-registration

clean:
	$(RM) *fifo*
	$(RM) *semaphore*
