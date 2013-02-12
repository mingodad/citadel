
#ifndef CONTEXT_H
#define CONTEXT_H

#include <stdarg.h>
#include "sysdep.h"
#include "server.h"
#include "sysdep_decls.h"
#include "threads.h"


/*
 * Values for CitContext.state
 * 
 * A session that is doing nothing is in CON_IDLE state.  When activity
 * is detected on the socket, it goes to CON_READY, indicating that it
 * needs to have a worker thread bound to it.  When a thread binds to
 * the session, it goes to CON_EXECUTING and does its thing.  When the
 * transaction is finished, the thread sets it back to CON_IDLE and lets
 * it go.
 */
typedef enum __CCState {
	CON_IDLE,		/* This context is doing nothing */
	CON_GREETING,		/* This context needs to output its greeting */
	CON_STARTING,		/* This context is outputting its greeting */
	CON_READY,		/* This context needs attention */
	CON_EXECUTING,		/* This context is bound to a thread */
	CON_SYS                 /* This is a system context and mustn't be purged */
} CCState;

#ifndef __ASYNCIO__
#define __ASYNCIO__
typedef struct AsyncIO AsyncIO; /* forward declaration for event_client.h */
#endif
#ifndef __CIT_CONTEXT__
#define __CIT_CONTEXT__
typedef struct CitContext CitContext;
#endif

/*
 * Here's the big one... the Citadel context structure.
 *
 * This structure keeps track of all information relating to a running 
 * session on the server.  We keep one of these for each session thread.
 *
 */
struct CitContext {
	CitContext *prev;	/* Link to previous session in list */
	CitContext *next;	/* Link to next session in the list */

	int cs_pid;		/* session ID */
	int dont_term;		/* for special activities like artv so we don't get killed */
	double created;      /* time of birth */
	time_t lastcmd;		/* time of last command executed */
	time_t lastidle;	/* For computing idle time */
	CCState state;		/* thread state (see CON_ values below) */
	int kill_me;		/* Set to nonzero to flag for termination */

	IOBuffer SendBuf, /* Our write Buffer */
		RecvBuf, /* Our block buffered read buffer */
		SBuf; /* Our block buffered read buffer for clients */

	StrBuf *MigrateBuf;        /* Our block buffered read buffer */
	StrBuf *sMigrateBuf;        /* Our block buffered read buffer */

	int client_socket;
	int is_local_socket;	/* set to 1 if client is on unix domain sock */
	/* Redirect this session's output to a memory buffer? */
	StrBuf *redirect_buffer;		/* the buffer */
	StrBuf *StatusMessage;
#ifdef HAVE_OPENSSL
	SSL *ssl;
	int redirect_ssl;
#endif

	char curr_user[USERNAME_SIZE];	/* name of current user */
	int logged_in;		/* logged in */
	int internal_pgm;	/* authenticated as internal program */
	int nologin;		/* not allowed to log in */
	int curr_view;		/* The view type for the current user/room */
	int is_master;		/* Is this session logged in using the master user? */

	char net_node[32]	;/* Is the client another Citadel server? */
	time_t previous_login;	/* Date/time of previous login */
	char lastcmdname[5];	/* name of last command executed */
	unsigned cs_flags;	/* miscellaneous flags */
	int is_async;		/* Nonzero if client accepts async msgs */
	int async_waiting;	/* Nonzero if there are async msgs waiting */
	int input_waiting;	/* Nonzero if there is client input waiting */
	int can_receive_im;	/* Session is capable of receiving instant messages */

	/* Client information */
	int cs_clientdev;	/* client developer ID */
	int cs_clienttyp;	/* client type code */
	int cs_clientver;	/* client version number */
	char cs_clientinfo[256];/* if its a unix domain socket, some info for logging. */
	uid_t cs_UDSclientUID;  /* the uid of the client when talking via UDS */
	char cs_clientname[32];	/* name of client software */
	char cs_host[64];	/* host logged in from */
	char cs_addr[64];	/* address logged in from */

	/* The Internet type of thing */
	char cs_inet_email[128];		/* Return address of outbound Internet mail */
	char cs_inet_other_emails[1024];	/* User's other valid Internet email addresses */
	char cs_inet_fn[128];			/* Friendly-name of outbound Internet mail */

	FILE *download_fp;	/* Fields relating to file transfer */
	size_t download_fp_total;
	char download_desired_section[128];
	FILE *upload_fp;
	char upl_file[256];
	char upl_path[PATH_MAX];
	char upl_comment[256];
	char upl_filedir[PATH_MAX];
	char upl_mimetype[64];
	char dl_is_net;
	char upload_type;

	struct ctdluser user;	/* Database record buffers */
	struct ctdlroom room;

	/* A linked list of all instant messages sent to us. */
	struct ExpressMessage *FirstExpressMessage;
	int disable_exp;	/* Set to 1 to disable incoming pages */
	int newmail;		/* Other sessions increment this */

	/* Masqueraded values in the 'who is online' list */
	char fake_username[USERNAME_SIZE];
	char fake_hostname[64];
	char fake_roomname[ROOMNAMELEN];

	/* Preferred MIME formats */
	char preferred_formats[256];
	int msg4_dont_decode;

	/* Dynamically allocated session data */
	void *session_specific_data;		/* Used by individual protocol modules */
	struct cit_ical *CIT_ICAL;		/* calendaring data */
	struct ma_info *ma;			/* multipart/alternative data */
	const char *ServiceName;		/* readable purpose of this session */
	long tcp_port;
	void *openid_data;			/* Data stored by the OpenID module */
	char *ldap_dn;				/* DN of user when using AUTHMODE_LDAP */

	void (*h_command_function) (void) ;	/* service command function */
	void (*h_async_function) (void) ;	/* do async msgs function */
	void (*h_greeting_function) (void) ;	/* greeting function for session startup */

	long *cached_msglist;			/* results of the previous CtdlForEachMessage() */
	int cached_num_msgs;

	AsyncIO *IO;				/* if this session has AsyncIO going on... */
};



#define CC MyContext()


extern pthread_key_t MyConKey;			/* TSD key for MyContext() */
extern int num_sessions;
extern CitContext masterCC;
extern CitContext *ContextList;

CitContext *MyContext (void);
void RemoveContext (struct CitContext *);
CitContext *CreateNewContext (void);
void context_cleanup(void);
void kill_session (int session_to_kill);
void InitializeMasterCC(void);
void dead_session_purge(int force);
void set_async_waiting(struct CitContext *ccptr);

CitContext *CloneContext(CitContext *CloneMe);

/* forcibly close and flush fd's on shutdown */
void terminate_all_sessions(void);

/* Deprecated, user CtdlBumpNewMailCounter() instead */
void BumpNewMailCounter(long) __attribute__ ((deprecated));

void terminate_idle_sessions(void);
int CtdlTerminateOtherSession (int session_num);
/* bits returned by CtdlTerminateOtherSession */
#define TERM_FOUND	0x01
#define TERM_ALLOWED	0x02
#define TERM_KILLED	0x03
#define TERM_NOTALLOWED -1

/*
 * Bind a thread to a context.  (It's inline merely to speed things up.)
 */
static INLINE void become_session(CitContext *which_con) {
/*
	pid_t tid = syscall(SYS_gettid);
*/
	pthread_setspecific(MyConKey, (void *)which_con );
/*
	syslog(LOG_DEBUG, "[%d]: Now doing %s\n", 
		      (int) tid, 
		      ((which_con != NULL) && (which_con->ServiceName != NULL)) ? 
		      which_con->ServiceName:"");
*/
}



/* typedef void (*CtdlDbgFunction) (const int); */

extern int DebugSession;
#define CONDBGLOG(LEVEL) if ((LEVEL != LOG_DEBUG) || (DebugSession != 0))

#define CON_syslog(LEVEL, FORMAT, ...)				\
	CONDBGLOG(LEVEL) syslog(LEVEL,				\
				"Context: " FORMAT, __VA_ARGS__)

#define CONM_syslog(LEVEL, FORMAT)			\
	CONDBGLOG(LEVEL) syslog(LEVEL,			\
				"Context: " FORMAT);


#endif /* CONTEXT_H */
