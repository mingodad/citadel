all: webserver webcit

webserver: webserver.o context_loop.o
	cc webserver.o context_loop.o -lpthread -o webserver

webserver.o: webserver.c
	cc -c webserver.c

context_loop.o: context_loop.c
	cc -c context_loop.c

webcit: webcit.o
	cc webcit.o -o webcit

webcit.o: webcit.c
	cc -c webcit.c
