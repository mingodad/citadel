/*
 * $Id: ipc_c_tcp.c -1   $
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
#include <libcitadel.h>
#include "citadel.h"
#include "citadel_ipc.h"
#include "citadel_decls.h"
#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif
#include "commands.h"

/* Note that some of these functions may not work with multiple instances. */

static void (*deathHook)(void) = NULL;
int (*error_printf)(char *s, ...) = (int (*)(char *, ...))printf;

void setIPCDeathHook(void (*hook)(void)) {
	deathHook = hook;
}

void setIPCErrorPrintf(int (*func)(char *s, ...)) {
	error_printf = func;
}

void connection_died(CtdlIPC* ipc, int using_ssl) {
	if (deathHook != NULL) {
		deathHook();
	}

	stty_ctdl(SB_RESTORE);
	fprintf(stderr, "\r\n\n\n");
	fprintf(stderr, "Your connection to %s is broken.\n", ipc->ServInfo.humannode);

#ifdef HAVE_OPENSSL
	if (using_ssl) {
		fprintf(stderr, "Last error: %s\n", ERR_reason_error_string(ERR_get_error()));
		SSL_free(ipc->ssl);
		ipc->ssl = NULL;
	} else
#endif
		fprintf(stderr, "Last error: %s\n", strerror(errno));

	fprintf(stderr, "Please re-connect and log in again.\n");
	fflush(stderr);
	fflush(stdout);
	shutdown(ipc->sock, 2);
	ipc->sock = -1;
        exit(1);
}
