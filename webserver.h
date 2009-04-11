/* $Id$ */

extern char *static_dirs[PATH_MAX];          /**< Web representation */
extern char *static_content_dirs[PATH_MAX];  /**< Disk representation */
extern int ndirs;
extern char socket_dir[PATH_MAX];

int ClientGetLine(int *sock, StrBuf *Target, StrBuf *CLineBuf, const char **Pos);
int client_getln(int *sock, char *buf, int bufsiz);
int client_read(int *sock, StrBuf *Target, StrBuf *buf, int bytes);
int client_read_to(int *sock, StrBuf *Target, StrBuf *Buf, int bytes, int timeout);
int lprintf(int loglevel, const char *format, ...);
void wc_backtrace(void);
void ShutDownWebcit(void);
void shutdown_ssl(void);
