/*
 * $Id$
 * 
 * Client-side IPC functions
 *
 */


#include "sysdep.h"
#undef NDEBUG
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include "citadel.h"
#include "citadel_ipc.h"
#include "citadel_decls.h"
#include "tools.h"
#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

/*
 * FIXME: rewrite all of Ford's stuff here, it won't work with multiple
 * instances
 */

static void (*deathHook)(void) = NULL;
int (*error_printf)(char *s, ...) = (int (*)(char *, ...))printf;

void setIPCDeathHook(void (*hook)(void)) {
	deathHook = hook;
}

void setIPCErrorPrintf(int (*func)(char *s, ...)) {
	error_printf = func;
}

void connection_died(CtdlIPC *ipc) {
	if (deathHook != NULL)
		deathHook();

	error_printf("\rYour connection to this Citadel server is broken.\n"
			"Last error: %s\n"
			"Please re-connect and log in again.\n",
			strerror(errno));
#ifdef HAVE_OPENSSL
	SSL_shutdown(ipc->ssl);
	SSL_free(ipc->ssl);
	ipc->ssl = NULL;
#endif
	shutdown(ipc->sock, 2);
	ipc->sock = -1;
}


/*
static void ipc_timeout(int signum)
{
	error_printf("\rConnection timed out.\n");
	logoff(NULL, 3);
}
*/
