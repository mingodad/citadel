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

struct sdm_userdata {
	sieve2_context_t *sieve2_context;	/**< for libsieve's use */
	long config_msgnum;			/**< confirms that a sieve config was located */
	char config_roomname[ROOMNAMELEN];
	long lastproc;				/**< last message processed */
	struct sdm_script *first_script;
};

struct ctdl_sieve {
	char *rfc822headers;
	int actiontaken;		/* Set to 1 if the message was successfully acted upon */
	int keep;			/* Set to 1 to suppress message deletion from the inbox */
	long usernum;			/* Owner of the mailbox we're processing */
	long msgnum;			/* Message base ID of the message being processed */
	struct sdm_userdata *u;		/* Info related to the current session */
	char recp_user[256];
	char recp_node[256];
	char recp_name[256];
	char sender[256];		/* To whom shall we send reject bounces or vacation messages? */
};


/* If you change this string you will break all of your Sieve configs. */
#define CTDLSIEVECONFIGSEPARATOR	"\n-=<CtdlSieveConfigSeparator>=-\n"

extern struct RoomProcList *sieve_list;

void sieve_queue_room(struct ctdlroom *);
void perform_sieve_processing(void);

void msiv_load(struct sdm_userdata *u);
void msiv_store(struct sdm_userdata *u);
int msiv_setactive(struct sdm_userdata *u, char *script_name);
char *msiv_getscript(struct sdm_userdata *u, char *script_name);
int msiv_deletescript(struct sdm_userdata *u, char *script_name);
void msiv_putscript(struct sdm_userdata *u, char *script_name, char *script_content);
extern char *msiv_extensions;

#endif /* HAVE_LIBSIEVE */
