/*
 * $Id$
 *
 */

struct citxmpp {			/* Information about the current session */
	XML_Parser xp;			/* XML parser instance for incoming client stream */
	char server_name[256];		/* who they think we are */
	char *chardata;
	int chardata_len;
	int chardata_alloc;
	char client_jid[256];		/* "full JID" of the client */
	int last_event_processed;

	char iq_type[256];		/* for <iq> stanzas */
	char iq_id[256];
	char iq_from[256];
	char iq_to[256];
	char iq_client_username[256];	/* username requested by the client (NON SASL ONLY) */
	char iq_client_password[256];	/* password requested by the client (NON SASL ONLY) */
	char iq_client_resource[256];	/* resource name requested by the client */
	int iq_session;			/* nonzero == client is requesting a session */
	char iq_query_xmlns[256];	/* Namespace of <query> */

	char sasl_auth_mech[32];	/* SASL auth mechanism requested by the client */

	char message_to[256];
	char *message_body;		/* Message body in transit */
	int html_tag_level;		/* <html> tag nesting level */

	int bind_requested;		/* In this stanza, client is asking server to bind a resource. */
};

#define XMPP ((struct citxmpp *)CC->session_specific_data)

struct xmpp_event {
	struct xmpp_event *next;
	int event_seq;
	time_t event_time;
	int event_type;
	char event_jid[256];
	int session_which_generated_this_event;
};

extern struct xmpp_event *xmpp_queue;
extern int queue_event_seq;

enum {
	XMPP_EVT_LOGIN,
	XMPP_EVT_LOGOUT
};

void xmpp_cleanup_function(void);
void xmpp_greeting(void);
void xmpp_command_loop(void);
void xmpp_async_loop(void);
void xmpp_sasl_auth(char *, char *);
void xmpp_output_auth_mechs(void);
void xmpp_query_namespace(char *, char *, char *, char *);
void jabber_wholist_presence_dump(void);
void jabber_output_incoming_messages(void);
void xmpp_queue_event(int, char *);
void xmpp_process_events(void);
void xmpp_presence_notify(char *, int);
void jabber_roster_item(struct CitContext *);
void jabber_send_message(char *, char *);
void jabber_non_sasl_authenticate(char *, char *, char *, char *);
