all: webserver webcit

webserver: webserver.o context_loop.o
	cc webserver.o context_loop.o -lpthread -o webserver

webserver.o: webserver.c webcit.h
	cc -c webserver.c

context_loop.o: context_loop.c webcit.h
	cc -c context_loop.c

webcit: webcit.o
	cc webcit.o -o webcit

webcit.o: webcit.c webcit.h
	cc -c webcit.c
