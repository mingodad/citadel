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

typedef struct _citxmpp {			/* Information about the current session */
	StrBuf *OutBuf;
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

extern struct xmpp_event *xmpp_queue;
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
void xmpp_query_namespace(char *, char *, char *, char *);
void xmpp_output_incoming_messages(void);
void xmpp_queue_event(int, char *);
void xmpp_process_events(void);
void xmpp_presence_notify(char *, int);
void xmpp_roster_item(struct CitContext *);
void xmpp_send_message(char *, char *);
void xmpp_non_sasl_authenticate(char *, char *, char *, char *);
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


void XUnbuffer(void);
void XPutBody(const char *Str, long Len);
void XPutProp(const char *Str, long Len);
void XPut(const char *Str, long Len);
#define XPUT(CONSTSTR) XPut(CONSTSTR, sizeof(CONSTSTR) -1)

void XPrintf(const char *Format, ...);


void AddXMPPStartHandler(const char *key,
			 long len,
			 xmpp_handler_func Handler,
			 int Flags);

void AddXMPPEndHandler(const char *key,
		       long len,
		       xmpp_handler_func Handler,
		       int Flags);


#define XCLOSED (1<<0)
void XPrint(const char *Token, long tlen,
	    int Flags,
	    ...);

#define TYPE_STR 1
#define TYPE_OPTSTR 2
#define TYPE_INT 3
#define TYPE_BODYSTR 4
#define TYPE_ARGEND 5
#define XPROPERTY(NAME, VALUE, VLEN) TYPE_STR, NAME, sizeof(NAME)-1, VALUE, VLEN
#define XOPROPERTY(NAME, VALUE, VLEN) TYPE_OPTSTR, NAME, sizeof(NAME)-1, VALUE, VLEN
#define XCPROPERTY(NAME, VALUE) TYPE_STR, NAME, sizeof(NAME)-1, VALUE, sizeof(VALUE) - 1
#define XIPROPERTY(NAME, LVALUE) TYPE_INT, NAME, SIZEOF(NAME)-1
#define XBODY(VALUE, VLEN) TYPE_BODYSTR, VALUE, VLEN
#define XCFGBODY(WHICH) TYPE_BODYSTR, config.WHICH, configlen.WHICH
