/*
 * $Id: $
 *
 */

struct citxmpp {			/* Information about the current session */
	XML_Parser xp;			/* XML parser instance for incoming client stream */
	char server_name[256];		/* who they think we are */
	char *chardata;
	int chardata_len;
	int chardata_alloc;
	char client_jid[256];		/* "full JID" of the client */

	char iq_bind_id[256];		/* for <iq> stanzas */
	char iq_client_resource[256];	/* resource name requested by the client */
	int iq_session;			/* nonzero == client is requesting a session */

	char sasl_auth_mech[32];	/* SASL auth mechanism requested by the client */
};

#define XMPP ((struct citxmpp *)CC->session_specific_data)

void xmpp_cleanup_function(void);
void xmpp_greeting(void);
void xmpp_command_loop(void);
void xmpp_sasl_auth(char *, char *);
void xmpp_output_auth_mechs(void);
