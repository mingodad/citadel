
#ifndef SYSDEP_DECLS_H
#define SYSDEP_DECLS_H

#include <stdarg.h>
#include "sysdep.h"

#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif

#ifdef HAVE_DB_H
#include <db.h>
#elif defined(HAVE_DB4_DB_H)
#include <db4/db.h>
#else
#error Neither <db.h> nor <db4/db.h> was found by configure. Install db4-devel.
#endif


#if DB_VERSION_MAJOR < 4 || DB_VERSION_MINOR < 1
#error Citadel requires Berkeley DB v4.1 or newer.  Please upgrade.
#endif

#include "server.h"
#include "database.h"

#if SIZEOF_SIZE_T == SIZEOF_INT 
#define SIZE_T_FMT "%d"
#else
#define SIZE_T_FMT "%ld"
#endif

#if SIZEOF_LOFF_T == SIZEOF_LONG 
#define LOFF_T_FMT "%ld"
#else
#define LOFF_T_FMT "%lld"
#endif

void cputbuf(const StrBuf *Buf);

#ifdef __GNUC__
void cprintf (const char *format, ...) __attribute__((__format__(__printf__,1,2)));
#else
void cprintf (const char *format, ...);
#endif

void init_sysdep (void);
int ctdl_tcp_server(char *ip_addr, int port_number, int queue_len, char *errormessage);
int ctdl_uds_server(char *sockpath, int queue_len, char *errormessage);
void buffer_output(void);
void unbuffer_output(void);
void flush_output(void);
int client_write (const char *buf, int nbytes);
int client_read_to (char *buf, int bytes, int timeout);
int client_read (char *buf, int bytes);
int client_getln (char *buf, int maxbytes);
int CtdlClientGetLine(StrBuf *Target);
int client_read_blob(StrBuf *Target, int bytes, int timeout);
void client_set_inbound_buf(long N);
int client_read_random_blob(StrBuf *Target, int timeout);
void client_close(void);
void sysdep_master_cleanup (void);
void kill_session (int session_to_kill);
void start_daemon (int do_close_stdio);
void checkcrash(void);
void cmd_nset (char *cmdbuf);
int convert_login (char *NameToConvert);
void init_master_fdset(void);
void *worker_thread(void *);

extern volatile int exit_signal;
extern volatile int shutdown_and_halt;
extern volatile int running_as_daemon;
extern volatile int restart_server;

extern int verbosity;
extern int rescan[];


extern int SyslogFacility(char *name);

#endif /* SYSDEP_DECLS_H */
