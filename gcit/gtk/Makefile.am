INCLUDES = -I$(top_srcdir)/libcli_cit $(GDK_IMLIB_CFLAGS)
Q_PIXMAP_DIR = $(datadir)
AM_CPPFLAGS = -DDATA_DIR=\"$(datadir)\" -DQ_PIXMAP_DIR=\"$(Q_PIXMAP_DIR)\"
AM_CFLAGS = $(XTRA_CFLAGS)

bin_PROGRAMS = gcit

gcit_SOURCES = gcit.c gtk_misc.c gui.c gcit.h gtk_misc.h gui.h

gcit_LDADD = $(GDK_IMLIB_LIBS) ../libcli_cit/libcli_cit.la

data_DATA = gcit.gif gcitrc

EXTRA_DIST = gcit.gif gcitrc
