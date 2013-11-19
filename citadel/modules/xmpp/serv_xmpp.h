/*
 * Copyright (c) 2007-2009 by Art Cancro
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *  
 *  
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  
 *  
 *  
 */

#include "xmpp_xmacros.h"
#include "xmpp_util.h"


typedef struct _citxmpp {			/* Information about the current session */
	StrBuf *OutBuf;
	XML_Parser xp;			/* XML parser instance for incoming client stream */
	char server_name[256];		/* who they think we are */
	char *chardata;
	int chardata_len;
	int chardata_alloc;
	char client_jid[256];		/* "full JID" of the client */
	int last_event_processed;

	TheToken_iq IQ;

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
	int ping_requested;		/* In this stanza, client is pinging the server. */
} citxmpp;

#define XMPP ((citxmpp *)CC->session_specific_data)

struct xmpp_event {
	struct xmpp_event *next;
	int event_seq;
	time_t event_time;
	int event_type;
	char event_jid[256];
	int session_which_generated_this_event;
};

extern int queue_event_seq;

enum {
	XMPP_EVT_LOGIN,
	XMPP_EVT_LOGOUT
};


typedef void (*xmpp_handler_func)(void *data, const char *supplied_el, const char **attr);

typedef struct __xmpp_handler {
	int               Flags;
	xmpp_handler_func Handler;
}xmpp_handler;


void xmpp_cleanup_function(void);
void xmpp_greeting(void);
void xmpp_command_loop(void);
void xmpp_async_loop(void);
void xmpp_sasl_auth(char *, char *);
void xmpp_output_auth_mechs(void);
void xmpp_query_namespace(TheToken_iq *iq, char *);
void xmpp_output_incoming_messages(void);
void xmpp_queue_event(int, char *);
void xmpp_process_events(void);
void xmpp_presence_notify(char *, int);
void xmpp_roster_item(struct CitContext *);
void xmpp_send_message(char *, char *);
void xmpp_non_sasl_authenticate(StrBuf *IQ_id, char *, char *, char *);
void xmpp_massacre_roster(void);
void xmpp_delete_old_buddies_who_no_longer_exist_from_the_client_roster(void);
int xmpp_is_visible(struct CitContext *from, struct CitContext *to_whom);
char *xmlesc(char *buf, char *str, int bufsiz);

extern int XMPPSrvDebugEnable;

#define DBGLOG(LEVEL) if ((LEVEL != LOG_DEBUG) || (XMPPSrvDebugEnable != 0))

#define XMPP_syslog(LEVEL, FORMAT, ...)				\
	DBGLOG(LEVEL) syslog(LEVEL,				\
			     "XMPP: " FORMAT, __VA_ARGS__)

#define XMPPM_syslog(LEVEL, FORMAT)		\
	DBGLOG(LEVEL) syslog(LEVEL,		\
			     "XMPP: " FORMAT);


void AddXMPPStartHandler(const char *key,
			 long len,
			 xmpp_handler_func Handler,
			 int Flags);

void AddXMPPEndHandler(const char *key,
		       long len,
		       xmpp_handler_func Handler,
		       int Flags);




