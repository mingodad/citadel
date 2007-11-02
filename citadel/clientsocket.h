/*
 * $Id$
 *
 * Header file for TCP client socket library
 */

int sock_connect(char *host, char *service, char *protocol);
int sock_read_to(int sock, char *buf, int bytes, int timeout, int keep_reading_until_full);
int sock_read(int sock, char *buf, int bytes, int keep_reading_until_full);
int sock_write(int sock, char *buf, int nbytes);
int ml_sock_gets(int sock, char *buf);
int sock_getln(int sock, char *buf, int bufsize);
int sock_puts(int sock, char *buf);


/* 
 * This looks dumb, but it's being done for future portability
 */
#define sock_close(sock)		close(sock)
#define sock_shutdown(sock, how)	shutdown(sock, how)

/* 
 * Default timeout for client sessions
 */
#define CLIENT_TIMEOUT		600
