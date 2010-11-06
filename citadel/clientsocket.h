/*
 * $Id$
 *
 * Header file for TCP client socket library
 *
 * Copyright (c) 1987-2009 by the citadel.org team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

int sock_connect(char *host, char *service, char *protocol);
int sock_read_to(int *sock, char *buf, int bytes, int timeout, int keep_reading_until_full);
int sock_read(int *sock, char *buf, int bytes, int keep_reading_until_full);
int sock_write(int *sock, const char *buf, int nbytes);
int ml_sock_gets(int *sock, char *buf, int nSec);
int sock_getln(int *sock, char *buf, int bufsize);
int CtdlSockGetLine(int *sock, StrBuf *Target, int nSec);
int sock_puts(int *sock, char *buf);


/* 
 * This looks dumb, but it's being done for future portability
 */
#define sock_close(sock)		close(sock)
#define sock_shutdown(sock, how)	shutdown(sock, how)

/* 
 * Default timeout for client sessions
 */
#define CLIENT_TIMEOUT		600
