/* $Id$ */
void attach_to_server(int argc, char **argv, char *hostbuf, char *portbuf);
extern int server_is_local;
int getsockfd(void);
char serv_getc(void);
int is_connected(void);
