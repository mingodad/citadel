all: webserver webcit


clean:
	rm *.o webcit webserver


webserver: webserver.o context_loop.o
	cc webserver.o context_loop.o \
		-lpthread -o webserver

webserver.o: webserver.c webcit.h
	cc -c -D_REENTRANT webserver.c

context_loop.o: context_loop.c webcit.h
	cc -c -D_REENTRANT context_loop.c



webcit: webcit.o auth.o tcp_sockets.o mainmenu.o serv_func.o who.o
	cc webcit.o auth.o tcp_sockets.o mainmenu.o serv_func.o who.o -o webcit

webcit.o: webcit.c webcit.h
	cc -c webcit.c

auth.o: auth.c webcit.h
	cc -c auth.c

tcp_sockets.o: tcp_sockets.c webcit.h
	cc -c tcp_sockets.c

mainmenu.o: mainmenu.c webcit.h
	cc -c mainmenu.c

serv_func.o: serv_func.c webcit.h
	cc -c serv_func.c

who.o: who.c webcit.h
	cc -c who.c
