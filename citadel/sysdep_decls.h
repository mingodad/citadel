/* $Id$ */

#ifndef SYSDEP_DECLS_H
#define SYSDEP_DECLS_H

/*
 * Uncomment this #define if you are a Citadel developer tracking
 * down memory leaks in the server.  Do NOT do this on a production
 * system because it definitely incurs a lot of additional overhead.
#define DEBUG_MEMORY_LEAKS
 */


#include <pthread.h>
#include "sysdep.h"
#include "server.h"


/* Logging levels - correspond to syslog(3) */
enum LogLevel {
	/* When about to exit the server for an unrecoverable error */
	 CTDL_EMERG,	/* system is unusable */
	/* Manual intervention is required to avoid an abnormal exit */
	 CTDL_ALERT,	/* action must be taken immediately */
	/* The server can continue to run with degraded functionality */
	 CTDL_CRIT,	/* critical conditions */
	/* An error occurs but the server continues to run normally */
	 CTDL_ERR,	/* error conditions */
	/* An abnormal condition was detected; server will continue normally */
	 CTDL_WARNING,	/* warning conditions */
	/* Normal messages (login/out, activity, etc.) */
	 CTDL_NOTICE,	/* normal but significant condition */
	/* Unimportant progress messages, etc. */
	 CTDL_INFO,	/* informational */
	/* Debugging messages */
	 CTDL_DEBUG	/* debug-level messages */
};

#ifdef __GNUC__
void lprintf (enum LogLevel loglevel, const char *format, ...) __attribute__((__format__(__printf__,2,3)));
void cprintf (const char *format, ...) __attribute__((__format__(__printf__,1,2)));
#else
void lprintf (enum LogLevel loglevel, const char *format, ...);
void cprintf (const char *format, ...);
#endif

extern pthread_key_t MyConKey;			/* TSD key for MyContext() */

extern int enable_syslog;

void init_sysdep (void);
void begin_critical_section (int which_one);
void end_critical_section (int which_one);
int ig_tcp_server (char *ip_addr, int port_number, int queue_len,char **errormessage);
int ig_uds_server(char *sockpath, int queue_len, char **errormessage);
struct CitContext *MyContext (void);
struct CitContext *CreateNewContext (void);
void InitMyContext (struct CitContext *con);
void buffer_output(void);
void unbuffer_output(void);
void flush_output(void);
void client_write (char *buf, int nbytes);
int client_read_to (char *buf, int bytes, int timeout);
int client_read (char *buf, int bytes);
int client_getln (char *buf, int maxbytes);
void sysdep_master_cleanup (void);
void kill_session (int session_to_kill);
void *sd_context_loop (struct CitContext *con);
void start_daemon (int do_close_stdio);
void cmd_nset (char *cmdbuf);
int convert_login (char *NameToConvert);
void *worker_thread (void *arg);
void become_session(struct CitContext *which_con);
void InitializeMasterCC(void);
void init_master_fdset(void);
void create_worker(void);
void InitialiseSemaphores(void);

extern int num_sessions;
extern volatile int time_to_die;
extern volatile int shutdown_and_halt;
extern volatile int running_as_daemon;
extern volatile int restart_server;

extern int verbosity;
extern int rescan[];

extern struct worker_node {
        pthread_t tid;
        struct worker_node *next;
} *worker_list;

extern int SyslogFacility(char *name);
extern int syslog_facility;

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

void create_maintenance_threads(void);

#endif /* SYSDEP_DECLS_H */
