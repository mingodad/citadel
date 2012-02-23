/*
 * This file contains functions which handle ctdlsh's connection
 * to the Citadel server's admin socket.  
 * 
 * Copyright (c) 2009-2012 by the Citadel.org team.
 * This program is open source software, cheerfully made available to you under
 * the terms of the GNU General Public License version 3.
 */

#include "ctdlsh.h"


int uds_connectsock(char *sockpath)
{
	struct sockaddr_un addr;
	int s;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sockpath, sizeof addr.sun_path);

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0) {
		fprintf(stderr, "Can't create socket[%s]: %s\n", sockpath, strerror(errno));
		return(-1);
	}

	if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		fprintf(stderr, "Can't connect [%s]: %s\n", sockpath, strerror(errno));
		close(s);
		return(-1);
	}

	return s;
}


/*
 * sock_read_to() - input binary data from socket, with a settable timeout.
 * Returns the number of bytes read, or -1 for error.
 * If keep_reading_until_full is nonzero, we keep reading until we get the number of requested bytes
 */
int sock_read_to(int sock, char *buf, int bytes, int timeout, int keep_reading_until_full)
{
	int len,rlen;
	fd_set rfds;
	struct timeval tv;
	int retval;

	len = 0;
	while (len<bytes) {
		FD_ZERO(&rfds);
		FD_SET(sock, &rfds);
		tv.tv_sec = timeout;
		tv.tv_usec = 0;

		retval = select(sock+1, &rfds, NULL, NULL, &tv);

		if (FD_ISSET(sock, &rfds) == 0) {	/* timed out */
			fprintf(stderr, "sock_read() timed out.\n");
			return(-1);
		}

		rlen = read(sock, &buf[len], bytes-len);
		if (rlen<1) {
			fprintf(stderr, "sock_read() failed: %s\n",
				strerror(errno));
			return(-1);
		}
		len = len + rlen;
		if (!keep_reading_until_full) return(len);
	}
	return(bytes);
}


/*
 * sock_read() - input binary data from socket.
 * Returns the number of bytes read, or -1 for error.
 */
int sock_read(int sock, char *buf, int bytes, int keep_reading_until_full)
{
	return sock_read_to(sock, buf, bytes, 30, keep_reading_until_full);
}


/*
 * sock_write() - send binary to server.
 * Returns the number of bytes written, or -1 for error.
 */
int sock_write(int sock, char *buf, int nbytes)
{
	int bytes_written = 0;
	int retval;
	while (bytes_written < nbytes) {
		retval = write(sock, &buf[bytes_written],
			       nbytes - bytes_written);
		if (retval < 1) {
			return (-1);
		}
		bytes_written = bytes_written + retval;
	}
	return (bytes_written);
}


/*
 * Input string from socket - implemented in terms of sock_read()
 * 
 */
int sock_getln(int sock, char *buf, int bufsize)
{
	int i;

	/* Read one character at a time.
	 */
	for (i = 0;; i++) {
		if (sock_read(sock, &buf[i], 1, 1) < 0) return(-1);
		if (buf[i] == '\n' || i == (bufsize-1))
			break;
	}

	/* If we got a long line, discard characters until the newline.
	 */
	if (i == (bufsize-1))
		while (buf[i] != '\n')
			if (sock_read(sock, &buf[i], 1, 1) < 0) return(-1);

	/* Strip any trailing CR and LF characters.
	 */
	buf[i] = 0;
	while ( (i > 0)
		&& ( (buf[i - 1]==13)
		     || ( buf[i - 1]==10)) ) {
		i--;
		buf[i] = 0;
	}
	return(i);
}


/*
 * sock_puts() - send line to server - implemented in terms of sock_write()
 * Returns the number of bytes written, or -1 for error.
 */
int sock_puts(int sock, char *buf)
{
	int i, j;

	i = sock_write(sock, buf, strlen(buf));
	if (i<0) return(i);
	j = sock_write(sock, "\n", 1);
	if (j<0) return(j);
	return(i+j);
}


/*
 * Write a formatted string to the server - implemented in terms of sock_write()
 */
void sock_printf(int sock, const char *format,...)
{
	va_list arg_ptr;
	char buf[4096];
	size_t len;

	va_start(arg_ptr, format);
	vsnprintf(buf, sizeof buf, format, arg_ptr);
	va_end(arg_ptr);

	sock_write(sock, buf, strlen(buf));
}
