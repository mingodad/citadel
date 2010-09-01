
#ifndef SYSDEP_DECLS_H
#define SYSDEP_DECLS_H

/*
 * Uncomment this #define if you are a Citadel developer tracking
 * down memory leaks in the server.  Do NOT do this on a production
 * system because it definitely incurs a lot of additional overhead.
#define DEBUG_MEMORY_LEAKS
 */


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

void cputbuf(const StrBuf *Buf);

#ifdef __GNUC__
void cprintf (const char *format, ...) __attribute__((__format__(__printf__,1,2)));
#else
void cprintf (const char *format, ...);
#endif

void CtdlLogPrintf(enum LogLevel loglevel, const char *format, ...);
void vCtdlLogPrintf (enum LogLevel loglevel, const char *format, va_list arg_ptr);

extern int enable_syslog;
extern int print_to_logfile;

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
void sysdep_master_cleanup (void);
void kill_session (int session_to_kill);
void start_daemon (int do_close_stdio);
void checkcrash(void);
void cmd_nset (char *cmdbuf);
int convert_login (char *NameToConvert);
void *worker_thread (void *arg);
void init_master_fdset(void);
void create_worker(void);
void *select_on_master (void *arg);

extern volatile int exit_signal;
extern volatile int shutdown_and_halt;
extern volatile int running_as_daemon;
extern volatile int restart_server;

extern int verbosity;
extern int rescan[];




extern int SyslogFacility(char *name);
extern int syslog_facility;


/*
 * Typdefs and stuff to abstract pthread for Citadel
 */
#ifdef HAVE_PTHREAD_H

typedef pthread_t	citthread_t;
typedef pthread_key_t	citthread_key_t;
typedef pthread_mutex_t	citthread_mutex_t;
typedef pthread_cond_t	citthread_cond_t;
typedef pthread_attr_t	citthread_attr_t;


#define citthread_mutex_init	pthread_mutex_init
#define citthread_cond_init	pthread_cond_init
#define citthread_attr_init	pthread_attr_init
#define citthread_mutex_trylock	pthread_mutex_trylock
#define citthread_mutex_lock	pthread_mutex_lock
#define citthread_mutex_unlock	pthread_mutex_unlock
#define citthread_key_create	pthread_key_create
#define citthread_getspecific	pthread_getspecific
#define citthread_setspecific	pthread_setspecific
#define citthread_mutex_destroy	pthread_mutex_destroy
#define citthread_cond_destroy	pthread_cond_destroy
#define citthread_attr_destroy	pthread_attr_destroy

#define citthread_kill		pthread_kill
#define citthread_cond_signal	pthread_cond_signal
#define citthread_cancel	pthread_cancel
#define citthread_cond_timedwait	pthread_cond_timedwait
#define citthread_equal		pthread_equal
#define citthread_self		pthread_self
#define citthread_create	pthread_create
#define citthread_attr_setstacksize	pthread_attr_setstacksize
#define citthread_join		pthread_join
#define citthread_cleanup_push	pthread_cleanup_push
#define citthread_cleanup_pop	pthread_cleanup_pop


#endif /* HAVE_PTHREAD_H */


#ifdef DEBUG_MEMORY_LEAKS
#define malloc(x) tracked_malloc(x, __FILE__, __LINE__)
#define realloc(x,y) tracked_realloc(x, y, __FILE__, __LINE__)
#undef strdup
#define strdup(x) tracked_strdup(x, __FILE__, __LINE__)
#define free(x) tracked_free(x)
void *tracked_malloc(size_t size, char *file, int line);
void *tracked_realloc(void *ptr, size_t size, char *file, int line);
void tracked_free(void *ptr);
char *tracked_strdup(const char *s, char *file, int line);
void dump_heap(void);
#endif

#endif /* SYSDEP_DECLS_H */
