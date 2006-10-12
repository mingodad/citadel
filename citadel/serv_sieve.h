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
};

#endif /* HAVE_LIBSIEVE */



extern struct RoomProcList *sieve_list;

void sieve_queue_room(struct ctdlroom *);
void perform_sieve_processing(void);

/* If you change this string you will break all of your Sieve configs. */
#define CTDLSIEVECONFIGSEPARATOR	"\n-=<CtdlSieveConfigSeparator>=-\n"
