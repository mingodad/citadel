CFLAGS=`wx-config --cflags`
LFLAGS=`wx-config --libs`

all: daphne

.SUFFIXES: .cpp .o .hpp

.cpp.o:
	c++ $(CFLAGS) -c $<


daphne: main.o citclient.o userlogin.o testwindow.o who.o \
	utils.o express_message.o send_express.o \
	roomtree.o roomview.o tcp_sockets.o
	c++ main.o citclient.o userlogin.o testwindow.o who.o \
	utils.o express_message.o send_express.o \
	roomtree.o roomview.o tcp_sockets.o \
	$(LFLAGS) -o daphne

clean:
	rm -f *.o core daphne
