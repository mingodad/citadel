/*
 * $Id$
 *
 * Header file for TCP client socket library
 */

int sock_connect(char *host, char *service, char *protocol);
int sock_read(int sock, char *buf, int bytes);
int sock_write(int sock, *buf, int nbytes);
int *sock_gets(int sock, *buf);
int sock_puts(int sock, *buf);

/* 
 * This looks dumb, but it's being done for future portability
 */
#define sock_close(sock)	close(sock)
