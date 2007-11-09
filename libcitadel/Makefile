CC=gcc
CFLAGS=-I. -W -Wall -g -fPIC
#CFLAGS+=-DDEBUG
CPP=cpp
LD=gcc
LDFLAGS=-L. -g
PREFIX=/usr/local
INSTALL=install

OBJS=libcitadel.o

all: libcitadel.a libcitadel.so

clean:
	rm -f ${OBJS} \
		libcitadel.so libcitadel.a .depend

.c.o:
	${CC} ${CFLAGS} -o $@ -c $<

libcitadel.a: ${OBJS}
	ar rcs $@ $?

libcitadel.so: ${OBJS}
	${LD} -rdynamic -shared -o $@ $?

depend .depend:
	for i in *.c;do $(CPP) -M $$i;done > .tmpdepend
	mv .tmpdepend .depend

install: libcitadel.a libcitadel.so
	$(INSTALL) -d $(PREFIX)/include $(PREFIX)/lib
	$(INSTALL) -m 644 libcitadel.h $(PREFIX)/include
	$(INSTALL) -m 644 libcitadel.a libcitadel.so $(PREFIX)/lib

ifeq (.depend,$(wildcard .depend))
include .depend
endif
