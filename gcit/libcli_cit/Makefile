# Citadel client makefile

CC = gcc
LIB_OBJS = tcp_trans.o citadel_util.o citadel_api.o client_api.o
TARGET = ../libcli_cit.so

all: $(TARGET)

clean:
	rm -f $(TARGET) *.o *~

install: $(TARGET)
	cp $(TARGET) /usr/lib/
	ldconfig

$(TARGET): $(LIB_OBJS)
	$(CC) -shared -o $(TARGET) $(LIB_OBJS)

tcp_trans.o: tcp_trans.c client_api.h transport.h

citadel_util.o: transport.h client_api.h citadel_util.h citadel_util.c

citadel_api.o: transport.h client_api.h citadel_api.h citadel_util.h citadel_api.c

client_api.o: client_api.c client_api.h transport.h citadel_api.h citadel_util.h
