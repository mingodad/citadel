CFLAGS=`wx-config --cflags`
LFLAGS=`wx-config --libs`

daphne: main.o citclient.o userlogin.o
	c++ main.o citclient.o userlogin.o $(LFLAGS) -o daphne

main.o: main.cpp
	c++ -c $(CFLAGS) main.cpp

citclient.o: citclient.cpp
	c++ -c $(CFLAGS) citclient.cpp

userlogin.o: userlogin.cpp
	c++ -c $(CFLAGS) userlogin.cpp
