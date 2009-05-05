/*
 * $Id$
 */

/*
 * Uncomment this to log all communications with the Citadel server
#define SERV_TRACE 1
 */


#include "webcit.h"
#include "webserver.h"

/*
 *  register the timeout
 *  signum signalhandler number
 * \return signals
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
	fdflags = fdflags | O_NONBLOCK;
	fcntl(s, F_SETFD, fdflags);
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
	int bytes_written = 0;
	int retval;
	while (bytes_written < nbytes) {
		retval = write(WC->serv_sock, &buf[bytes_written],
			       nbytes - bytes_written);
		if (retval < 1) {
			lprintf(1, "Server connection broken: %s\n",
				strerror(errno));
			close(WC->serv_sock);
			WC->serv_sock = (-1);
			WC->connected = 0;
			WC->logged_in = 0;
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

