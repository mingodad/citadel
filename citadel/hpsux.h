/* $Id$ */

/* This nice file makes Citadel/UX work with HP/UX's dynamic loader. */
/* It's unusual to put C code in a .h file, but I think it's easier for the
   moment.  */

#ifndef _CITADEL_UX_HPSUX_H
#define _CITADEL_UX_HPSUX_H

/* includes */
#include <errno.h>
#include <dl.h>
#include <string.h>


/* functions */
void *dlopen(const char *, int);
int dlclose(void *);
const char *dlerror(void);
void *dlsym(void *, char *);


/* #defines mapped */

#define RTLD_LAZY	BIND_DEFERRED
#define RTLD_NOW	BIND_IMMEDIATE
#define RTLD_GLOBAL	0	/* This SEEMS to be the default for HP/UX */


/* extern variables */
extern int errno;


/* local variables */
static int dlerrmsg;	/* Last errno received */


/* functions mapped */

/* dlopen() */
void *dlopen(const char *filename, int flag)
{
	shl_t handle;

	handle = shl_load(filename, flag, 0L);
	if (handle == NULL)
		dlerrmsg = errno;
	else
		dlerrmsg = 0;
	return (void *)handle;
}

/* dlclose() */
int dlclose(void *handle)
{
	return shl_unload(handle);
}

/* dlerror() */
/* I think this is as thread safe as it's going to get */
const char *dlerror(void)
{
	int msg;

	msg = dlerrmsg;
	dlerrmsg = 0;
	if (msg)
		return strerror(msg);
	return NULL;
}

/* dlsym() */
void *dlsym(void *handle, char *symbol)
{
	void *symaddr = NULL;	/* Linux man page says 0 is a valid symbol */
	/* address.  I don't understand this, of course, but what do I know? */

	if (shl_findsym((shl_t *)&handle, symbol, TYPE_PROCEDURE, &symaddr) == -1)
		dlerrmsg = errno;
	else {
		dlerrmsg = 0;
	}
	return symaddr;
}

#endif /* _CITADEL_UX_HPSUX_H */
