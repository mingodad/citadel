# Citadel client makefile

CC = gcc

# For Gtk+ client

# Gtk+ version of citadel/ux client
CLIENTDIR = gtk
CLIENT = gcit
LIB = libcli_cit.so

all: $(CLIENT) $(LIB)

clean:
	rm -f $(CLIENT) $(LIB) *.o *~
	(cd libcli_cit; make clean ; cd ..)
	(cd $(CLIENTDIR); make clean; cd ..)

install: $(CLIENT) $(LIB)
	(cd libcli_cit; make install; cd ..)
	(cd $(CLIENTDIR); make install; cd ..)

$(CLIENT)::
	(cd $(CLIENTDIR) ; make all; cd ..)

$(LIB)::
	(cd libcli_cit ; make all; cd ..)

