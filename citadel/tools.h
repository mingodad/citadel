/* $Id$ */
char *safestrncpy(char *dest, const char *src, size_t n);
int num_tokens (char *source, char tok);
void extract_token(char *dest, char *source, int parmnum, char separator);
int extract_int (char *source, int parmnum);
long int extract_long (char *source, long int parmnum);
void encode_base64(char *dest, char *source);
int decode_base64(char *dest, char *source, size_t length);
void striplt(char *);
int haschar(const char *st, int ch);
int collapsed_strcmp(char *s1, char *s2);
void remove_token(char *source, int parmnum, char separator);
void fmt_date(char *buf, time_t thetime, int seconds);
int is_msg_in_mset(char *mset, long msgnum);
char *memreadline(char *start, char *buf, int maxlen);

#ifndef HAVE_STRNCASECMP
int strncasecmp(char *, char *, int)
#endif
#ifndef HAVE_STRCASECMP
#define strcasecmp(x,y) strncasecmp(x,y,INT_MAX);
#endif

#define extract(dest,source,parmnum)	extract_token(dest,source,parmnum,'|')
#define num_parms(source)		num_tokens(source, '|')
void stripout(char *str, char leftboundary, char rightboundary);
void stripallbut(char *str, char leftboundary, char rightboundary);
