WXHTML=/usr/local/wxHTML

CFLAGS=`wx-config --cflags` -I$(WXHTML)/include
LFLAGS=`wx-config --libs` -L$(WXHTML)/lib -lwxhtml

all: daphne

.SUFFIXES: .cpp .o .hpp

.cpp.o:
	c++ $(CFLAGS) -c $<


daphne: main.o citclient.o userlogin.o testwindow.o who.o \
	utils.o express_message.o send_express.o prefs.o \
	roomtree.o roomview.o tcp_sockets.o message.o enter.o \
	selectuser.o servprops.o
	c++ main.o citclient.o userlogin.o testwindow.o who.o \
	utils.o express_message.o send_express.o prefs.o \
	roomtree.o roomview.o tcp_sockets.o message.o enter.o \
	selectuser.o servprops.o \
	$(LFLAGS) -o daphne

clean:
	rm -f *.o core daphne
