# Citadel client makefile

CC = gcc
DATA_DIR=/usr/share/gcit
Q_PIXMAP_DIR=\"$(DATA_DIR)\"
DEFS= -DHAS_GDK_IMLIB -DDATA_DIR=\"$(DATA_DIR)\" -DQ_PIXMAP_DIR=$(Q_PIXMAP_DIR)
GDK_IMLIB = -lgif -ljpeg -lpng -lz -ltiff -lgdk_imlib
CFLAGS = -Wall -g `gtk-config --cflags` -I../libcli_cit/ $(DEFS)
LDFLAGS = `gtk-config --libs`  -lcli_cit -L..
# For Gtk+ client

# Gtk+ version of citadel/ux client
TARGET = ../gcit
OBJS = gcit.o gtk_misc.o gui.o

all: $(TARGET)

clean:
	rm -f $(TARGET) *.o *~

install: $(TARGET)
	cp $(TARGET) /usr/bin/
	if [ ! -d $(DATA_DIR) ]; then mkdir $(DATA_DIR) ; fi
	cp gcit.gif $(DATA_DIR)/
	cp gcitrc $(DATA_DIR)/

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJS) $(GDK_IMLIB)

gcit.o: gcit.c ../libcli_cit/client_api.h ../libcli_cit/citadel_util.h gtk_misc.h

gtk_misc.o: gtk_misc.c gtk_misc.h

gui.o: ../libcli_cit/client_api.h ../libcli_cit/citadel_util.h gtk_misc.h gui.h
