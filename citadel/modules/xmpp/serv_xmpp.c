/*
 * XMPP (Jabber) service for the Citadel system
 * Copyright (c) 2007-2011 by Art Cancro
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#define SHOW_ME_VAPPEND_PRINTF
#include <libcitadel.h>
#include <expat.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "user_ops.h"
#include "database.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "md5.h"
#include "ctdl_module.h"
#include "serv_xmpp.h"

/* XML_StopParser is present in expat 2.x */
#if XML_MAJOR_VERSION > 1
#define HAVE_XML_STOPPARSER
#endif

struct xmpp_event *xmpp_queue = NULL;
HashList *XMPP_StartHandlers = NULL;
HashList *XMPP_EndHandlers = NULL;

int XMPPSrvDebugEnable = 0;

void XUnbuffer(void)
{
	citxmpp *Xmpp = XMPP;

	cputbuf(Xmpp->OutBuf);
	FlushStrBuf(Xmpp->OutBuf);
}

void XPutBody(const char *Str, long Len)
{
	StrBufXMLEscAppend(XMPP->OutBuf, NULL, Str, Len, 0);
}

void XPutProp(const char *Str, long Len)
{
	StrEscAppend(XMPP->OutBuf, NULL, Str, 0, 1);
}

void XPut(const char *Str, long Len)
{
	StrBufAppendBufPlain(XMPP->OutBuf, Str, Len, 0);
}
#define XPUT(CONSTSTR) XPut(CONSTSTR, sizeof(CONSTSTR) -1)

void XPrintf(const char *Format, ...)
{
        va_list arg_ptr;
        va_start(arg_ptr, Format);
	StrBufVAppendPrintf(XMPP->OutBuf, Format, arg_ptr);
	va_end(arg_ptr);
}


#ifdef HAVE_XML_STOPPARSER
/* Stop the parser if an entity declaration is hit. */
static void xmpp_entity_declaration(void *userData, const XML_Char *entityName,
				int is_parameter_entity, const XML_Char *value,
				int value_length, const XML_Char *base,
				const XML_Char *systemId, const XML_Char *publicId,
				const XML_Char *notationName
) {
	XMPPM_syslog(LOG_WARNING, "Illegal entity declaration encountered; stopping parser.");
	XML_StopParser(XMPP->xp, XML_FALSE);
}
#endif

/*
 * We have just received a <stream> tag from the client, so send them ours
 */
void xmpp_stream_start(void *data, const char *supplied_el, const char **attr)
{
	citxmpp *Xmpp = XMPP;

	while (*attr) {
		if (!strcasecmp(attr[0], "to")) {
			safestrncpy(Xmpp->server_name, attr[1], sizeof Xmpp->server_name);
		}
		attr += 2;
	}

	XPUT("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");

   	XPUT("<stream:stream ");
	XPUT("from=\"");
	XPutProp(Xmpp->server_name, strlen(Xmpp->server_name));
	XPUT("\" id=\"");
	XPrintf("%08x\" ", CC->cs_pid);
	XPUT("version=\"1.0\" "
		  "xmlns:stream=\"http://etherx.jabber.org/streams\" "
		  "xmlns=\"jabber:client\">");

	/* The features of this stream are... */
	XPUT("<stream:features>");

	/*
	 * TLS encryption (but only if it isn't already active)
	 */ 
#ifdef HAVE_OPENSSL
	if (!CC->redirect_ssl) {
		XPUT("<starttls xmlns='urn:ietf:params:xml:ns:xmpp-tls'></starttls>");
	}
#endif

	if (!CC->logged_in) {
		/* If we're not logged in yet, offer SASL as our feature set */
		xmpp_output_auth_mechs();

		/* Also offer non-SASL authentication */
		XPUT("<auth xmlns=\"http://jabber.org/features/iq-auth\"/>");
	}

	/* Offer binding and sessions as part of our feature set */
	XPUT("<bind xmlns=\"urn:ietf:params:xml:ns:xmpp-bind\"/>"
		  "<session xmlns=\"urn:ietf:params:xml:ns:xmpp-session\"/>"
		  "</stream:features>");

	CC->is_async = 1;		/* XMPP sessions are inherently async-capable */
}

void xmpp_start_query(void *data, const char *supplied_el, const char **attr)
{
	XMPP->iq_query_xmlns[0] = 0;
	safestrncpy(XMPP->iq_query_xmlns, supplied_el, sizeof XMPP->iq_query_xmlns);
}

void xmpp_start_bind(void *data, const char *supplied_el, const char **attr)
{
	XMPP->bind_requested = 1;
}

void xmpp_start_iq(void *data, const char *supplied_el, const char **attr)
{
	int i;
	for (i=0; attr[i] != NULL; i+=2) {
		if (!strcasecmp(attr[i], "type")) {
			safestrncpy(XMPP->iq_type, attr[i+1], sizeof XMPP->iq_type);
		}
		else if (!strcasecmp(attr[i], "id")) {
			safestrncpy(XMPP->iq_id, attr[i+1], sizeof XMPP->iq_id);
		}
		else if (!strcasecmp(attr[i], "from")) {
			safestrncpy(XMPP->iq_from, attr[i+1], sizeof XMPP->iq_from);
		}
		else if (!strcasecmp(attr[i], "to")) {
			safestrncpy(XMPP->iq_to, attr[i+1], sizeof XMPP->iq_to);
		}
	}
}

void xmpp_start_auth(void *data, const char *supplied_el, const char **attr)
{
	int i;

	XMPP->sasl_auth_mech[0] = 0;
	for (i=0; attr[i] != NULL; i+=2) {
		if (!strcasecmp(attr[i], "mechanism")) {
			safestrncpy(XMPP->sasl_auth_mech, attr[i+1], sizeof XMPP->sasl_auth_mech);
		}
	}
}

void xmpp_start_message(void *data, const char *supplied_el, const char **attr)
{
	int i;

	for (i=0; attr[i] != NULL; i+=2) {
		if (!strcasecmp(attr[i], "to")) {
			safestrncpy(XMPP->message_to, attr[i+1], sizeof XMPP->message_to);
		}
	}
}

void xmpp_start_html(void *data, const char *supplied_el, const char **attr)
{
	++XMPP->html_tag_level;
}

void xmpp_xml_start(void *data, const char *supplied_el, const char **attr)
{
	char el[256];
	long newlen;
	long len;
	char *sep = NULL;
	void *pv;
	
	/* Axe the namespace, we don't care about it */
	newlen = len = safestrncpy(el, supplied_el, sizeof el);
	while (sep = strchr(el, ':'), sep)
	{
		newlen -= ++sep - el;
		memmove(el, sep, newlen + 1);
		len = newlen;
	}

	/*
	XMPP_syslog(LOG_DEBUG, "XMPP ELEMENT START: <%s>\n", el);
	for (i=0; attr[i] != NULL; i+=2) {
		XMPP_syslog(LOG_DEBUG, "                    Attribute '%s' = '%s'\n", attr[i], attr[i+1]);
	}
	uncomment for more verbosity */

	if (GetHash(XMPP_StartHandlers, el, len, &pv))
	{
		xmpp_handler *h;
		h = (xmpp_handler*) pv;
		h->Handler(data, supplied_el, attr);
	}
	XUnbuffer();
}

void xmpp_end_resource(void *data, const char *supplied_el, const char **attr)
{
	if (XMPP->chardata_len > 0) {
		safestrncpy(XMPP->iq_client_resource, XMPP->chardata,
			    sizeof XMPP->iq_client_resource);
		striplt(XMPP->iq_client_resource);
	}
}

void xmpp_end_username(void *data, const char *supplied_el, const char **attr)
{
	/* NON SASL ONLY */
	if (XMPP->chardata_len > 0) {
		safestrncpy(XMPP->iq_client_username, XMPP->chardata,
			    sizeof XMPP->iq_client_username);
		striplt(XMPP->iq_client_username);
	}
}

void xmpp_end_password(void *data, const char *supplied_el, const char **attr)
{		/* NON SASL ONLY */
	if (XMPP->chardata_len > 0) {
		safestrncpy(XMPP->iq_client_password, XMPP->chardata,
			    sizeof XMPP->iq_client_password);
		striplt(XMPP->iq_client_password);
	}
}

void xmpp_end_iq(void *data, const char *supplied_el, const char **attr)
{
	citxmpp *Xmpp = XMPP;

	/*
	 * iq type="get" (handle queries)
	 */
	if (!strcasecmp(Xmpp->iq_type, "get"))
	{
		/*
		 * Query on a namespace
		 */
		if (!IsEmptyStr(Xmpp->iq_query_xmlns)) {
			xmpp_query_namespace(Xmpp->iq_id, Xmpp->iq_from,
					     Xmpp->iq_to, Xmpp->iq_query_xmlns);
		}
		
		/*
		 * ping ( http://xmpp.org/extensions/xep-0199.html )
		 */
		else if (Xmpp->ping_requested) {
			XPUT("<iq type=\"result\" ");
			if (!IsEmptyStr(Xmpp->iq_from)) {
				XPUT("to=\"");
				XPutProp(Xmpp->iq_from, strlen(Xmpp->iq_from));
				XPUT("\" ");
			}
			if (!IsEmptyStr(Xmpp->iq_to)) {
				XPUT("from=\"");
				XPutProp(Xmpp->iq_to, strlen(Xmpp->iq_to));
				XPUT("\" ");
			}
			XPUT("id=\"");
			XPutProp(Xmpp->iq_id, strlen(Xmpp->iq_id));
			XPUT("\"/>");
		}

		/*
		 * Unknown query ... return the XML equivalent of a blank stare
		 */
		else {
/*
			Xmpp_syslog(LOG_DEBUG,
				    "Unknown query <%s> - returning <service-unavailable/>\n",
				    el
				);
*/
			XPUT("<iq type=\"error\" id=\"");
			XPutProp(Xmpp->iq_id, strlen(Xmpp->iq_id));
			XPUT("\">"
			     "<error code=\"503\" type=\"cancel\">"
			     "<service-unavailable xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
			     "</error>"
			     "</iq>");
		}
	}

	/*
	 * Non SASL authentication
	 */
	else if (
		(!strcasecmp(Xmpp->iq_type, "set"))
		&& (!strcasecmp(Xmpp->iq_query_xmlns, "jabber:iq:auth:query"))
		) {
		
		xmpp_non_sasl_authenticate(
			Xmpp->iq_id,
			Xmpp->iq_client_username,
			Xmpp->iq_client_password,
			Xmpp->iq_client_resource
			);
	}

	/*
	 * If this <iq> stanza was a "bind" attempt, process it ...
	 */
	else if (
		(Xmpp->bind_requested)
		&& (!IsEmptyStr(Xmpp->iq_id))
		&& (!IsEmptyStr(Xmpp->iq_client_resource))
		&& (CC->logged_in)
		) {

		/* Generate the "full JID" of the client resource */

		snprintf(Xmpp->client_jid, sizeof Xmpp->client_jid,
			 "%s/%s",
			 CC->cs_inet_email,
			 Xmpp->iq_client_resource
			);

		/* Tell the client what its JID is */

		XPUT("<iq type=\"result\" id=\"");
		XPutProp(Xmpp->iq_id, strlen(Xmpp->iq_id));
		XPUT("\">"
		     "<bind xmlns=\"urn:ietf:params:xml:ns:xmpp-bind\">");
		XPUT("<jid>");
		XPutBody(Xmpp->client_jid, strlen(Xmpp->client_jid));
		XPUT("</jid>"
		     "</bind>"
		     "</iq>");
	}

	else if (Xmpp->iq_session) {
		XPUT("<iq type=\"result\" id=\"");
		XPutProp(Xmpp->iq_id, strlen(Xmpp->iq_id));
		XPUT("\">"
		     "</iq>");
	}

	else {
		XPUT("<iq type=\"error\" id=\"");
		XPutProp(Xmpp->iq_id, strlen(Xmpp->iq_id));
		XPUT("\">");
		XPUT("<error>Don't know howto do '");
		XPutBody(Xmpp->iq_type, strlen(Xmpp->iq_type));
		XPUT("'!</error>"
		     "</iq>");
	}

	/* Now clear these fields out so they don't get used by a future stanza */
	Xmpp->iq_id[0] = 0;
	Xmpp->iq_from[0] = 0;
	Xmpp->iq_to[0] = 0;
	Xmpp->iq_type[0] = 0;
	Xmpp->iq_client_resource[0] = 0;
	Xmpp->iq_session = 0;
	Xmpp->iq_query_xmlns[0] = 0;
	Xmpp->bind_requested = 0;
	Xmpp->ping_requested = 0;
}


void xmpp_end_auth(void *data, const char *supplied_el, const char **attr)
{
	/* Try to authenticate (this function is responsible for the output stanza) */
	xmpp_sasl_auth(XMPP->sasl_auth_mech, (XMPP->chardata != NULL ? XMPP->chardata : "") );
	
	/* Now clear these fields out so they don't get used by a future stanza */
	XMPP->sasl_auth_mech[0] = 0;
}

void xmpp_end_session(void *data, const char *supplied_el, const char **attr)
{
	XMPP->iq_session = 1;
}

void xmpp_end_body(void *data, const char *supplied_el, const char **attr)
{
	if (XMPP->html_tag_level == 0)
	{
		if (XMPP->message_body != NULL)
		{
			free(XMPP->message_body);
			XMPP->message_body = NULL;
		}
		if (XMPP->chardata_len > 0) {
			XMPP->message_body = strdup(XMPP->chardata);
		}
	}
}

void xmpp_end_html(void *data, const char *supplied_el, const char **attr)
{
	--XMPP->html_tag_level;
}

void xmpp_end_starttls(void *data, const char *supplied_el, const char **attr)
{
#ifdef HAVE_OPENSSL
	XPUT("<proceed xmlns='urn:ietf:params:xml:ns:xmpp-tls'/>");
	CtdlModuleStartCryptoMsgs(NULL, NULL, NULL);
	if (!CC->redirect_ssl) CC->kill_me = KILLME_NO_CRYPTO;
#else
	XPUT("<failure xmlns='urn:ietf:params:xml:ns:xmpp-tls'/>");
	CC->kill_me = KILLME_NO_CRYPTO;
#endif
}

void xmpp_end_ping(void *data, const char *supplied_el, const char **attr)
{
	XMPP->ping_requested = 1;
}

void xmpp_end_stream(void *data, const char *supplied_el, const char **attr)
{
	XMPPM_syslog(LOG_DEBUG, "XMPP client shut down their stream\n");
	xmpp_massacre_roster();
	XPUT("</stream>\n");
	CC->kill_me = KILLME_CLIENT_LOGGED_OUT;
}

void xmpp_xml_end(void *data, const char *supplied_el)
{
	char el[256];
	long len;
	long newlen;
	char *sep = NULL;
	void *pv;

	/* Axe the namespace, we don't care about it */
	newlen = len = safestrncpy(el, supplied_el, sizeof el);
	while (sep = strchr(el, ':'), sep) {

		newlen -= ++sep - el;
		memmove(el, sep, newlen + 1);
		len = newlen;
	}

	/*
	XMPP_syslog(LOG_DEBUG, "XMPP ELEMENT END  : <%s>\n", el);
	if (XMPP->chardata_len > 0) {
		XMPP_syslog(LOG_DEBUG, "          chardata: %s\n", XMPP->chardata);
	}
	uncomment for more verbosity */

	if (GetHash(XMPP_EndHandlers, el, len, &pv))
	{
		xmpp_handler *h;
		h = (xmpp_handler*) pv;
		h->Handler(data, supplied_el, NULL);
	}
	else
	{
		XMPP_syslog(LOG_DEBUG, "Ignoring unknown tag <%s>\n", el);
	}

	XMPP->chardata_len = 0;
	if (XMPP->chardata_alloc > 0) {
		XMPP->chardata[0] = 0;
	}
	XUnbuffer();
}


void xmpp_xml_chardata(void *data, const XML_Char *s, int len)
{
	citxmpp *X = XMPP;

	if (X->chardata_alloc == 0) {
		X->chardata_alloc = SIZ;
		X->chardata = malloc(X->chardata_alloc);
	}
	if ((X->chardata_len + len + 1) > X->chardata_alloc) {
		X->chardata_alloc = X->chardata_len + len + 1024;
		X->chardata = realloc(X->chardata, X->chardata_alloc);
	}
	memcpy(&X->chardata[X->chardata_len], s, len);
	X->chardata_len += len;
	X->chardata[X->chardata_len] = 0;
}


/*
 * This cleanup function blows away the temporary memory and files used by the XMPP service.
 */
void xmpp_cleanup_function(void) {

	/* Don't do this stuff if this is not a XMPP session! */
	if (CC->h_command_function != xmpp_command_loop) return;

	if (XMPP->chardata != NULL) {
		free(XMPP->chardata);
		XMPP->chardata = NULL;
		XMPP->chardata_len = 0;
		XMPP->chardata_alloc = 0;
		if (XMPP->message_body != NULL) {
			free(XMPP->message_body);
		}
	}
	XML_ParserFree(XMPP->xp);
	free(XMPP);
}



/*
 * Here's where our XMPP session begins its happy day.
 */
void xmpp_greeting(void) {
	client_set_inbound_buf(4);
	strcpy(CC->cs_clientname, "XMPP session");
	CC->session_specific_data = malloc(sizeof(citxmpp));
	memset(XMPP, 0, sizeof(citxmpp));
	XMPP->last_event_processed = queue_event_seq;

	/* XMPP does not use a greeting, but we still have to initialize some things. */

	XMPP->xp = XML_ParserCreateNS("UTF-8", ':');
	if (XMPP->xp == NULL) {
		XMPPM_syslog(LOG_ALERT, "Cannot create XML parser!\n");
		CC->kill_me = KILLME_XML_PARSER;
		return;
	}

	XML_SetElementHandler(XMPP->xp, xmpp_xml_start, xmpp_xml_end);
	XML_SetCharacterDataHandler(XMPP->xp, xmpp_xml_chardata);
	// XML_SetUserData(XMPP->xp, something...);

	/* Prevent the "billion laughs" attack against expat by disabling
	 * internal entity expansion.  With 2.x, forcibly stop the parser
	 * if an entity is declared - this is safer and a more obvious
	 * failure mode.  With older versions, simply prevent expansion
	 * of such entities. */
#ifdef HAVE_XML_STOPPARSER
	XML_SetEntityDeclHandler(XMPP->xp, xmpp_entity_declaration);
#else
	XML_SetDefaultHandler(XMPP->xp, NULL);
#endif

	CC->can_receive_im = 1;		/* This protocol is capable of receiving instant messages */
}


/* 
 * Main command loop for XMPP sessions.
 */
void xmpp_command_loop(void) {
	int rc;
	StrBuf *stream_input = NewStrBuf();

	time(&CC->lastcmd);
	rc = client_read_random_blob(stream_input, 30);
	if (rc > 0) {
		XML_Parse(XMPP->xp, ChrPtr(stream_input), rc, 0);
	}
	else {
		XMPPM_syslog(LOG_ERR, "client disconnected: ending session.\n");
		CC->kill_me = KILLME_CLIENT_DISCONNECTED;
	}
	FreeStrBuf(&stream_input);
}


/*
 * Async loop for XMPP sessions (handles the transmission of unsolicited stanzas)
 */
void xmpp_async_loop(void) {
	xmpp_process_events();
	xmpp_output_incoming_messages();
}


/*
 * Login hook for XMPP sessions
 */
void xmpp_login_hook(void) {
	xmpp_queue_event(XMPP_EVT_LOGIN, CC->cs_inet_email);
}


/*
 * Logout hook for XMPP sessions
 */
void xmpp_logout_hook(void) {
	xmpp_queue_event(XMPP_EVT_LOGOUT, CC->cs_inet_email);
}


void LogXMPPSrvDebugEnable(const int n)
{
	XMPPSrvDebugEnable = n;
}
const char *CitadelServiceXMPP="XMPP";
extern void xmpp_cleanup_events(void);



/******************************************************************************
 *                    XMPP handler registering logic                           *
 ******************************************************************************/

void AddXMPPStartHandler(const char *key,
			 long len,
			 xmpp_handler_func Handler,
			 int Flags)
{
	xmpp_handler *h;
	h = (xmpp_handler*) malloc(sizeof (xmpp_handler));
	h->Flags = Flags;
	h->Handler = Handler;
	Put(XMPP_StartHandlers, key, len, h, NULL);
}

void AddXMPPEndHandler(const char *key,
		       long len,
		       xmpp_handler_func Handler,
		       int Flags)
{
	xmpp_handler *h;
	h = (xmpp_handler*) malloc(sizeof (xmpp_handler));
	h->Flags = Flags;
	h->Handler = Handler;
	Put(XMPP_EndHandlers, key, len, h, NULL);
}

CTDL_MODULE_INIT(xmpp)
{
	if (!threading) {
		CtdlRegisterServiceHook(config.c_xmpp_c2s_port,
					NULL,
					xmpp_greeting,
					xmpp_command_loop,
					xmpp_async_loop,
					CitadelServiceXMPP);


		XMPP_StartHandlers = NewHash(1, NULL);
		XMPP_EndHandlers = NewHash(1, NULL);

		AddXMPPEndHandler(HKEY("resource"),	 xmpp_end_resource, 0);
		AddXMPPEndHandler(HKEY("username"),	 xmpp_end_username, 0);
		AddXMPPEndHandler(HKEY("password"),	 xmpp_end_password, 0);
		AddXMPPEndHandler(HKEY("iq"),		 xmpp_end_iq, 0);
		AddXMPPEndHandler(HKEY("auth"),		 xmpp_end_auth, 0);
		AddXMPPEndHandler(HKEY("session"),	 xmpp_end_session, 0);
		AddXMPPEndHandler(HKEY("body"),		 xmpp_end_body, 0);
		AddXMPPEndHandler(HKEY("html"),		 xmpp_end_html, 0);
		AddXMPPEndHandler(HKEY("starttls"),	 xmpp_end_starttls, 0);
		AddXMPPEndHandler(HKEY("ping"),		 xmpp_end_ping, 0);
		AddXMPPEndHandler(HKEY("stream"),	 xmpp_end_stream, 0);

		AddXMPPStartHandler(HKEY("stream"),	xmpp_stream_start, 0);
		AddXMPPStartHandler(HKEY("query"),	xmpp_start_query, 0);
		AddXMPPStartHandler(HKEY("bind"),	xmpp_start_bind, 0);
		AddXMPPStartHandler(HKEY("iq"),		xmpp_start_iq, 0);
		AddXMPPStartHandler(HKEY("auth"),	xmpp_start_auth, 0);
		AddXMPPStartHandler(HKEY("message"),	xmpp_start_message, 0);
		AddXMPPStartHandler(HKEY("html"),	xmpp_start_html, 0);


		CtdlRegisterDebugFlagHook(HKEY("serv_xmpp"), LogXMPPSrvDebugEnable, &XMPPSrvDebugEnable);
		CtdlRegisterSessionHook(xmpp_cleanup_function, EVT_STOP, PRIO_STOP + 70);
                CtdlRegisterSessionHook(xmpp_login_hook, EVT_LOGIN, PRIO_LOGIN + 90);
                CtdlRegisterSessionHook(xmpp_logout_hook, EVT_LOGOUT, PRIO_LOGOUT + 90);
                CtdlRegisterSessionHook(xmpp_login_hook, EVT_UNSTEALTH, PRIO_UNSTEALTH + 1);
                CtdlRegisterSessionHook(xmpp_logout_hook, EVT_STEALTH, PRIO_STEALTH + 1);
		CtdlRegisterCleanupHook(xmpp_cleanup_events);

	}

	/* return our module name for the log */
	return "xmpp";
}
