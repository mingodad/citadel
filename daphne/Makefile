CFLAGS=`wx-config --cflags`
LFLAGS=`wx-config --libs`

daphne: main.o citclient.o userlogin.o testwindow.o who.o \
	utils.o express_message.o send_express.o \
	roomtree.o roomview.o tcp_sockets.o
	c++ main.o citclient.o userlogin.o testwindow.o who.o \
	utils.o express_message.o send_express.o \
	roomtree.o roomview.o tcp_sockets.o \
	$(LFLAGS) -o daphne

main.o: main.cpp includes.hpp
	c++ -c $(CFLAGS) main.cpp

citclient.o: citclient.cpp includes.hpp
	c++ -c $(CFLAGS) citclient.cpp

userlogin.o: userlogin.cpp includes.hpp
	c++ -c $(CFLAGS) userlogin.cpp

testwindow.o: testwindow.cpp includes.hpp
	c++ -c $(CFLAGS) testwindow.cpp

who.o: who.cpp includes.hpp
	c++ -c $(CFLAGS) who.cpp

utils.o: utils.cpp includes.hpp
	c++ -c $(CFLAGS) utils.cpp

express_message.o: express_message.cpp includes.hpp
	c++ -c $(CFLAGS) express_message.cpp

send_express.o: send_express.cpp includes.hpp
	c++ -c $(CFLAGS) send_express.cpp

roomtree.o: roomtree.cpp includes.hpp
	c++ -c $(CFLAGS) roomtree.cpp

roomview.o: roomview.cpp includes.hpp
	c++ -c $(CFLAGS) roomview.cpp

tcp_sockets.o: tcp_sockets.cpp includes.hpp
	c++ -c $(CFLAGS) tcp_sockets.cpp
