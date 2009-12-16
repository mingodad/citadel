/* $Id: sysdep_decls.h 7265 2009-03-25 23:18:46Z dothebart $ */

#ifndef CONTEXT_H
#define CONTEXT_H

#include <stdarg.h>
#include "sysdep.h"
#include "server.h"
#include "sysdep_decls.h"
#include "threads.h"


/*
 * Here's the big one... the Citadel context structure.
 *
 * This structure keeps track of all information relating to a running 
 * session on the server.  We keep one of these for each session thread.
 *
 */
struct CitContext {
	struct CitContext *prev;	/* Link to previous session in list */
	struct CitContext *next;	/* Link to next session in the list */

	int state;		/* thread state (see CON_ values below) */
	int kill_me;		/* Set to nonzero to flag for termination */
	int client_socket;
	int cs_pid;		/* session ID */
	int dont_term;          /* for special activities like artv so we don't get killed */
	time_t lastcmd;		/* time of last command executed */
	time_t lastidle;	/* For computing idle time */

	char curr_user[USERNAME_SIZE];	/* name of current user */
	int logged_in;		/* logged in */
	int internal_pgm;	/* authenticated as internal program */
	int nologin;		/* not allowed to log in */
	int is_local_socket;	/* set to 1 if client is on unix domain sock */
	int curr_view;		/* The view type for the current user/room */
	int is_master;		/* Is this session logged in using the master user? */

	char net_node[32]	;/* Is the client another Citadel server? */
	time_t previous_login;	/* Date/time of previous login */
	char lastcmdname[5];	/* name of last command executed */
	unsigned cs_flags;	/* miscellaneous flags */
	void (*h_command_function) (void) ;	/* service command function */
	void (*h_async_function) (void) ;	/* do async msgs function */
	int is_async;		/* Nonzero if client accepts async msgs */
	int async_waiting;	/* Nonzero if there are async msgs waiting */
	int input_waiting;	/* Nonzero if there is client input waiting */
	int can_receive_im;	/* Session is capable of receiving instant messages */

	/* Client information */
	int cs_clientdev;	/* client developer ID */
	int cs_clienttyp;	/* client type code */
	int cs_clientver;	/* client version number */
	uid_t cs_UDSclientUID;  /* the uid of the client when talking via UDS */
	char cs_clientname[32];	/* name of client software */
	char cs_host[64];	/* host logged in from */
	char cs_addr[64];	/* address logged in from */

	/* The Internet type of thing */
	char cs_inet_email[128];		/* Return address of outbound Internet mail */
	char cs_inet_other_emails[1024];	/* User's other valid Internet email addresses */
	char cs_inet_fn[128];			/* Friendly-name of outbound Internet mail */

	FILE *download_fp;	/* Fields relating to file transfer */
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

	/* Beginning of cryptography - session nonce */
	char cs_nonce[NONCE_SIZE];	/* The nonce for this session's next auth transaction */

	/* Redirect this session's output to a memory buffer? */
	char *redirect_buffer;		/* the buffer */
	size_t redirect_len;		/* length of data in buffer */
	size_t redirect_alloc;		/* length of allocated buffer */
#ifdef HAVE_OPENSSL
	SSL *ssl;
	int redirect_ssl;
#endif

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
	char *session_specific_data;		/* Used by individual protocol modules */
	struct cit_ical *CIT_ICAL;		/* calendaring data */
	struct ma_info *ma;			/* multipart/alternative data */
	const char *ServiceName;		/* readable purpose of this session */
	void *openid_data;			/* Data stored by the OpenID module */
	char *ldap_dn;				/* DN of user when using AUTHMODE_LDAP */
};

typedef struct CitContext CitContext;

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
enum {
	CON_IDLE,		/* This context is doing nothing */
	CON_READY,		/* This context needs attention */
	CON_EXECUTING		/* This context is bound to a thread */
};

#define CC MyContext()


extern citthread_key_t MyConKey;			/* TSD key for MyContext() */
extern int num_sessions;
extern CitContext masterCC;
extern CitContext *ContextList;

CitContext *MyContext (void);
void RemoveContext (struct CitContext *);
CitContext *CreateNewContext (void);
void context_cleanup(void);
void kill_session (int session_to_kill);
INLINE void become_session(struct CitContext *which_con);
void InitializeMasterCC(void);
void dead_session_purge(int force);

/* Deprecated, user CtdlBumpNewMailCounter() instead */
void BumpNewMailCounter(long) __attribute__ ((deprecated));

void terminate_idle_sessions(void);
int CtdlTerminateOtherSession (int session_num);
/* bits returned by CtdlTerminateOtherSession */
#define TERM_FOUND	0x01
#define TERM_ALLOWED	0x02
#define TERM_KILLED	0x03
#define TERM_NOTALLOWED -1
#endif /* CONTEXT_H */
