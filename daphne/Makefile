CFLAGS=`wx-config --cflags`
LFLAGS=`wx-config --libs`

all: daphne

.SUFFIXES: .cpp .o .hpp

.cpp.o:
	c++ $(CFLAGS) -c -lwine $<


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
	
	install -m 755 daphne /usr/local/bin/./
	install bitmaps/*.xpm /usr/local/share/bitmaps
	install bitmaps/*.bmp /usr/local/share/bitmaps
clean:
	rm -f *.o core daphne
