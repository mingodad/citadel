/*
 * mini http client
 */


#include "webcit.h"
#include "webserver.h"
#define CLIENT_TIMEOUT 15
#define sock_close(sock) close(sock)

int sock_connect(char *host, char *service, char *protocol)
{
	struct hostent *phe;
	struct servent *pse;
	struct protoent *ppe;
	struct sockaddr_in sin;
	struct sockaddr_in egress_sin;
	int s, type;

	if ((host == NULL) || IsEmptyStr(host)) 
		return(-1);
	if ((service == NULL) || IsEmptyStr(service)) 
		return(-1);
	if ((protocol == NULL) || IsEmptyStr(protocol)) 
		return(-1);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;

	pse = getservbyname(service, protocol);
	if (pse) {
		sin.sin_port = pse->s_port;
	} else if ((sin.sin_port = htons((u_short) atoi(service))) == 0) {
		lprintf(CTDL_CRIT, "Can't get %s service entry: %s\n",
			service, strerror(errno));
		return(-1);
	}
	phe = gethostbyname(host);
	if (phe) {
		memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);
	} else if ((sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE) {
		lprintf(CTDL_ERR, "Can't get %s host entry: %s\n",
			host, strerror(errno));
		return(-1);
	}
	if ((ppe = getprotobyname(protocol)) == 0) {
		lprintf(CTDL_CRIT, "Can't get %s protocol entry: %s\n",
			protocol, strerror(errno));
		return(-1);
	}
	if (!strcmp(protocol, "udp")) {
		type = SOCK_DGRAM;
	} else {
		type = SOCK_STREAM;
	}

	s = socket(PF_INET, type, ppe->p_proto);
	if (s < 0) {
		lprintf(CTDL_CRIT, "Can't create socket: %s\n", strerror(errno));
		return(-1);
	}

	/* If citserver is bound to a specific IP address on the host, make
	 * sure we use that address for outbound connections.  FIXME make this work in webcit
	 */
	memset(&egress_sin, 0, sizeof(egress_sin));
	egress_sin.sin_family = AF_INET;
	//	if (!IsEmptyStr(config.c_ip_addr)) {
		//	egress_sin.sin_addr.s_addr = inet_addr(config.c_ip_addr);
        	//	if (egress_sin.sin_addr.s_addr == !INADDR_ANY) {
                	//	egress_sin.sin_addr.s_addr = INADDR_ANY;
		//	}

		/* If this bind fails, no problem; we can still use INADDR_ANY */
		bind(s, (struct sockaddr *)&egress_sin, sizeof(egress_sin));
        //	}

	/* Now try to connect to the remote host. */
	if (connect(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		lprintf(CTDL_ERR, "Can't connect to %s:%s: %s\n",
			host, service, strerror(errno));
		close(s);
		return(-1);
	}

	return (s);
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
			lprintf(CTDL_ERR, "sock_read_to() timed out.\n");
			return(-1);
		}

		rlen = read(sock, &buf[len], bytes-len);
		if (rlen<1) {
			lprintf(CTDL_ERR, "sock_read_to() failed: %s\n",
				strerror(errno));
			return(-1);
		}
		len = len + rlen;
		if (!keep_reading_until_full) return(len);
	}
	return(bytes);
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
 * Input string from socket - implemented in terms of sock_read_to()
 * 
 */
int sock_getln(int sock, char *buf, int bufsize)
{
	int i;

	/* Read one character at a time.
	 */
	for (i = 0;; i++) {
		if (sock_read_to(sock, &buf[i], 1, CLIENT_TIMEOUT, 1) < 0) return(-1);
		if (buf[i] == '\n' || i == (bufsize-1))
			break;
	}

	/* If we got a long line, discard characters until the newline.
	 */
	if (i == (bufsize-1))
		while (buf[i] != '\n')
			if (sock_read_to(sock, &buf[i], 1, CLIENT_TIMEOUT, 1) < 0) return(-1);

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
 * sock_puts() - send line to server - implemented in terms of serv_write()
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
 * Fallback handler for fetch_http() that uses our built-in mini client
 */
int fetch_http_using_mini_client(char *url, char *target_buf, int maxbytes)
{
	char buf[1024];
	char httphost[1024];
	int httpport = 80;
	char httpurl[1024];
	int sock = (-1);
	int got_bytes = (-1);
	int redirect_count = 0;
	int total_bytes_received = 0;
	int i = 0;

	/* Parse the URL */
	snprintf(buf, (sizeof buf)-1, "%s", url);
	i = parse_url(buf, httphost, &httpport, httpurl);
	if (i == 1) {
		snprintf(buf, (sizeof buf)-1, "http://%s", url);
		i = parse_url(buf, httphost, &httpport, httpurl);
	}
	if (i == 4) {
		strcat(buf, "/");
		i = parse_url(buf, httphost, &httpport, httpurl);
	}
	if (i != 0) {
		lprintf(CTDL_ALERT, "Invalid URL: %s (%d)\n", url, i);
		return(-1);
	}

retry:	lprintf(CTDL_NOTICE, "Connecting to <%s>\n", httphost);
	sprintf(buf, "%d", httpport);
	sock = sock_connect(httphost, buf, "tcp");
	if (sock >= 0) {
		lprintf(CTDL_DEBUG, "Connected!\n");

		snprintf(buf, sizeof buf, "GET %s HTTP/1.0", httpurl);
		lprintf(CTDL_DEBUG, "<%s\n", buf);
		sock_puts(sock, buf);

		snprintf(buf, sizeof buf, "Host: %s", httphost);
		lprintf(CTDL_DEBUG, "<%s\n", buf);
		sock_puts(sock, buf);

		snprintf(buf, sizeof buf, "User-Agent: WebCit");
		lprintf(CTDL_DEBUG, "<%s\n", buf);
		sock_puts(sock, buf);

		snprintf(buf, sizeof buf, "Accept: */*");
		lprintf(CTDL_DEBUG, "<%s\n", buf);
		sock_puts(sock, buf);

		sock_puts(sock, "");

		if (sock_getln(sock, buf, sizeof buf) >= 0) {
			lprintf(CTDL_DEBUG, ">%s\n", buf);
			remove_token(buf, 0, ' ');

			/* 200 OK */
			if (buf[0] == '2') {

				while (got_bytes = sock_getln(sock, buf, sizeof buf),
				      (got_bytes >= 0 && (strcmp(buf, "")) && (strcmp(buf, "\r"))) ) {
					/* discard headers */
				}

				while (got_bytes = sock_read_to(sock, buf, sizeof buf, CLIENT_TIMEOUT, 0),
				      (got_bytes>0)  ) {

					if (total_bytes_received + got_bytes > maxbytes) {
						got_bytes = maxbytes - total_bytes_received;
					}
					if (got_bytes > 0) {
						memcpy(&target_buf[total_bytes_received], buf, got_bytes);
						total_bytes_received += got_bytes;
					}

				}
			}

			/* 30X redirect */
			else if ( (!strncmp(buf, "30", 2)) && (redirect_count < 16) ) {
			        while (got_bytes = sock_getln(sock, buf, sizeof buf),
				      (got_bytes >= 0 && (strcmp(buf, "")) && (strcmp(buf, "\r"))) ) {
					if (!strncasecmp(buf, "Location:", 9)) {
						++redirect_count;
						strcpy(buf, &buf[9]);
						striplt(buf);
						if (parse_url(buf, httphost, &httpport, httpurl) == 0) {
							sock_close(sock);
							goto retry;
						}
						else {
							lprintf(CTDL_ALERT, "Invalid URL: %s\n", buf);
						}
					}
				}
			}

		}
		sock_close(sock);
	}
	else {
		lprintf(CTDL_ERR, "Could not connect: %s\n", strerror(errno));
	}

	return total_bytes_received;
}



/*
 * Begin an HTTP fetch (returns number of bytes actually fetched, or -1 for error)
 * We first try 'curl' or 'wget' because they have more robust HTTP handling, and also
 * support HTTPS.  If neither one works, we fall back to a built in mini HTTP client.
 */
int fetch_http(char *url, char *target_buf, int maxbytes)
{
	FILE *fp;
	char cmd[1024];
	int bytes_received = 0;
	char ch;

	memset(target_buf, 0, maxbytes);

	/* First try curl */
	snprintf(cmd, sizeof cmd, "curl -L %s </dev/null 2>/dev/null", url);
	fp = popen(cmd, "r");

	/* Then try wget */
	if (!fp) {
		snprintf(cmd, sizeof cmd, "wget -q -O - %s </dev/null 2>/dev/null", url);
		fp = popen(cmd, "r");
	}

	if (fp) {
		while ( (!feof(fp)) && (bytes_received < maxbytes) ) {
			ch = fgetc(fp);
			if (ch != EOF) {
				target_buf[bytes_received++] = ch;
			}
		}
		pclose(fp);
		return bytes_received;
	}

	/* Fall back to the built-in mini handler */
	return fetch_http_using_mini_client(url, target_buf, maxbytes);
}
