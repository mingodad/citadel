/* $Id$ */

#include "sysdep.h"
#include "client_crypto.h"

#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Quick and dirty hack; we don't want to use malloc() in C++ */
#ifdef __cplusplus
#define ialloc(t)	new t()
#define ifree(o)	delete o
#else
#define ialloc(t)	malloc(sizeof(t))
#define ifree(o)	free(o);
#endif

/* This class is responsible for the server connection */
typedef struct _CtdlIPC {
#if defined(HAVE_OPENSSL)
	/* NULL if not encrypted, non-NULL otherwise */
	SSL *ssl;
#endif
#if defined(HAVE_PTHREAD_H)
	/* Fast mutex, call CtdlIPC_lock() or CtdlIPC_unlock() to use */
	pthread_mutex_t mutex;
#endif
	/* -1 if not connected, >= 0 otherwise */
	int sock;
	/* 1 if server is local, 0 otherwise or if not connected */
	int isLocal;
} CtdlIPC;

/* C constructor */
CtdlIPC* CtdlIPC_new(int argc, char **argv, char *hostbuf, char *portbuf);
/* C destructor */
void CtdlIPC_delete(CtdlIPC* ipc);
/* Convenience destructor; also nulls out caller's pointer */
void CtdlIPC_delete_ptr(CtdlIPC** pipc);
/* Read a line from server, discarding newline */
void CtdlIPC_getline(CtdlIPC* ipc, char *buf);
/* Write a line to server, adding newline */
void CtdlIPC_putline(CtdlIPC* ipc, const char *buf);

/* Internals */
int starttls(CtdlIPC *ipc);
void endtls(CtdlIPC *ipc);
void setCryptoStatusHook(void (*hook)(char *s));
void serv_read(CtdlIPC *ipc, char *buf, int bytes);
void serv_write(CtdlIPC *ipc, const char *buf, int nbytes);
#ifdef HAVE_OPENSSL
void serv_read_ssl(CtdlIPC *ipc, char *buf, int bytes);
void serv_write_ssl(CtdlIPC *ipc, const char *buf, int nbytes);
void ssl_lock(int mode, int n, const char *file, int line);
#endif /* HAVE_OPENSSL */
/* This is all Ford's doing.  FIXME: figure out what it's doing */
extern int (*error_printf)(char *s, ...);
void setIPCDeathHook(void (*hook)(void));
void setIPCErrorPrintf(int (*func)(char *s, ...));

#ifdef __cplusplus
}
#endif
