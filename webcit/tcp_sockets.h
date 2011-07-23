int uds_connectsock(char *);
int tcp_connectsock(char *, char *);
int serv_getln(char *strbuf, int bufsize);
int StrBuf_ServGetln(StrBuf *buf);
int GetServerStatus(StrBuf *Line, long* FullState);
int serv_puts(const char *string);

int serv_write(const char *buf, int nbytes);
int serv_putbuf(const StrBuf *string);
int serv_printf(const char *format,...)__attribute__((__format__(__printf__,1,2)));
int serv_read_binary(StrBuf *Ret, size_t total_len, StrBuf *Buf);
int StrBuf_ServGetBLOB(StrBuf *buf, long BlobSize);
int StrBuf_ServGetBLOBBuffered(StrBuf *buf, long BlobSize);
int read_server_text(StrBuf *Buf, long *nLines);

void text_to_server(char *ptr);
void text_to_server_qp(char *ptr);
void server_to_text(void);
int lingering_close(int fd);
