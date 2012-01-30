/*
 * Header file for TCP client socket library
 *
 * Copyright (c) 1987-2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

int sock_connect(char *host, char *service);
int sock_write(int *sock, const char *buf, int nbytes);
int sock_write_timeout(int *sock, const char *buf, int nbytes, int timeout);
int ml_sock_gets(int *sock, char *buf, int nSec);
int sock_getln(int *sock, char *buf, int bufsize);
int CtdlSockGetLine(int *sock, StrBuf *Target, int nSec);
int sock_puts(int *sock, char *buf);
int socket_read_blob(int *Socket, StrBuf * Target, int bytes, int timeout);


/* 
 * This looks dumb, but it's being done for future portability
 */
#define sock_close(sock)		close(sock)
#define sock_shutdown(sock, how)	shutdown(sock, how)

/* 
 * Default timeout for client sessions
 */
#define CLIENT_TIMEOUT		600
