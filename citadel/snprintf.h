#ifndef __DECC

#ifdef __GNUC__
int snprintf (char *buf, size_t max, const char *fmt, ...) __attribute__((__format__(__printf__,3,4)));
int vsnprintf (char *buf, size_t max, const char *fmt, va_list argp) __attribute__((__format__(__printf__,3,0)));
#else
int snprintf (char *buf, size_t max, const char *fmt, ...);
int vsnprintf (char *buf, size_t max, const char *fmt, va_list argp);
#endif

#endif
