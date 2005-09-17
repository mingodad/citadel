CC=gcc
OBJFILES=zlib_interface.o os-support.o io-internal.o md5.o digcalc.o \
	net-support.o conversions.o xmlparse.o netio.o \
	main.o parsedate.o rdf_parsedate.o
CFLAGS=`xml2-config --cflags`
LDFLAGS=`xml2-config --libs`

rss2ctdl: $(OBJFILES)
	$(CC) $(CFLAGS) $(OBJFILES) $(LDFLAGS) -o rss2ctdl

clean: 
	rm -f *.o rss2ctdl
