##
## libCxClient - Citadel/UX Extensible Client API
## Copyright (c) 2000, Flaming Sword Productions
## Copyright (c) 2001, The Citadel/UX Consortium
## All Rights Reserved
##
INCLUDES  = $(all_includes)
SUBDIRS = .
pkgincludedir = $(includedir)

lib_LTLIBRARIES = libCxClient.la
 
libCxClient_la_LDFLAGS = -version-info 0:0:0
libCxClient_la_SOURCES = chat.c debug.c files.c libtransport.c listmgt.c messages.c misc.c rooms.c users.c
 
# headers to be installed
pkginclude_HEADERS = CxClient.h
