/* $Id$ */

#include <pthread.h>
#include "sysdep.h"
#include "server.h"

void lprintf (int loglevel, const char *format, ...);
void init_sysdep (void);
void begin_critical_section (int which_one);
void end_critical_section (int which_one);
int ig_tcp_server (int port_number, int queue_len);
int ig_uds_server(char *sockpath, int queue_len);
struct CitContext *MyContext (void);
struct CitContext *CreateNewContext (void);
void InitMyContext (struct CitContext *con);
void client_write (char *buf, int nbytes);
void cprintf (const char *format, ...);
int client_read_to (char *buf, int bytes, int timeout);
int client_read (char *buf, int bytes);
int client_gets (char *buf);
void sysdep_master_cleanup (void);
void kill_session (int session_to_kill);
void *sd_context_loop (struct CitContext *con);
void start_daemon (int do_close_stdio);
void cmd_nset (char *cmdbuf);
int convert_login (char *NameToConvert);
void *worker_thread (void *arg);
void become_session(struct CitContext *which_con);
void CtdlRedirectOutput(FILE *fp, int sock);
void InitializeMasterCC(void);
void init_master_fdset(void);
void create_worker(void);

extern DLEXP int num_sessions;
extern DLEXP volatile int time_to_die;
extern DLEXP int verbosity;
extern DLEXP int rescan[];
extern DLEXP pthread_t initial_thread;

extern DLEXP struct worker_node {
        pthread_t tid;
        struct worker_node *next;
} *worker_list;
