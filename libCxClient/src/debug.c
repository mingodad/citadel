/**
 ** libCxClient - Citadel/UX Extensible Client API
 ** Copyright (c) 2000, Flaming Sword Productions
 ** Copyright (c) 2001, The Citadel/UX Consortium
 ** All Rights Reserved
 **
 ** Module: debug.o
 ** Date: 2000-10-15
 ** Last Revision: 2000-10-15
 ** Description: Debug functions.
 ** CVS: $Id$
 **/
#include	<stdio.h>
#include	<stdlib.h>
#include	<stdarg.h>
#include	<CxClient.h>
#include	"autoconf.h"

#ifdef DEBUG
#warning	"Debugging Mode Enabled.  This may not be practical for you..."

/**
 ** CxDebug(): Output debugging information.
 **
 ** [Expects]
 **  (#define) DFA: __FILE__, __LINE__, __FUNCTION__.
 **  (char *) fmt: printf()-style format string.
 **  ...: Arguments to printf() format.
 **/
void		CxDebug(
			const char *file, 
			int line, 
			const char *function, 
			char *fmt, 
			...) {
va_list		ap;

	va_start(ap,fmt);
	fprintf(stderr,"%% [%s:%d] %s(): ", file, line, function);
	vfprintf(stderr,fmt,ap);
	fprintf(stderr,"\n");
	va_end(ap);

}

#else


#endif

/**
 ** CxMalloc(): Allocate memory.  Annotate allocation in debug log.
 **/
void		*CxMalloc(int szlen) {
void		*ret;

	ret = malloc(szlen);
	if(ret) {
		DPF((DFA,"MEM/ALC:\t%d\t@0x%08x",szlen, ret));
	}

	return(ret);
}

/**
 ** CxFree(): Free memory.  Annotate deallocation in debug log.
 **/
void		*CxFree(void *obj) {
	DPF((DFA,"MEM/FRE:\t-1\t@0x%08x",obj));
	free(obj);
}

