CFLAGS=`wx-config --cflags`
LFLAGS=`wx-config --libs`

daphne: main.o citclient.o userlogin.o testwindow.o
	c++ main.o citclient.o userlogin.o testwindow.o $(LFLAGS) -o daphne

main.o: main.cpp
	c++ -c $(CFLAGS) main.cpp

citclient.o: citclient.cpp
	c++ -c $(CFLAGS) citclient.cpp

userlogin.o: userlogin.cpp
	c++ -c $(CFLAGS) userlogin.cpp

testwindow.o: testwindow.cpp
	c++ -c $(CFLAGS) testwindow.cpp
