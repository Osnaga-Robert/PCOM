CFLAGS = -Wall -g -Werror -Wno-error=unused-variable -O2
LIBS = -lm

# Portul pe care asculta serverul
PORT = 12345

# Adresa IP a serverului
IP_SERVER = 127.0.0.1
ID = ID1

all: server subscriber

common.o: common.c

# Compileaza server.c
server: server.c common.o
	gcc $(CFLAGS) -o $@ $^ $(LIBS)

# Compileaza subscriber.c
subscriber: subscriber.c common.o
	gcc $(CFLAGS) -o $@ $^ $(LIBS)

.PHONY: clean run_server run_subscriber

# Ruleaza serverul
run_server:
	./server ${PORT}

# Ruleaza subscriberul 	
run_subscriber:
	./subscriber ${ID} ${IP_SERVER} ${PORT}

clean:
	rm -rf server subscriber *.o *.dSYM
