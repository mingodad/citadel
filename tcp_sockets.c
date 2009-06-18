/*
 * $Id$
 */

/*
 * Uncomment this to log all communications with the Citadel server
#define SERV_TRACE 1
 */


#include "webcit.h"
#include "webserver.h"

extern int DisableGzip;

/*
 * register the timeout
 */
RETSIGTYPE timeout(int signum)
{
	lprintf(1, "Connection timed out; unable to reach citserver\n");
	/* no exit here, since we need to server the connection unreachable thing. exit(3); */
}


/*
 *  Connect a unix domain socket
 *  sockpath where to open a unix domain socket
 */
int uds_connectsock(char *sockpath)
{
	struct sockaddr_un addr;
	int s;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sockpath, sizeof addr.sun_path);

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0) {
		lprintf(1, "Can't create socket[%s]: %s\n",
			sockpath,
			strerror(errno));
		return(-1);
	}

	if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		lprintf(1, "Can't connect [%s]: %s\n",
			sockpath,
			strerror(errno));
		close(s);
		return(-1);
	}

	return s;
}


/*
 *  Connect a TCP/IP socket
 *  host the host to connect to
 *  service the service on the host to call
 */
int tcp_connectsock(char *host, char *service)
{
        int fdflags;
	struct hostent *phe;
	struct servent *pse;
	struct protoent *ppe;
	struct sockaddr_in sin;
	int s;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;

	pse = getservbyname(service, "tcp");
	if (pse) {
		sin.sin_port = pse->s_port;
	} else if ((sin.sin_port = htons((u_short) atoi(service))) == 0) {
		lprintf(1, "Can't get %s service entry\n", service);
		return (-1);
	}
	phe = gethostbyname(host);
	if (phe) {
		memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);
	} else if ((sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE) {
		lprintf(1, "Can't get %s host entry: %s\n",
			host, strerror(errno));
		return (-1);
	}
	if ((ppe = getprotobyname("tcp")) == 0) {
		lprintf(1, "Can't get TCP protocol entry: %s\n",
			strerror(errno));
		return (-1);
	}

	s = socket(PF_INET, SOCK_STREAM, ppe->p_proto);
	if (s < 0) {
		lprintf(1, "Can't create socket: %s\n", strerror(errno));
		return (-1);
	}

	fdflags = fcntl(s, F_GETFL);
	if (fdflags < 0)
		lprintf(1, "unable to get socket flags!  %s.%s: %s \n",
			host, service, strerror(errno));
	fdflags = fdflags | O_NONBLOCK;
	if (fcntl(s, F_SETFD, fdflags) < 0)
		lprintf(1, "unable to set socket nonblocking flags!  %s.%s: %s \n",
			host, service, strerror(errno));

	signal(SIGALRM, timeout);
	alarm(30);

	if (connect(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		lprintf(1, "Can't connect to %s.%s: %s\n",
			host, service, strerror(errno));
		close(s);
		return (-1);
	}
	alarm(0);
	signal(SIGALRM, SIG_IGN);

	fdflags = fcntl(s, F_GETFL);
	if (fdflags < 0)
		lprintf(1, "unable to get socket flags!  %s.%s: %s \n",
			host, service, strerror(errno));
	fdflags = fdflags | O_NONBLOCK;
	if (fcntl(s, F_SETFD, fdflags) < 0)
		lprintf(1, "unable to set socket nonblocking flags!  %s.%s: %s \n",
			host, service, strerror(errno));
	return (s);
}



/*
 *  input string from pipe
 */
int serv_getln(char *strbuf, int bufsize)
{
	wcsession *WCC = WC;
	int len;

	*strbuf = '\0';
	StrBuf_ServGetln(WCC->MigrateReadLineBuf);
	len = StrLength(WCC->MigrateReadLineBuf);
	if (len > bufsize)
		len = bufsize - 1;
	memcpy(strbuf, ChrPtr(WCC->MigrateReadLineBuf), len);
	FlushStrBuf(WCC->MigrateReadLineBuf);
	strbuf[len] = '\0';
#ifdef SERV_TRACE
	lprintf(9, "%3d>%s\n", WC->serv_sock, strbuf);
#endif
	return len;
}


int StrBuf_ServGetln(StrBuf *buf)
{
	wcsession *WCC = WC;
	const char *ErrStr = NULL;
	int rc;

	rc = StrBufTCP_read_buffered_line_fast(buf, 
					       WCC->ReadBuf, 
					       &WCC->ReadPos, 
					       &WCC->serv_sock, 
					       5, 1, 
					       &ErrStr);
	if (rc < 0)
	{
		lprintf(1, "Server connection broken: %s\n",
			ErrStr);
		wc_backtrace();
		WCC->serv_sock = (-1);
		WCC->connected = 0;
		WCC->logged_in = 0;
	}
	return rc;
}

int StrBuf_ServGetBLOBBuffered(StrBuf *buf, long BlobSize)
{
	wcsession *WCC = WC;
	const char *Err;
	int rc;
	
	rc = StrBufReadBLOBBuffered(buf, 
				    WCC->ReadBuf, 
				    &WCC->ReadPos,
				    &WCC->serv_sock, 
				    1, 
				    BlobSize, 
				    NNN_TERM,
				    &Err);
	if (rc < 0)
	{
		lprintf(1, "Server connection broken: %s\n",
			Err);
		wc_backtrace();
		WCC->serv_sock = (-1);
		WCC->connected = 0;
		WCC->logged_in = 0;
	}
	return rc;
}

int StrBuf_ServGetBLOB(StrBuf *buf, long BlobSize)
{
	wcsession *WCC = WC;
	const char *Err;
	int rc;
	
	WCC->ReadPos = NULL;
	rc = StrBufReadBLOB(buf, &WCC->serv_sock, 1, BlobSize, &Err);
	if (rc < 0)
	{
		lprintf(1, "Server connection broken: %s\n",
			Err);
		wc_backtrace();
		WCC->serv_sock = (-1);
		WCC->connected = 0;
		WCC->logged_in = 0;
	}
	return rc;
}

/*
 *  send binary to server
 *  buf the buffer to write to citadel server
 *  nbytes how many bytes to send to citadel server
 */
void serv_write(const char *buf, int nbytes)
{
	wcsession *WCC = WC;
	int bytes_written = 0;
	int retval;

	FlushStrBuf(WCC->ReadBuf);
	WCC->ReadPos = NULL;
	while (bytes_written < nbytes) {
		retval = write(WCC->serv_sock, &buf[bytes_written],
			       nbytes - bytes_written);
		if (retval < 1) {
			lprintf(1, "Server connection broken: %s\n",
				strerror(errno));
			close(WCC->serv_sock);
			WCC->serv_sock = (-1);
			WCC->connected = 0;
			WCC->logged_in = 0;
			return;
		}
		bytes_written = bytes_written + retval;
	}
}


/*
 *  send line to server
 *  string the line to send to the citadel server
 */
void serv_puts(const char *string)
{
	wcsession *WCC = WC;
#ifdef SERV_TRACE
	lprintf(9, "%3d<%s\n", WC->serv_sock, string);
#endif
	FlushStrBuf(WCC->ReadBuf);
	WCC->ReadPos = NULL;

	serv_write(string, strlen(string));
	serv_write("\n", 1);
}

/*
 *  send line to server
 *  string the line to send to the citadel server
 */
void serv_putbuf(const StrBuf *string)
{
	wcsession *WCC = WC;
#ifdef SERV_TRACE
	lprintf(9, "%3d<%s\n", WC->serv_sock, ChrPtr(string));
#endif
	FlushStrBuf(WCC->ReadBuf);
	WCC->ReadPos = NULL;

	serv_write(ChrPtr(string), StrLength(string));
	serv_write("\n", 1);
}


/*
 *  convenience function to send stuff to the server
 *  format the formatstring
 *  ... the entities to insert into format 
 */
void serv_printf(const char *format,...)
{
	wcsession *WCC = WC;
	va_list arg_ptr;
	char buf[SIZ];
	size_t len;

	FlushStrBuf(WCC->ReadBuf);
	WCC->ReadPos = NULL;

	va_start(arg_ptr, format);
	vsnprintf(buf, sizeof buf, format, arg_ptr);
	va_end(arg_ptr);

	len = strlen(buf);
	buf[len++] = '\n';
	buf[len] = '\0';
	serv_write(buf, len);
#ifdef SERV_TRACE
	lprintf(9, "<%s", buf);
#endif
}




int ClientGetLine(ParsedHttpHdrs *Hdr, StrBuf *Target)
{
	const char *Error, *pch, *pchs;
	int rlen, len, retval = 0;

#ifdef HAVE_OPENSSL
	if (is_https) {
		int ntries = 0;
		if (StrLength(Hdr->ReadBuf) > 0) {
			pchs = ChrPtr(Hdr->ReadBuf);
			pch = strchr(pchs, '\n');
			if (pch != NULL) {
				rlen = 0;
				len = pch - pchs;
				if (len > 0 && (*(pch - 1) == '\r') )
					rlen ++;
				StrBufSub(Target, Hdr->ReadBuf, 0, len - rlen);
				StrBufCutLeft(Hdr->ReadBuf, len + 1);
				return len - rlen;
			}
		}

		while (retval == 0) { 
				pch = NULL;
				pchs = ChrPtr(Hdr->ReadBuf);
				if (*pchs != '\0')
					pch = strchr(pchs, '\n');
				if (pch == NULL) {
					retval = client_read_sslbuffer(Hdr->ReadBuf, SLEEPING);
					pchs = ChrPtr(Hdr->ReadBuf);
					pch = strchr(pchs, '\n');
				}
				if (retval == 0) {
					sleeeeeeeeeep(1);
					ntries ++;
				}
				if (ntries > 10)
					return 0;
		}
		if ((retval > 0) && (pch != NULL)) {
			rlen = 0;
			len = pch - pchs;
			if (len > 0 && (*(pch - 1) == '\r') )
				rlen ++;
			StrBufSub(Target, Hdr->ReadBuf, 0, len - rlen);
			StrBufCutLeft(Hdr->ReadBuf, len + 1);
			return len - rlen;

		}
		else 
			return -1;
	}
	else 
#endif
		return StrBufTCP_read_buffered_line_fast(Target, 
							 Hdr->ReadBuf,
							 &Hdr->Pos,
							 &Hdr->http_sock,
							 5,
							 1,
							 &Error);
}

/* 
 * This is a generic function to set up a master socket for listening on
 * a TCP port.  The server shuts down if the bind fails.
 *
 * ip_addr 	IP address to bind
 * port_number	port number to bind
 * queue_len	number of incoming connections to allow in the queue
 */
int ig_tcp_server(char *ip_addr, int port_number, int queue_len)
{
	struct protoent *p;
	struct sockaddr_in sin;
	int s, i;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	if (ip_addr == NULL) {
		sin.sin_addr.s_addr = INADDR_ANY;
	} else {
		sin.sin_addr.s_addr = inet_addr(ip_addr);
	}

	if (sin.sin_addr.s_addr == INADDR_NONE) {
		sin.sin_addr.s_addr = INADDR_ANY;
	}

	if (port_number == 0) {
		lprintf(1, "Cannot start: no port number specified.\n");
		exit(WC_EXIT_BIND);
	}
	sin.sin_port = htons((u_short) port_number);

	p = getprotobyname("tcp");

	s = socket(PF_INET, SOCK_STREAM, (p->p_proto));
	if (s < 0) {
		lprintf(1, "Can't create a socket: %s\n", strerror(errno));
		exit(WC_EXIT_BIND);
	}
	/* Set some socket options that make sense. */
	i = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));

	#ifndef __APPLE__
	fcntl(s, F_SETFL, O_NONBLOCK); /* maide: this statement is incorrect
					  there should be a preceding F_GETFL
					  and a bitwise OR with the previous
					  fd flags */
	#endif
	
	if (bind(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		lprintf(1, "Can't bind: %s\n", strerror(errno));
		exit(WC_EXIT_BIND);
	}
	if (listen(s, queue_len) < 0) {
		lprintf(1, "Can't listen: %s\n", strerror(errno));
		exit(WC_EXIT_BIND);
	}
	return (s);
}



/*
 * Create a Unix domain socket and listen on it
 * sockpath - file name of the unix domain socket
 * queue_len - Number of incoming connections to allow in the queue
 */
int ig_uds_server(char *sockpath, int queue_len)
{
	struct sockaddr_un addr;
	int s;
	int i;
	int actual_queue_len;

	actual_queue_len = queue_len;
	if (actual_queue_len < 5) actual_queue_len = 5;

	i = unlink(sockpath);
	if ((i != 0) && (errno != ENOENT)) {
		lprintf(1, "webcit: can't unlink %s: %s\n",
			sockpath, strerror(errno));
		exit(WC_EXIT_BIND);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	safestrncpy(addr.sun_path, sockpath, sizeof addr.sun_path);

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0) {
		lprintf(1, "webcit: Can't create a socket: %s\n",
			strerror(errno));
		exit(WC_EXIT_BIND);
	}

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		lprintf(1, "webcit: Can't bind: %s\n",
			strerror(errno));
		exit(WC_EXIT_BIND);
	}

	if (listen(s, actual_queue_len) < 0) {
		lprintf(1, "webcit: Can't listen: %s\n",
			strerror(errno));
		exit(WC_EXIT_BIND);
	}

	chmod(sockpath, 0777);
	return(s);
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
int client_read_to(ParsedHttpHdrs *Hdr, StrBuf *Target, int bytes, int timeout)
{
	const char *Error;
	int retval = 0;

#ifdef HAVE_OPENSSL
	if (is_https) {
		long bufremain;

		if (Hdr->Pos == NULL)
			Hdr->Pos = ChrPtr(Hdr->ReadBuf);
		bufremain = StrLength(Hdr->ReadBuf) - (Hdr->Pos - ChrPtr(Hdr->ReadBuf));

		if (bytes < bufremain)
			bufremain = bytes;
		StrBufAppendBufPlain(Target, Hdr->Pos, bufremain, 0);
		StrBufCutLeft(Hdr->ReadBuf, bufremain);

		if (bytes > bufremain) 
		{
			while ((StrLength(Hdr->ReadBuf) + StrLength(Target) < bytes) &&
			       (retval >= 0))
				retval = client_read_sslbuffer(Hdr->ReadBuf, timeout);
			if (retval >= 0) {
				StrBufAppendBuf(Target, Hdr->ReadBuf, 0); /* todo: Buf > bytes? */
#ifdef HTTP_TRACING
				write(2, "\033[32m", 5);
				write(2, buf, bytes);
				write(2, "\033[30m", 5);
#endif
				return 1;
			}
			else {
				lprintf(2, "client_read_ssl() failed\n");
				return -1;
			}
		}
		else 
			return 1;
	}
#endif

	retval = StrBufReadBLOBBuffered(Target, 
					Hdr->ReadBuf, 
					&Hdr->Pos, 
					&Hdr->http_sock, 
					1, 
					bytes,
					O_TERM,
					&Error);
	if (retval < 0) {
		lprintf(2, "client_read() failed: %s\n",
			Error);
		return retval;
	}

#ifdef HTTP_TRACING
	write(2, "\033[32m", 5);
	write(2, buf, bytes);
	write(2, "\033[30m", 5);
#endif
	return 1;
}


/*
 * Begin buffering HTTP output so we can transmit it all in one write operation later.
 */
void begin_burst(void)
{
	if (WC->WBuf == NULL) {
		WC->WBuf = NewStrBufPlain(NULL, 32768);
	}
}


/*
 * Finish buffering HTTP output.  [Compress using zlib and] output with a Content-Length: header.
 */
long end_burst(void)
{
	wcsession *WCC = WC;
        const char *ptr, *eptr;
        long count;
	ssize_t res = 0;
        fd_set wset;
        int fdflags;

	if (!DisableGzip && (WCC->Hdr->HR.gzip_ok) && CompressBuffer(WCC->WBuf))
	{
		hprintf("Content-encoding: gzip\r\n");
	}

	if (WCC->Hdr->HR.prohibit_caching)
		hprintf("Pragma: no-cache\r\nCache-Control: no-store\r\nExpires:-1\r\n");
	hprintf("Content-length: %d\r\n\r\n", StrLength(WCC->WBuf));

	ptr = ChrPtr(WCC->HBuf);
	count = StrLength(WCC->HBuf);
	eptr = ptr + count;

#ifdef HAVE_OPENSSL
	if (is_https) {
		client_write_ssl(WCC->HBuf);
		client_write_ssl(WCC->WBuf);
		return (count);
	}
#endif

	
#ifdef HTTP_TRACING
	
	write(2, "\033[34m", 5);
	write(2, ptr, StrLength(WCC->WBuf));
	write(2, "\033[30m", 5);
#endif
	if (WCC->Hdr->http_sock == -1)
		return -1;
	fdflags = fcntl(WC->Hdr->http_sock, F_GETFL);

	while ((ptr < eptr) && (WCC->Hdr->http_sock != -1)){
                if ((fdflags & O_NONBLOCK) == O_NONBLOCK) {
                        FD_ZERO(&wset);
                        FD_SET(WCC->Hdr->http_sock, &wset);
                        if (select(WCC->Hdr->http_sock + 1, NULL, &wset, NULL, NULL) == -1) {
                                lprintf(2, "client_write: Socket select failed (%s)\n", strerror(errno));
                                return -1;
                        }
                }

                if ((WCC->Hdr->http_sock == -1) || 
		    (res = write(WCC->Hdr->http_sock, 
				 ptr,
				 count)) == -1) {
                        lprintf(2, "client_write: Socket write failed (%s)\n", strerror(errno));
		        wc_backtrace();
                        return res;
                }
                count -= res;
		ptr += res;
        }

	ptr = ChrPtr(WCC->WBuf);
	count = StrLength(WCC->WBuf);
	eptr = ptr + count;

#ifdef HTTP_TRACING
	
	write(2, "\033[34m", 5);
	write(2, ptr, StrLength(WCC->WBuf));
	write(2, "\033[30m", 5);
#endif

        while ((ptr < eptr) && (WCC->Hdr->http_sock != -1)) {
                if ((fdflags & O_NONBLOCK) == O_NONBLOCK) {
                        FD_ZERO(&wset);
                        FD_SET(WCC->Hdr->http_sock, &wset);
                        if (select(WCC->Hdr->http_sock + 1, NULL, &wset, NULL, NULL) == -1) {
                                lprintf(2, "client_write: Socket select failed (%s)\n", strerror(errno));
                                return -1;
                        }
                }

                if ((WCC->Hdr->http_sock == -1) || 
		    (res = write(WCC->Hdr->http_sock, 
				 ptr,
				 count)) == -1) {
                        lprintf(2, "client_write: Socket write failed (%s)\n", strerror(errno));
			wc_backtrace();
                        return res;
                }
                count -= res;
		ptr += res;
        }

	return StrLength(WCC->WBuf);
}


/*
 * lingering_close() a`la Apache. see
 * http://www.apache.org/docs/misc/fin_wait_2.html for rationale
 */
int lingering_close(int fd)
{
	char buf[SIZ];
	int i;
	fd_set set;
	struct timeval tv, start;

	gettimeofday(&start, NULL);
	if (fd == -1)
		return -1;
	shutdown(fd, 1);
	do {
		do {
			gettimeofday(&tv, NULL);
			tv.tv_sec = SLEEPING - (tv.tv_sec - start.tv_sec);
			tv.tv_usec = start.tv_usec - tv.tv_usec;
			if (tv.tv_usec < 0) {
				tv.tv_sec--;
				tv.tv_usec += 1000000;
			}
			FD_ZERO(&set);
			FD_SET(fd, &set);
			i = select(fd + 1, &set, NULL, NULL, &tv);
		} while (i == -1 && errno == EINTR);

		if (i <= 0)
			break;

		i = read(fd, buf, sizeof buf);
	} while (i != 0 && (i != -1 || errno == EINTR));

	return close(fd);
}

void
HttpNewModule_TCPSOCKETS
(ParsedHttpHdrs *httpreq)
{

	httpreq->ReadBuf = NewStrBufPlain(NULL, SIZ * 4);
}

void
HttpDetachModule_TCPSOCKETS
(ParsedHttpHdrs *httpreq)
{

	FlushStrBuf(httpreq->ReadBuf);
	ReAdjustEmptyBuf(httpreq->ReadBuf, 4 * SIZ, SIZ);
}

void
HttpDestroyModule_TCPSOCKETS
(ParsedHttpHdrs *httpreq)
{

	FreeStrBuf(&httpreq->ReadBuf);
}


void
SessionNewModule_TCPSOCKETS
(wcsession *sess)
{
	sess->CLineBuf = NewStrBuf();
	sess->MigrateReadLineBuf = NewStrBuf();
}

void 
SessionDestroyModule_TCPSOCKETS
(wcsession *sess)
{
	FreeStrBuf(&sess->CLineBuf);
	FreeStrBuf(&sess->ReadBuf);
	FreeStrBuf(&sess->MigrateReadLineBuf);
	if (sess->serv_sock > 0)
		close(sess->serv_sock);
}
