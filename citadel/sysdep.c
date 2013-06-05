/*
 * Citadel "system dependent" stuff.
 *
 * Here's where we (hopefully) have most parts of the Citadel server that
 * would need to be altered to run the server in a non-POSIX environment.
 * 
 * If we ever port to a different platform and either have multiple
 * variants of this file or simply load it up with #ifdefs.
 *
 * Copyright (c) 1987-2011 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <syslog.h>
#include <sys/syslog.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <limits.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/un.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <grp.h>
#define SHOW_ME_VAPPEND_PRINTF
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "database.h"
#include "housekeeping.h"
#include "modules/crypto/serv_crypto.h"	/* Needed for init_ssl, client_write_ssl, client_read_ssl, destruct_ssl */
#include "ecrash.h"
#include "context.h"

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

#include "ctdl_module.h"
#include "threads.h"
#include "user_ops.h"
#include "control.h"


/*
 * Signal handler to shut down the server.
 */

volatile int exit_signal = 0;
volatile int shutdown_and_halt = 0;
volatile int restart_server = 0;
volatile int running_as_daemon = 0;

static RETSIGTYPE signal_cleanup(int signum) {
	syslog(LOG_DEBUG, "Caught signal %d; shutting down.", signum);
	exit_signal = signum;
	server_shutting_down = 1;
}

static RETSIGTYPE signal_exit(int signum) {
	exit(1);
}



/*
 * Some initialization stuff...
 */
void init_sysdep(void) {
	sigset_t set;

	/* Avoid vulnerabilities related to FD_SETSIZE if we can. */
#ifdef FD_SETSIZE
#ifdef RLIMIT_NOFILE
	struct rlimit rl;
	getrlimit(RLIMIT_NOFILE, &rl);
	rl.rlim_cur = FD_SETSIZE;
	rl.rlim_max = FD_SETSIZE;
	setrlimit(RLIMIT_NOFILE, &rl);
#endif
#endif

	/* If we've got OpenSSL, we're going to use it. */
#ifdef HAVE_OPENSSL
	init_ssl();
#endif

	/*
	 * Set up a place to put thread-specific data.
	 * We only need a single pointer per thread - it points to the
	 * CitContext structure (in the ContextList linked list) of the
	 * session to which the calling thread is currently bound.
	 */
	if (pthread_key_create(&MyConKey, NULL) != 0) {
		syslog(LOG_CRIT, "Can't create TSD key: %s", strerror(errno));
	}

	/*
	 * Interript, hangup, and terminate signals should cause the server
	 * to gracefully clean up and shut down.
	 */
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGHUP);
	sigaddset(&set, SIGTERM);
	sigprocmask(SIG_UNBLOCK, &set, NULL);

	signal(SIGINT, signal_cleanup);
	signal(SIGHUP, signal_cleanup);
	signal(SIGTERM, signal_cleanup);
	signal(SIGUSR2, signal_exit);

	/*
	 * Do not shut down the server on broken pipe signals, otherwise the
	 * whole Citadel service would come down whenever a single client
	 * socket breaks.
	 */
	signal(SIGPIPE, SIG_IGN);
}


/* 
 * This is a generic function to set up a master socket for listening on
 * a TCP port.  The server shuts down if the bind fails.  (IPv4/IPv6 version)
 *
 * ip_addr 	IP address to bind
 * port_number	port number to bind
 * queue_len	number of incoming connections to allow in the queue
 */
int ctdl_tcp_server(char *ip_addr, int port_number, int queue_len, char *errormessage)
{
	struct protoent *p;
	struct sockaddr_in6 sin6;
	struct sockaddr_in sin4;
	int s, i, b;
	int ip_version = 6;

	memset(&sin6, 0, sizeof(sin6));
	memset(&sin4, 0, sizeof(sin4));
	sin6.sin6_family = AF_INET6;
	sin4.sin_family = AF_INET;

	if (	(ip_addr == NULL)							/* any IPv6 */
		|| (IsEmptyStr(ip_addr))
		|| (!strcmp(ip_addr, "*"))
	) {
		ip_version = 6;
		sin6.sin6_addr = in6addr_any;
	}
	else if (!strcmp(ip_addr, "0.0.0.0"))						/* any IPv4 */
	{
		ip_version = 4;
		sin4.sin_addr.s_addr = INADDR_ANY;
	}
	else if ((strchr(ip_addr, '.')) && (!strchr(ip_addr, ':')))			/* specific IPv4 */
	{
		ip_version = 4;
		if (inet_pton(AF_INET, ip_addr, &sin4.sin_addr) <= 0) {
			snprintf(errormessage, SIZ,
				 "Error binding to [%s] : %s", ip_addr, strerror(errno)
			);
			syslog(LOG_ALERT, "%s", errormessage);
			return (-1);
		}
	}
	else										/* specific IPv6 */
	{
		ip_version = 6;
		if (inet_pton(AF_INET6, ip_addr, &sin6.sin6_addr) <= 0) {
			snprintf(errormessage, SIZ,
				 "Error binding to [%s] : %s", ip_addr, strerror(errno)
			);
			syslog(LOG_ALERT, "%s", errormessage);
			return (-1);
		}
	}

	if (port_number == 0) {
		snprintf(errormessage, SIZ, "Can't start: no port number specified.");
		syslog(LOG_ALERT, "%s", errormessage);
		return (-1);
	}
	sin6.sin6_port = htons((u_short) port_number);
	sin4.sin_port = htons((u_short) port_number);

	p = getprotobyname("tcp");

	s = socket( ((ip_version == 6) ? PF_INET6 : PF_INET), SOCK_STREAM, (p->p_proto));
	if (s < 0) {
		snprintf(errormessage, SIZ,
			 "Can't create a listening socket: %s", strerror(errno)
		);
		syslog(LOG_ALERT, "%s", errormessage);
		return (-1);
	}
	/* Set some socket options that make sense. */
	i = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));

	if (ip_version == 6) {
		b = bind(s, (struct sockaddr *) &sin6, sizeof(sin6));
	}
	else {
		b = bind(s, (struct sockaddr *) &sin4, sizeof(sin4));
	}

	if (b < 0) {
		snprintf(errormessage, SIZ,
			 "Can't bind: %s", strerror(errno)
		);
		syslog(LOG_ALERT, "%s", errormessage);
		return (-1);
	}

	fcntl(s, F_SETFL, O_NONBLOCK);

	if (listen(s, ((queue_len >= 5) ? queue_len : 5) ) < 0) {
		snprintf(errormessage, SIZ,
			 "Can't listen: %s", strerror(errno)
		);
		syslog(LOG_ALERT, "%s", errormessage);
		return (-1);
	}
	return (s);
}





/*
 * Create a Unix domain socket and listen on it
 */
int ctdl_uds_server(char *sockpath, int queue_len, char *errormessage)
{
	struct sockaddr_un addr;
	int s;
	int i;
	int actual_queue_len;
#ifdef HAVE_STRUCT_UCRED
	int passcred = 1;
#endif

	actual_queue_len = queue_len;
	if (actual_queue_len < 5) actual_queue_len = 5;

	i = unlink(sockpath);
	if ((i != 0) && (errno != ENOENT)) {
		snprintf(errormessage, SIZ, "citserver: can't unlink %s: %s",
			sockpath, strerror(errno)
		);
		syslog(LOG_EMERG, "%s", errormessage);
		return(-1);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	safestrncpy(addr.sun_path, sockpath, sizeof addr.sun_path);

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0) {
		snprintf(errormessage, SIZ, 
			 "citserver: Can't create a socket: %s",
			 strerror(errno));
		syslog(LOG_EMERG, "%s", errormessage);
		return(-1);
	}

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		snprintf(errormessage, SIZ, 
			 "citserver: Can't bind: %s",
			 strerror(errno));
		syslog(LOG_EMERG, "%s", errormessage);
		return(-1);
	}

	/* set to nonblock - we need this for some obscure situations */
	if (fcntl(s, F_SETFL, O_NONBLOCK) < 0) {
		snprintf(errormessage, SIZ, 
			 "citserver: Can't set socket to non-blocking: %s",
			 strerror(errno));
		syslog(LOG_EMERG, "%s", errormessage);
		close(s);
		return(-1);
	}

	if (listen(s, actual_queue_len) < 0) {
		snprintf(errormessage, SIZ, 
			 "citserver: Can't listen: %s",
			 strerror(errno));
		syslog(LOG_EMERG, "%s", errormessage);
		return(-1);
	}

#ifdef HAVE_STRUCT_UCRED
	setsockopt(s, SOL_SOCKET, SO_PASSCRED, &passcred, sizeof(passcred));
#endif

	chmod(sockpath, S_ISGID|S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH);
	return(s);
}



/*
 * The following functions implement output buffering on operating systems which
 * support it (such as Linux and various BSD flavors).
 */
#ifndef HAVE_DARWIN
#ifdef TCP_CORK
#	define HAVE_TCP_BUFFERING
#else
#	ifdef TCP_NOPUSH
#		define HAVE_TCP_BUFFERING
#		define TCP_CORK TCP_NOPUSH
#	endif
#endif /* TCP_CORK */
#endif /* HAVE_DARWIN */

static unsigned on = 1, off = 0;

void buffer_output(void) {
#ifdef HAVE_TCP_BUFFERING
#ifdef HAVE_OPENSSL
	if (!CC->redirect_ssl)
#endif
		setsockopt(CC->client_socket, IPPROTO_TCP, TCP_CORK, &on, 4);
#endif
}

void unbuffer_output(void) {
#ifdef HAVE_TCP_BUFFERING
#ifdef HAVE_OPENSSL
	if (!CC->redirect_ssl)
#endif
		setsockopt(CC->client_socket, IPPROTO_TCP, TCP_CORK, &off, 4);
#endif
}

void flush_output(void) {
#ifdef HAVE_TCP_BUFFERING
	struct CitContext *CCC = CC;
	setsockopt(CCC->client_socket, IPPROTO_TCP, TCP_CORK, &off, 4);
	setsockopt(CCC->client_socket, IPPROTO_TCP, TCP_CORK, &on, 4);
#endif
}

/*
static void flush_client_inbuf(void)
{
	CitContext *CCC=CC;

	FlushStrBuf(CCC->ReadBuf);
	CCC->RecvBuf->ReadWritePointer = NULL;

}
*/


/*
 * client_close()	...	close the client socket
 */
void client_close(void) {
	CitContext *CCC = CC;

	if (!CCC) return;
	if (CCC->client_socket <= 0) return;
	syslog(LOG_DEBUG, "Closing socket %d", CCC->client_socket);

	close(CCC->client_socket);
	CCC->client_socket = -1 ;
}




/*
 * client_write()   ...    Send binary data to the client.
 */
int client_write(const char *buf, int nbytes)
{
	int bytes_written = 0;
	int retval;
#ifndef HAVE_TCP_BUFFERING
	int old_buffer_len = 0;
#endif
	fd_set wset;
	CitContext *Ctx;
	int fdflags;

	if (nbytes < 1) return(0);

	Ctx = CC;

#ifdef BIGBAD_IODBG
	{
		int rv = 0;
		char fn [SIZ];
		FILE *fd;
		
		snprintf(fn, SIZ, "/tmp/foolog_%s.%d", Ctx->ServiceName, Ctx->cs_pid);
		
		fd = fopen(fn, "a+");
		if (fd)
		{
		    fprintf(fd, "Sending: BufSize: %d BufContent: [",
			    nbytes);
		    rv = fwrite(buf, nbytes, 1, fd);
		    fprintf(fd, "]\n");
		    fclose(fd);
		}
	}
#endif
//	flush_client_inbuf();
	if (Ctx->redirect_buffer != NULL) {
		StrBufAppendBufPlain(Ctx->redirect_buffer,
				     buf, nbytes, 0);
		return 0;
	}

#ifdef HAVE_OPENSSL
	if (Ctx->redirect_ssl) {
		client_write_ssl(buf, nbytes);
		return 0;
	}
#endif
	if (Ctx->client_socket == -1) return -1;

	fdflags = fcntl(Ctx->client_socket, F_GETFL);

	while ((bytes_written < nbytes) && (Ctx->client_socket != -1)){
		if ((fdflags & O_NONBLOCK) == O_NONBLOCK) {
			FD_ZERO(&wset);
			FD_SET(Ctx->client_socket, &wset);
			if (select(1, NULL, &wset, NULL, NULL) == -1) {
				if (errno == EINTR)
				{
					syslog(LOG_DEBUG, "client_write(%d bytes) select() interrupted.",
						nbytes-bytes_written
					);
					if (server_shutting_down) {
						CC->kill_me = KILLME_SELECT_INTERRUPTED;
						return (-1);
					} else {
						/* can't trust fd's and stuff so we need to re-create them */
						continue;
					}
				} else {
					syslog(LOG_ERR,
						"client_write(%d bytes) select failed: %s (%d)",
						nbytes - bytes_written,
						strerror(errno), errno
					);
					cit_backtrace();
					client_close();
					Ctx->kill_me = KILLME_SELECT_FAILED;
					return -1;
				}
			}
		}

		retval = write(Ctx->client_socket, &buf[bytes_written], nbytes - bytes_written);
		if (retval < 1) {
			syslog(LOG_ERR,
				"client_write(%d bytes) failed: %s (%d)",
				nbytes - bytes_written,
				strerror(errno), errno
			);
			cit_backtrace();
			client_close();
			Ctx->kill_me = KILLME_WRITE_FAILED;
			return -1;
		}
		bytes_written = bytes_written + retval;
	}
	return 0;
}

void cputbuf(const StrBuf *Buf) {   
	client_write(ChrPtr(Buf), StrLength(Buf)); 
}   


/*
 * cprintf()	Send formatted printable data to the client.
 *		Implemented in terms of client_write() so it's technically not sysdep...
 */
void cprintf(const char *format, ...) {   
	va_list arg_ptr;   
	char buf[1024];
   
	va_start(arg_ptr, format);   
	if (vsnprintf(buf, sizeof buf, format, arg_ptr) == -1)
		buf[sizeof buf - 2] = '\n';
	client_write(buf, strlen(buf)); 
	va_end(arg_ptr);
}   


/*
 * Read data from the client socket.
 *
 * sock		socket fd to read from
 * buf		buffer to read into 
 * bytes	number of bytes to read
 * timeout	Number of seconds to wait before timing out
 *
 * Possible return values:
 *      1       Requested number of bytes has been read.
 *      0       Request timed out.
 *	-1   	Connection is broken, or other error.
 */
int client_read_blob(StrBuf *Target, int bytes, int timeout)
{
	CitContext *CCC=CC;
	const char *Error;
	int retval = 0;

#ifdef HAVE_OPENSSL
	if (CCC->redirect_ssl) {
#ifdef BIGBAD_IODBG
		int rv = 0;
		char fn [SIZ];
		FILE *fd;
		
		snprintf(fn, SIZ, "/tmp/foolog_%s.%d", CCC->ServiceName, CCC->cs_pid);
			
		fd = fopen(fn, "a+");
		fprintf(fd, "Reading BLOB: BufSize: %d ",
			bytes);
		rv = fwrite(ChrPtr(Target), StrLength(Target), 1, fd);
		fprintf(fd, "]\n");
		
			
		fclose(fd);
#endif
		retval = client_read_sslblob(Target, bytes, timeout);
		if (retval < 0) {
			syslog(LOG_CRIT, "client_read_blob() failed");
		}
#ifdef BIGBAD_IODBG
		snprintf(fn, SIZ, "/tmp/foolog_%s.%d", CCC->ServiceName, CCC->cs_pid);
		
		fd = fopen(fn, "a+");
		fprintf(fd, "Read: %d BufContent: [",
			StrLength(Target));
		rv = fwrite(ChrPtr(Target), StrLength(Target), 1, fd);
		fprintf(fd, "]\n");
		
		
		fclose(fd);
#endif
	}
	else 
#endif
	{
#ifdef BIGBAD_IODBG
		int rv = 0;
		char fn [SIZ];
		FILE *fd;
		
		snprintf(fn, SIZ, "/tmp/foolog_%s.%d", CCC->ServiceName, CCC->cs_pid);
			
		fd = fopen(fn, "a+");
		fprintf(fd, "Reading BLOB: BufSize: %d ",
			bytes);
		rv = fwrite(ChrPtr(Target), StrLength(Target), 1, fd);
		fprintf(fd, "]\n");
		
			
		fclose(fd);
#endif
		retval = StrBufReadBLOBBuffered(Target, 
						CCC->RecvBuf.Buf,
						&CCC->RecvBuf.ReadWritePointer,
						&CCC->client_socket,
						1, 
						bytes,
						O_TERM,
						&Error);
		if (retval < 0) {
			syslog(LOG_CRIT, "client_read_blob() failed: %s", Error);
			client_close();
			return retval;
		}
#ifdef BIGBAD_IODBG
		snprintf(fn, SIZ, "/tmp/foolog_%s.%d", CCC->ServiceName, CCC->cs_pid);
		
		fd = fopen(fn, "a+");
		fprintf(fd, "Read: %d BufContent: [",
			StrLength(Target));
		rv = fwrite(ChrPtr(Target), StrLength(Target), 1, fd);
		fprintf(fd, "]\n");
		fclose(fd);
#endif
	}
	return retval;
}


/*
 * to make client_read_random_blob() more efficient, increase buffer size.
 * just use in greeting function, else your buffer may be flushed
 */
void client_set_inbound_buf(long N)
{
	CitContext *CCC=CC;
	FlushStrBuf(CCC->RecvBuf.Buf);
	ReAdjustEmptyBuf(CCC->RecvBuf.Buf, N * SIZ, N * SIZ);
}

int client_read_random_blob(StrBuf *Target, int timeout)
{
	CitContext *CCC=CC;
	int rc;

	rc =  client_read_blob(Target, 1, timeout);
	if (rc > 0)
	{
		long len;
		const char *pch;
		
		len = StrLength(CCC->RecvBuf.Buf);
		pch = ChrPtr(CCC->RecvBuf.Buf);

		if (len > 0)
		{
			if (CCC->RecvBuf.ReadWritePointer != NULL) {
				len -= CCC->RecvBuf.ReadWritePointer - pch;
				pch = CCC->RecvBuf.ReadWritePointer;
			}
			StrBufAppendBufPlain(Target, pch, len, 0);
			FlushStrBuf(CCC->RecvBuf.Buf);
			CCC->RecvBuf.ReadWritePointer = NULL;
#ifdef BIGBAD_IODBG
			{
				int rv = 0;
				char fn [SIZ];
				FILE *fd;
			
				snprintf(fn, SIZ, "/tmp/foolog_%s.%d", CCC->ServiceName, CCC->cs_pid);
			
				fd = fopen(fn, "a+");
				fprintf(fd, "Read: BufSize: %d BufContent: [",
					StrLength(Target));
				rv = fwrite(ChrPtr(Target), StrLength(Target), 1, fd);
				fprintf(fd, "]\n");
			
			
				fclose(fd);
			}
#endif
	
			return StrLength(Target);
		}
		return rc;
	}
	else
		return rc;
}

int client_read_to(char *buf, int bytes, int timeout)
{
	CitContext *CCC=CC;
	int rc;

	rc = client_read_blob(CCC->MigrateBuf, bytes, timeout);
	if (rc < 0)
	{
		*buf = '\0';
		return rc;
	}
	else
	{
		memcpy(buf, 
		       ChrPtr(CCC->MigrateBuf),
		       StrLength(CCC->MigrateBuf) + 1);
		FlushStrBuf(CCC->MigrateBuf);
		return rc;
	}
}


int HaveMoreLinesWaiting(CitContext *CCC)
{
	if ((CCC->kill_me != 0) ||
	    ( (CCC->RecvBuf.ReadWritePointer == NULL) && 
	      (StrLength(CCC->RecvBuf.Buf) == 0) && 
	      (CCC->client_socket != -1)) )
		return 0;
	else
		return 1;
}


/*
 * Read data from the client socket with default timeout.
 * (This is implemented in terms of client_read_to() and could be
 * justifiably moved out of sysdep.c)
 */
INLINE int client_read(char *buf, int bytes)
{
	return(client_read_to(buf, bytes, config.c_sleeping));
}

int CtdlClientGetLine(StrBuf *Target)
{
	CitContext *CCC=CC;
	const char *Error;
	int rc;

	FlushStrBuf(Target);
#ifdef HAVE_OPENSSL
	if (CCC->redirect_ssl) {
#ifdef BIGBAD_IODBG
		char fn [SIZ];
		FILE *fd;
		int len = 0;
		int rlen = 0;
		int  nlen = 0;
		int nrlen = 0;
		const char *pch;

		snprintf(fn, SIZ, "/tmp/foolog_%s.%d", CCC->ServiceName, CCC->cs_pid);

		fd = fopen(fn, "a+");
		pch = ChrPtr(CCC->RecvBuf.Buf);
		len = StrLength(CCC->RecvBuf.Buf);
		if (CCC->RecvBuf.ReadWritePointer != NULL)
			rlen = CCC->RecvBuf.ReadWritePointer - pch;
		else
			rlen = 0;

/*		fprintf(fd, "\n\n\nBufSize: %d BufPos: %d \nBufContent: [%s]\n\n_____________________\n",
			len, rlen, pch);
*/
		fprintf(fd, "\n\n\nSSL1: BufSize: %d BufPos: %d \n_____________________\n",
			len, rlen);
#endif
		rc = client_readline_sslbuffer(Target,
					       CCC->RecvBuf.Buf,
					       &CCC->RecvBuf.ReadWritePointer,
					       1);
#ifdef BIGBAD_IODBG
                pch = ChrPtr(CCC->RecvBuf.Buf);
                nlen = StrLength(CCC->RecvBuf.Buf);
                if (CCC->RecvBuf.ReadWritePointer != NULL)
                        nrlen = CCC->RecvBuf.ReadWritePointer - pch;
                else
                        nrlen = 0;
/*
                fprintf(fd, "\n\n\nBufSize: was: %d is: %d BufPos: was: %d is: %d \nBufContent: [%s]\n\n_____________________\n",
                        len, nlen, rlen, nrlen, pch);
*/
                fprintf(fd, "\n\n\nSSL2: BufSize: was: %d is: %d BufPos: was: %d is: %d \n",
                        len, nlen, rlen, nrlen);

                fprintf(fd, "SSL3: Read: BufSize: %d BufContent: [%s]\n\n*************\n",
                        StrLength(Target), ChrPtr(Target));
                fclose(fd);

		if (rc < 0) {
			syslog(LOG_CRIT, "CtdlClientGetLine() failed");
		}
#endif
		return rc;
	}
	else 
#endif
	{
#ifdef BIGBAD_IODBG
		char fn [SIZ];
		FILE *fd;
		int len, rlen, nlen, nrlen;
		const char *pch;

		snprintf(fn, SIZ, "/tmp/foolog_%s.%d", CCC->ServiceName, CCC->cs_pid);

		fd = fopen(fn, "a+");
		pch = ChrPtr(CCC->RecvBuf.Buf);
		len = StrLength(CCC->RecvBuf.Buf);
		if (CCC->RecvBuf.ReadWritePointer != NULL)
			rlen = CCC->RecvBuf.ReadWritePointer - pch;
		else
			rlen = 0;

/*		fprintf(fd, "\n\n\nBufSize: %d BufPos: %d \nBufContent: [%s]\n\n_____________________\n",
			len, rlen, pch);
*/
		fprintf(fd, "\n\n\nBufSize: %d BufPos: %d \n_____________________\n",
			len, rlen);
#endif
		rc = StrBufTCP_read_buffered_line_fast(Target, 
						       CCC->RecvBuf.Buf,
						       &CCC->RecvBuf.ReadWritePointer,
						       &CCC->client_socket,
						       5,
						       1,
						       &Error);

#ifdef BIGBAD_IODBG
                pch = ChrPtr(CCC->RecvBuf.Buf);
                nlen = StrLength(CCC->RecvBuf.Buf);
                if (CCC->RecvBuf.ReadWritePointer != NULL)
                        nrlen = CCC->RecvBuf.ReadWritePointer - pch;
                else
                        nrlen = 0;
/*
                fprintf(fd, "\n\n\nBufSize: was: %d is: %d BufPos: was: %d is: %d \nBufContent: [%s]\n\n_____________________\n",
                        len, nlen, rlen, nrlen, pch);
*/
                fprintf(fd, "\n\n\nBufSize: was: %d is: %d BufPos: was: %d is: %d \n",
                        len, nlen, rlen, nrlen);

                fprintf(fd, "Read: BufSize: %d BufContent: [%s]\n\n*************\n",
                        StrLength(Target), ChrPtr(Target));
                fclose(fd);

		if ((rc < 0) && (Error != NULL)) {
			syslog(LOG_CRIT, "CtdlClientGetLine() failed: %s", Error);
		}
#endif
		return rc;
	}
}


/*
 * client_getln()   ...   Get a LF-terminated line of text from the client.
 * (This is implemented in terms of client_read() and could be
 * justifiably moved out of sysdep.c)
 */
int client_getln(char *buf, int bufsize)
{
	int i, retval;
	CitContext *CCC=CC;
	const char *pCh;

	retval = CtdlClientGetLine(CCC->MigrateBuf);
	if (retval < 0)
	  return(retval >= 0);


	i = StrLength(CCC->MigrateBuf);
	pCh = ChrPtr(CCC->MigrateBuf);
	/* Strip the trailing LF, and the trailing CR if present.
	 */
	if (bufsize <= i)
		i = bufsize - 1;
	while ( (i > 0)
		&& ( (pCh[i - 1]==13)
		     || ( pCh[i - 1]==10)) ) {
		i--;
	}
	memcpy(buf, pCh, i);
	buf[i] = 0;

	FlushStrBuf(CCC->MigrateBuf);
	if (retval < 0) {
		safestrncpy(&buf[i], "000", bufsize - i);
	}
	return(retval >= 0);
}


/*
 * Cleanup any contexts that are left lying around
 */


void close_masters (void)
{
	struct ServiceFunctionHook *serviceptr;
	
	/*
	 * close all protocol master sockets
	 */
	for (serviceptr = ServiceHookTable; serviceptr != NULL;
	    serviceptr = serviceptr->next ) {

		if (serviceptr->tcp_port > 0)
		{
			syslog(LOG_INFO, "Closing %d listener on port %d\n",
			       serviceptr->msock,
			       serviceptr->tcp_port);
			serviceptr->tcp_port = 0;
		}
		
		if (serviceptr->sockpath != NULL)
			syslog(LOG_INFO, "Closing %d listener on '%s'\n",
			       serviceptr->msock,
			       serviceptr->sockpath);
                if (serviceptr->msock != -1)
			close(serviceptr->msock);
		/* If it's a Unix domain socket, remove the file. */
		if (serviceptr->sockpath != NULL) {
			unlink(serviceptr->sockpath);
			serviceptr->sockpath = NULL;
		}
	}
}


/*
 * The system-dependent part of master_cleanup() - close the master socket.
 */
void sysdep_master_cleanup(void) {
	
	close_masters();
	
	context_cleanup();
	
#ifdef HAVE_OPENSSL
	destruct_ssl();
#endif
	CtdlDestroyProtoHooks();
	CtdlDestroyDeleteHooks();
	CtdlDestroyXmsgHooks();
	CtdlDestroyNetprocHooks();
	CtdlDestroyUserHooks();
	CtdlDestroyMessageHook();
	CtdlDestroyCleanupHooks();
	CtdlDestroyFixedOutputHooks();	
	CtdlDestroySessionHooks();
	CtdlDestroyServiceHook();
	CtdlDestroyRoomHooks();
	CtdlDestroySearchHooks();
	CtdlDestroyDebugTable();
	#ifdef HAVE_BACKTRACE
///	eCrash_Uninit();
	#endif
}



pid_t current_child;
void graceful_shutdown(int signum) {
	kill(current_child, signum);
	unlink(file_pid_file);
	exit(0);
}

int nFireUps = 0;
int nFireUpsNonRestart = 0;
pid_t ForkedPid = 1;

/*
 * Start running as a daemon.
 */
void start_daemon(int unused) {
	int status = 0;
	pid_t child = 0;
	FILE *fp;
	int do_restart = 0;

	current_child = 0;

	/* Close stdin/stdout/stderr and replace them with /dev/null.
	 * We don't just call close() because we don't want these fd's
	 * to be reused for other files.
	 */
	if (chdir(ctdl_run_dir) != 0)
		syslog(LOG_EMERG, 
			      "unable to change into directory [%s]: %s", 
			      ctdl_run_dir, strerror(errno));

	child = fork();
	if (child != 0) {
		exit(0);
	}
	
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);

	setsid();
	umask(0);
        if ((freopen("/dev/null", "r", stdin) != stdin) || 
	    (freopen("/dev/null", "w", stdout) != stdout) || 
	    (freopen("/dev/null", "w", stderr) != stderr))
		syslog(LOG_EMERG, 
			      "unable to reopen stdin/out/err %s", 
			      strerror(errno));
		

	do {
		current_child = fork();

		signal(SIGTERM, graceful_shutdown);
	
		if (current_child < 0) {
			perror("fork");
			exit(errno);
		}
	
		else if (current_child == 0) {
			return; /* continue starting citadel. */
		}
	
		else {
			fp = fopen(file_pid_file, "w");
			if (fp != NULL) {
				fprintf(fp, ""F_PID_T"\n", getpid());
				fclose(fp);
			}
			waitpid(current_child, &status, 0);
		}

		nFireUpsNonRestart = nFireUps;
		
		/* Exit code 0 means the watcher should exit */
		if (WIFEXITED(status) && (WEXITSTATUS(status) == CTDLEXIT_SHUTDOWN)) {
			do_restart = 0;
		}

		/* Exit code 101-109 means the watcher should exit */
		else if (WIFEXITED(status) && (WEXITSTATUS(status) >= 101) && (WEXITSTATUS(status) <= 109)) {
			do_restart = 0;
		}

		/* Any other exit code, or no exit code, means we should restart. */
		else {
			do_restart = 1;
			nFireUps++;
			ForkedPid = current_child;
		}

	} while (do_restart);

	unlink(file_pid_file);
	exit(WEXITSTATUS(status));
}



void checkcrash(void)
{
	if (nFireUpsNonRestart != nFireUps)
	{
		StrBuf *CrashMail;

		CrashMail = NewStrBuf();
		syslog(LOG_ALERT, "Posting crash message\n");
		StrBufPrintf(CrashMail, 
			" \n"
			" The Citadel server process (citserver) terminated unexpectedly."
			"\n \n"
			" This could be the result of a bug in the server program, or some external "
			"factor.\n \n"
			" You can obtain more information about this by enabling core dumps.\n \n"
			" For more information, please see:\n \n"
			" http://citadel.org/doku.php?id=faq:mastering_your_os:gdb#how.do.i.make.my.system.produce.core-files"
			"\n \n"

			" If you have already done this, the core dump is likely to be found at %score.%d\n"
			,
			ctdl_run_dir, ForkedPid);
		CtdlAideMessage(ChrPtr(CrashMail), "Citadel server process terminated unexpectedly");
		FreeStrBuf(&CrashMail);
	}
}


/*
 * Generic routine to convert a login name to a full name (gecos)
 * Returns nonzero if a conversion took place
 */
int convert_login(char NameToConvert[]) {
	struct passwd *pw;
	int a;

	pw = getpwnam(NameToConvert);
	if (pw == NULL) {
		return(0);
	}
	else {
		strcpy(NameToConvert, pw->pw_gecos);
		for (a=0; a<strlen(NameToConvert); ++a) {
			if (NameToConvert[a] == ',') NameToConvert[a] = 0;
		}
		return(1);
	}
}



void HuntBadSession(void)
{
	int highest;
	CitContext *ptr;
	fd_set readfds;
	struct timeval tv;
	struct ServiceFunctionHook *serviceptr;



	/* Next, add all of the client sockets. */
	begin_critical_section(S_SESSION_TABLE);
	for (ptr = ContextList; ptr != NULL; ptr = ptr->next) {
		if ((ptr->state == CON_SYS) && (ptr->client_socket == 0))
			continue;
		/* Initialize the fdset. */
		FD_ZERO(&readfds);
		highest = 0;
		tv.tv_sec = 0;		/* wake up every second if no input */
		tv.tv_usec = 0;

		/* Don't select on dead sessions, only truly idle ones */
		if (	(ptr->state == CON_IDLE)
			&& (ptr->kill_me == 0)
			&& (ptr->client_socket > 0)
			) {
			FD_SET(ptr->client_socket, &readfds);
			if (ptr->client_socket > highest)
				highest = ptr->client_socket;
			
			if ((select(highest + 1, &readfds, NULL, NULL, &tv) < 0) &&
			    (errno == EBADF))
			{
				/* Gotcha! */
				syslog(LOG_EMERG,
				       "Killing Session CC[%d] bad FD: [%d:%d] User[%s] Host[%s:%s]\n",
				       ptr->cs_pid,
				       ptr->client_socket,
				       ptr->is_local_socket,
				       ptr->curr_user,
				       ptr->cs_host,ptr->cs_addr);

				ptr->kill_me = 1;
				ptr->client_socket = -1;
				break;
			}
		}
		
	}
	end_critical_section(S_SESSION_TABLE);


	/* First, add the various master sockets to the fdset. */
	for (serviceptr = ServiceHookTable; serviceptr != NULL; serviceptr = serviceptr->next ) {

		/* Initialize the fdset. */
		highest = 0;
		tv.tv_sec = 0;		/* wake up every second if no input */
		tv.tv_usec = 0;

		FD_SET(serviceptr->msock, &readfds);
		if (serviceptr->msock > highest) {
			highest = serviceptr->msock;
		}
		if ((select(highest + 1, &readfds, NULL, NULL, &tv) < 0) &&
		    (errno == EBADF))
		{
			/* Gotcha! server socket dead? commit suicide! */
			syslog(LOG_EMERG,
			       "Found bad FD: %d and its a server socket! Shutting Down!\n",
			       serviceptr->msock);

			server_shutting_down = 1;
			break;
		}
	}


}


/* 
 * This loop just keeps going and going and going...
 */
void *worker_thread(void *blah) {
	int highest;
	CitContext *ptr;
	CitContext *bind_me = NULL;
	fd_set readfds;
	int retval = 0;
	struct timeval tv;
	int force_purge = 0;
	struct ServiceFunctionHook *serviceptr;
	int ssock;                      /* Descriptor for client socket */
	CitContext *con = NULL;         /* Temporary context pointer */
	int i;

	++num_workers;

	while (!server_shutting_down) {

		/* make doubly sure we're not holding any stale db handles
		 * which might cause a deadlock.
		 */
		cdb_check_handles();
do_select:	force_purge = 0;
		bind_me = NULL;		/* Which session shall we handle? */

		/* Initialize the fdset. */
		FD_ZERO(&readfds);
		highest = 0;

		/* First, add the various master sockets to the fdset. */
		for (serviceptr = ServiceHookTable; serviceptr != NULL; serviceptr = serviceptr->next ) {
			FD_SET(serviceptr->msock, &readfds);
			if (serviceptr->msock > highest) {
				highest = serviceptr->msock;
			}
		}

		/* Next, add all of the client sockets. */
		begin_critical_section(S_SESSION_TABLE);
		for (ptr = ContextList; ptr != NULL; ptr = ptr->next) {
			if ((ptr->state == CON_SYS) && (ptr->client_socket == 0))
			    continue;

			/* Don't select on dead sessions, only truly idle ones */
			if (	(ptr->state == CON_IDLE)
				&& (ptr->kill_me == 0)
				&& (ptr->client_socket > 0)
			) {
				FD_SET(ptr->client_socket, &readfds);
				if (ptr->client_socket > highest)
					highest = ptr->client_socket;
			}
			if ((bind_me == NULL) && (ptr->state == CON_READY)) {
				bind_me = ptr;
				ptr->state = CON_EXECUTING;
				break;
			}
			if ((bind_me == NULL) && (ptr->state == CON_GREETING)) {
				bind_me = ptr;
				ptr->state = CON_STARTING;
				break;
			}
		}
		end_critical_section(S_SESSION_TABLE);

		if (bind_me) {
			goto SKIP_SELECT;
		}

		/* If we got this far, it means that there are no sessions
		 * which a previous thread marked for attention, so we go
		 * ahead and get ready to select().
		 */

		if (!server_shutting_down) {
			tv.tv_sec = 1;		/* wake up every second if no input */
			tv.tv_usec = 0;
			retval = select(highest + 1, &readfds, NULL, NULL, &tv);
		}
		else {
			--num_workers;
			return NULL;
		}

		/* Now figure out who made this select() unblock.
		 * First, check for an error or exit condition.
		 */
		if (retval < 0) {
			if (errno == EBADF) {
				syslog(LOG_EMERG, "select() failed: (%s)\n", strerror(errno));
				HuntBadSession ();
				goto do_select;
			}
			if (errno != EINTR) {
				syslog(LOG_EMERG, "Exiting (%s)\n", strerror(errno));
				server_shutting_down = 1;
				continue;
			} else {
#if 0
				syslog(LOG_DEBUG, "Interrupted select()\n");
#endif
				if (server_shutting_down) {
					--num_workers;
					return(NULL);
				}
				goto do_select;
			}
		}
		else if (retval == 0) {
			if (server_shutting_down) {
				--num_workers;
				return(NULL);
			}
		}

		/* Next, check to see if it's a new client connecting * on a master socket. */

		else if ((retval > 0) && (!server_shutting_down)) for (serviceptr = ServiceHookTable; serviceptr != NULL; serviceptr = serviceptr->next) {

			if (FD_ISSET(serviceptr->msock, &readfds)) {
				ssock = accept(serviceptr->msock, NULL, 0);
				if (ssock >= 0) {
					syslog(LOG_DEBUG, "New client socket %d", ssock);

					/* The master socket is non-blocking but the client
					 * sockets need to be blocking, otherwise certain
					 * operations barf on FreeBSD.  Not a fatal error.
					 */
					if (fcntl(ssock, F_SETFL, 0) < 0) {
						syslog(LOG_EMERG,
							"citserver: Can't set socket to blocking: %s\n",
							strerror(errno));
					}

					/* New context will be created already
					 * set up in the CON_EXECUTING state.
					 */
					con = CreateNewContext();

					/* Assign our new socket number to it. */
					con->tcp_port = serviceptr->tcp_port;
					con->client_socket = ssock;
					con->h_command_function = serviceptr->h_command_function;
					con->h_async_function = serviceptr->h_async_function;
					con->h_greeting_function = serviceptr->h_greeting_function;
					con->ServiceName = serviceptr->ServiceName;
					
					/* Determine whether it's a local socket */
					if (serviceptr->sockpath != NULL) {
						con->is_local_socket = 1;
					}
	
					/* Set the SO_REUSEADDR socket option */
					i = 1;
					setsockopt(ssock, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));
					con->state = CON_GREETING;
					retval--;
					if (retval == 0)
						break;
				}
			}
		}

		/* It must be a client socket.  Find a context that has data
		 * waiting on its socket *and* is in the CON_IDLE state.  Any
		 * active sockets other than our chosen one are marked as
		 * CON_READY so the next thread that comes around can just bind
		 * to one without having to select() again.
		 */
		begin_critical_section(S_SESSION_TABLE);
		for (ptr = ContextList; ptr != NULL; ptr = ptr->next) {
			int checkfd = ptr->client_socket;
			if ((checkfd != -1) && (ptr->state == CON_IDLE) ){
				if (FD_ISSET(checkfd, &readfds)) {
					ptr->input_waiting = 1;
					if (!bind_me) {
						bind_me = ptr;	/* I choose you! */
						bind_me->state = CON_EXECUTING;
					}
					else {
						ptr->state = CON_READY;
					}
				} else if ((ptr->is_async) && (ptr->async_waiting) && (ptr->h_async_function)) {
					if (!bind_me) {
						bind_me = ptr;	/* I choose you! */
						bind_me->state = CON_EXECUTING;
					}
					else {
						ptr->state = CON_READY;
					}
				}
			}
		}
		end_critical_section(S_SESSION_TABLE);

SKIP_SELECT:
		/* We're bound to a session */
		++active_workers;
		if (bind_me != NULL) {
			become_session(bind_me);

			if (bind_me->state == CON_STARTING) {
				bind_me->state = CON_EXECUTING;
				begin_session(bind_me);
				bind_me->h_greeting_function();
			}
			/* If the client has sent a command, execute it. */
			if (CC->input_waiting) {
				CC->h_command_function();

				while (HaveMoreLinesWaiting(CC))
				       CC->h_command_function();

				CC->input_waiting = 0;
			}

			/* If there are asynchronous messages waiting and the
			 * client supports it, do those now */
			if ((CC->is_async) && (CC->async_waiting)
			   && (CC->h_async_function != NULL)) {
				CC->h_async_function();
				CC->async_waiting = 0;
			}
			
			force_purge = CC->kill_me;
			become_session(NULL);
			bind_me->state = CON_IDLE;
		}

		dead_session_purge(force_purge);
		do_housekeeping();
		--active_workers;
	}

	/* If control reaches this point, the server is shutting down */
	--num_workers;
	return(NULL);
}



/*
 * SyslogFacility()
 * Translate text facility name to syslog.h defined value.
 */
int SyslogFacility(char *name)
{
	int i;
	struct
	{
		int facility;
		char *name;
	}   facTbl[] =
	{
		{   LOG_KERN,   "kern"		},
		{   LOG_USER,   "user"		},
		{   LOG_MAIL,   "mail"		},
		{   LOG_DAEMON, "daemon"	},
		{   LOG_AUTH,   "auth"		},
		{   LOG_SYSLOG, "syslog"	},
		{   LOG_LPR,	"lpr"		},
		{   LOG_NEWS,   "news"		},
		{   LOG_UUCP,   "uucp"		},
		{   LOG_LOCAL0, "local0"	},
		{   LOG_LOCAL1, "local1"	},
		{   LOG_LOCAL2, "local2"	},
		{   LOG_LOCAL3, "local3"	},
		{   LOG_LOCAL4, "local4"	},
		{   LOG_LOCAL5, "local5"	},
		{   LOG_LOCAL6, "local6"	},
		{   LOG_LOCAL7, "local7"	},
		{   0,		  NULL		}
	};
	for(i = 0; facTbl[i].name != NULL; i++) {
		if(!strcasecmp(name, facTbl[i].name))
			return facTbl[i].facility;
	}
	return LOG_DAEMON;
}
