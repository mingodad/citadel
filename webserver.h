/* $Id$ */
int client_gets(int sock, char *buf);
int client_read(int sock, char *buf, int bytes);
int client_read_to(int sock, char *buf, int bytes, int timeout);
ssize_t client_write(const void *buf, size_t count);
int lprintf(int loglevel, const char *format, ...);
