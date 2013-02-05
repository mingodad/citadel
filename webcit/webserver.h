
extern char *static_dirs[PATH_MAX];          /**< Web representation */
extern int ndirs;
extern char socket_dir[PATH_MAX];

extern char *default_landing_page;

int ClientGetLine(ParsedHttpHdrs *Hdr, StrBuf *Target);
int client_read_to(ParsedHttpHdrs *Hdr, StrBuf *Target, int bytes, int timeout);
void wc_backtrace(long LogLevel);
void ShutDownWebcit(void);
void shutdown_ssl(void);


