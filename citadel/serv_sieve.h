/*
 * $Id: $
 */


#ifdef HAVE_LIBSIEVE

#include <sieve2.h>
#include <sieve2_error.h>

struct sdm_script {
	struct sdm_script *next;
	char script_name[256];
	int script_active;
	char *script_content;
};

struct sdm_vacation {
	struct sdm_vacation *next;
	char fromaddr[256];
	time_t timestamp;
};

struct sdm_userdata {
	sieve2_context_t *sieve2_context;	/**< for libsieve's use */
	long config_msgnum;			/**< confirms that a sieve config was located */
	char config_roomname[ROOMNAMELEN];
	long lastproc;				/**< last message processed */
	struct sdm_script *first_script;
	struct sdm_vacation *first_vacation;
};

struct ctdl_sieve {
	char *rfc822headers;
	int cancel_implicit_keep;	/* Set to 1 if the message was successfully acted upon */
	int keep;			/* Set to 1 to suppress message deletion from the inbox */
	long usernum;			/* Owner of the mailbox we're processing */
	long msgnum;			/* Message base ID of the message being processed */
	struct sdm_userdata *u;		/* Info related to the current session */
	char recp_user[256];
	char recp_node[256];
	char recp_name[256];
	char sender[256];		/* To whom shall we send reject bounces or vacation messages? */
	char subject[1024];		/* Retain msg subject so we can use it in vacation messages */
	char envelope_from[1024];
	char envelope_to[1024];
};


/* If you change this string you will break all of your Sieve configs. */
#define CTDLSIEVECONFIGSEPARATOR	"\n-=<CtdlSieveConfigSeparator>=-\n"

/* Maximum time we keep vacation fromaddr records online.  This implies that a vacation
 * rule cannot exceed this amount of time.   (Any more than 30 days is a ridiculously
 * long vacation which the person probably doesn't deserve.)
 */
#define MAX_VACATION	30

extern struct RoomProcList *sieve_list;

void sieve_queue_room(struct ctdlroom *);
void perform_sieve_processing(void);

void msiv_load(struct sdm_userdata *u);
void msiv_store(struct sdm_userdata *u, int changes_made);
int msiv_setactive(struct sdm_userdata *u, char *script_name);
char *msiv_getscript(struct sdm_userdata *u, char *script_name);
int msiv_deletescript(struct sdm_userdata *u, char *script_name);
void msiv_putscript(struct sdm_userdata *u, char *script_name, char *script_content);
extern char *msiv_extensions;

#endif /* HAVE_LIBSIEVE */
