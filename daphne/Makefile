CFLAGS=`wx-config --cflags`
LFLAGS=`wx-config --libs`

all: daphne

.SUFFIXES: .cpp .o .hpp

.cpp.o:
	c++ $(CFLAGS) -c $<


daphne: main.o citclient.o userlogin.o testwindow.o who.o \
	utils.o express_message.o send_express.o prefs.o \
	roomtree.o roomview.o message.o enter.o \
	selectuser.o servprops.o 
	c++ main.o citclient.o userlogin.o testwindow.o who.o \
	utils.o express_message.o send_express.o prefs.o \
	roomtree.o roomview.o message.o enter.o \
	selectuser.o servprops.o\
	$(LFLAGS) -o daphne

install: 
	
	cp daphne /usr/local/bin/./
	mkdir /usr/local/share/bitmaps	
	cp bitmaps/*.xpm /usr/local/share/bitmaps/./
	cp bitmaps/*.bmp /usr/local/share/bitmaps/./
clean:
	rm -f *.o core daphne
