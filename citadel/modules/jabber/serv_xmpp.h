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

	char iq_type[256];		/* for <iq> stanzas */
	char iq_id[256];
	char iq_from[256];
	char iq_to[256];
	char iq_client_resource[256];	/* resource name requested by the client */
	int iq_session;			/* nonzero == client is requesting a session */
	char iq_query_xmlns[256];	/* Namespace of <query> */

	char sasl_auth_mech[32];	/* SASL auth mechanism requested by the client */
};

#define XMPP ((struct citxmpp *)CC->session_specific_data)

void xmpp_cleanup_function(void);
void xmpp_greeting(void);
void xmpp_command_loop(void);
void xmpp_sasl_auth(char *, char *);
void xmpp_output_auth_mechs(void);
void xmpp_query_namespace(char *, char *, char *, char *);
void jabber_wholist_presence_dump(void);
void jabber_output_incoming_messages(void);
