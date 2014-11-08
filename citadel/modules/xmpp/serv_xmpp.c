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

HashList *XMPP_StartHandlers = NULL;
HashList *XMPP_EndHandlers = NULL;
HashList *XMPP_SupportedNamespaces = NULL;
HashList *XMPP_NameSpaces = NULL;
HashList *XMPP_EndToken = NULL;
HashList *FlatToken = NULL;

int XMPPSrvDebugEnable = 1;

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

void XPrintf(const char *Format, ...)
{
        va_list arg_ptr;
        va_start(arg_ptr, Format);
	StrBufVAppendPrintf(XMPP->OutBuf, Format, arg_ptr);
	va_end(arg_ptr);
}


void XPrint(const char *Token, long tlen,
	    int Flags,
	    ...)

{
	int BodySeen = 0;
	int ArgType;
	int Finished = 0;
	char *PName;
	long PLen;
	char *Val;
	long VLen;
        va_list arg_ptr;

	XPUT("<");
	XPut(Token, tlen);

        va_start(arg_ptr, Flags);
	while (!Finished)
	{
		ArgType = va_arg(arg_ptr, int);
		switch (ArgType)
		{
		case TYPE_STR:
			PName = va_arg(arg_ptr, char*);
			PLen  = va_arg(arg_ptr, long);
			Val   = va_arg(arg_ptr, char*);
			VLen  = va_arg(arg_ptr, long);
			XPUT(" ");
			XPut(PName, PLen);
			XPUT("=\"");
			XPutProp(Val, VLen);
			XPUT("\"");
			break;
		case TYPE_OPTSTR:
			PName = va_arg(arg_ptr, char*);
			PLen  = va_arg(arg_ptr, long);
			Val   = va_arg(arg_ptr, char*);
			VLen  = va_arg(arg_ptr, long);
			if (VLen > 0)
			{
				XPUT(" ");
				XPut(PName, PLen);
				XPUT("=\"");
				XPutProp(Val, VLen);
				XPUT("\"");
			}
			break;
		case TYPE_INT:
			PName = va_arg(arg_ptr, char*);
			PLen  = va_arg(arg_ptr, long);
			VLen  = va_arg(arg_ptr, long);
			XPUT(" ");
			XPut(PName, PLen);
			XPUT("=\"");
			XPrintf("%ld", VLen);
			XPUT("\"");
			break;
		case TYPE_BODYSTR:
			BodySeen = 1;
			XPUT(">");
			Val   = va_arg(arg_ptr, char*);
			VLen  = va_arg(arg_ptr, long);
			XPutBody(Val, VLen);
			break;
		case TYPE_ARGEND:
			Finished = 1;
			break;
		}
	}
	if (Flags == XCLOSED)
	{
		if (BodySeen)
		{
			XPUT("</");
			XPut(Token, tlen);
			XPUT(">");
		}
		else
		{
			XPUT("></");
			XPut(Token, tlen);
			XPUT(">");
		}
	}
	else
		XPUT(">");
	va_end(arg_ptr);
}

void
separate_namespace(const char *supplied_el,
		   const char **Token, long *TLen,
		   HashList **ThisNamespace)
{
	const char *pch;
	const char *pToken;
	const char *NS = NULL;
	long NSLen;
	void *pv;

	*ThisNamespace = NULL;

	pToken = supplied_el;
	pch = strchr(pToken, ':');
	while (pch != NULL)
	{
		pToken = pch;
		pch = strchr(pToken  + 1, ':');
	}

	if (*pToken == ':')
	{
		NS = supplied_el;
		NSLen = pToken - supplied_el;
		if (GetHash(XMPP_NameSpaces, NS, NSLen, &pv))
		{
			*ThisNamespace = pv;

		}
		
		pToken ++;
	}

	*TLen = strlen(pToken);
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
		  "xmlns=\"jabber:client\">"
	/* The features of this stream are... */
	     "<stream:features>");

	/*
	 * TLS encryption (but only if it isn't already active)
	 */ 
/*
#ifdef HAVE_OPENSSL
	if (!CC->redirect_ssl) {
		XPUT("<starttls xmlns='urn:ietf:params:xml:ns:xmpp-tls'></starttls>");
	}
#endif
*/
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
/*
void xmpp_start_message(void *data, const char *supplied_el, const char **attr)
{
	int i;

	for (i=0; attr[i] != NULL; i+=2) {
		if (!strcasecmp(attr[i], "to")) {
			safestrncpy(XMPP->message_to, attr[i+1], sizeof XMPP->message_to);
		}
	}
}
*/
void xmpp_start_html(void *data, const char *supplied_el, const char **attr)
{
	++XMPP->html_tag_level;
}


void xmpp_xml_start(void *data, const char *supplied_el, const char **attr)
{
	HashList *ThisNamespace;
	const char *pToken;
	long len;
	void *pv;
	
	separate_namespace(supplied_el, &pToken, &len, &ThisNamespace);

	if (ThisNamespace != NULL)
	{
		if (GetHash(ThisNamespace, pToken, len, &pv))
		{
			TokenHandler *th;
			void *value;
			long i = 0;

			th = (TokenHandler*) pv;
			value = th->GetToken();

			while (attr[i] != NULL)
			{

				if (GetHash(th->Properties, attr[i], strlen(attr[i]), &pv))
				{
					PropertyHandler* ph = pv;
					char *val;
					StrBuf **pVal;
					long len;

					len = strlen(attr[i+1]);
					val = value;
					val += ph->offset;
					pVal = (StrBuf**) val;
					if (*pVal != NULL)
						StrBufPlain(*pVal, attr[i+1], len);
					else
						*pVal = NewStrBufPlain(attr[i+1], len);
				}
				i+=2;
			}
			return;
		}

	}
	/*
	XMPP_syslog(LOG_DEBUG, "XMPP ELEMENT START: <%s>\n", el);
	for (i=0; attr[i] != NULL; i+=2) {
		XMPP_syslog(LOG_DEBUG, "                    Attribute '%s' = '%s'\n", attr[i], attr[i+1]);
	}
	uncomment for more verbosity */

	if (GetHash(XMPP_StartHandlers, pToken, len, &pv))
	{
		xmpp_handler *h;
		h = (xmpp_handler*) pv;
		h->Handler(data, supplied_el, attr);
	}
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
	/* NON SASL OY */
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
	if (!strcasecmp(ChrPtr(Xmpp->IQ.type), "get"))
	{
		/*
		 * Query on a namespace
		 */
		if (!IsEmptyStr(Xmpp->iq_query_xmlns)) {
			xmpp_query_namespace(&Xmpp->IQ, Xmpp->iq_query_xmlns);
		}
		
		/*
		 * ping ( http://xmpp.org/extensions/xep-0199.html )
		 */
		else if (Xmpp->ping_requested) {
			XPUT("<iq type=\"result\" ");
			if (StrLength(Xmpp->IQ.from) > 0) {
				XPUT("to=\"");
				XPutSProp(Xmpp->IQ.from);
				XPUT("\" ");
			}
			if (StrLength(Xmpp->IQ.to)>0) {
				XPUT("from=\"");
				XPutSProp(Xmpp->IQ.to);
				XPUT("\" ");
			}
			XPUT("id=\"");
			XPutSProp(Xmpp->IQ.id);
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
			XPutSProp(Xmpp->IQ.id);
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
		(!strcasecmp(ChrPtr(Xmpp->IQ.type), "set"))
		&& (!strcasecmp(Xmpp->iq_query_xmlns, "jabber:iq:auth:query"))
		) {
		
		xmpp_non_sasl_authenticate(
			Xmpp->IQ.id,
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
		&& (StrLength(Xmpp->IQ.id)>0)
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
		XPutSProp(Xmpp->IQ.id);
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
		XPutSProp(Xmpp->IQ.id);
		XPUT("\">"
		     "</iq>");
	}

	else {
		XPUT("<iq type=\"error\" id=\"");
		XPutSProp(Xmpp->IQ.id);
		XPUT("\">");
		XPUT("<error>Don't know howto do '");
		XPutBody(SKEY(Xmpp->IQ.type));
		XPUT("'!</error>"
		     "</iq>");
	}

	/* Now clear these fields out so they don't get used by a future stanza */
	FlushStrBuf(Xmpp->IQ.id);
	FlushStrBuf(Xmpp->IQ.from);
	FlushStrBuf(Xmpp->IQ.to);
	FlushStrBuf(Xmpp->IQ.type);
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
	XUnbuffer();
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
	HashList *ThisNamespace;
	const char *pToken;
	long len;
	void *pv;
	
	separate_namespace(supplied_el, &pToken, &len, &ThisNamespace);

	if (ThisNamespace != NULL)
	{
		if (GetHash(XMPP_EndToken, pToken, len, &pv))
		{
			TokenHandler *th;
			void *value;
			long i = 0;

			th = (TokenHandler*) pv;
			value = th->GetToken();
/*
			while (attr[i] != NULL)
			{

				if (GetHash(th->Properties, attr[i], strlen(attr[i]), &pv))
				{
					PropertyHandler* ph = pv;
					char *val;
					StrBuf **pVal;
					long len;

					len = strlen(attr[i+1]);
					val = value;
					val += ph->offset;
					pVal = (StrBuf**) val;
					if (*pVal != NULL)
						StrBufPlain(*pVal, attr[i+1], len);
					else
						*pVal = NewStrBufPlain(attr[i+1], len);
				}
				i+=2;
			}
*/

			return;
		}

	}

	/*
	XMPP_syslog(LOG_DEBUG, "XMPP ELEMENT END  : <%s>\n", el);
	if (XMPP->chardata_len > 0) {
		XMPP_syslog(LOG_DEBUG, "          chardata: %s\n", XMPP->chardata);
	}
	uncomment for more verbosity */

	if (GetHash(XMPP_EndHandlers, pToken, len, &pv))
	{
		xmpp_handler *h;
		h = (xmpp_handler*) pv;
		h->Handler(data, supplied_el, NULL);
	}
	else
	{
		XMPP_syslog(LOG_DEBUG, "Ignoring unknown tag <%s>\n", pToken);
	}

	XMPP->chardata_len = 0;
	if (XMPP->chardata_alloc > 0) {
		XMPP->chardata[0] = 0;
	}
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
	free_buf_iq(&XMPP->IQ);

	XML_ParserFree(XMPP->xp);
	FreeStrBuf(&XMPP->OutBuf);
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
	XMPP->OutBuf = NewStrBufPlain(NULL, SIZ);
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
	XUnbuffer();
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
	XUnbuffer();
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

void HFreePropertyHandler(void *FreeMe)
{
	free(FreeMe);
}

void HDeleteTokenHandler(void *FreeMe)
{
	TokenHandler *th = (TokenHandler *) FreeMe;
	DeleteHash(&th->Properties);
	free(th);
}

void XMPP_RegisterTokenProperty(const char *NS, long NSLen,
				const char *Token, long TLen,
				const char *Property, long PLen,
				const char *PayloadSubToken, long pslen,
				GetTokenDataFunc GetToken,
				long offset)
{
	void *pv;
	HashList *ThisNamespace = NULL;
	PropertyHandler *h;
	TokenHandler *th;

	const char *pNS, *pToken, *pProperty, *pPayloadSubToken;

	pToken = (Token)?Token:"";
	pNS = (NS)?NS:"";
	pProperty = (Property)?Property:"";
	pPayloadSubToken = (PayloadSubToken)?PayloadSubToken:"";

	XMPP_syslog(LOG_DEBUG,
		    "New tag: Token <%s> Namespace <%s> Property <%s> PayloadSubToken <%s>\n",
		    pToken, pNS, pProperty, pPayloadSubToken);
		
	h = (PropertyHandler*) malloc(sizeof(PropertyHandler));
	h->NameSpace = NS;
	h->NameSpaceLen = NSLen;
	h->Token = Token;
	h->TokenLen = TLen;
	h->Property = Property;
	h->PropertyLen = PLen;
	h->offset = offset;
	h->PayloadSubToken = PayloadSubToken;
	h->pslen = pslen;

	if (!GetHash(XMPP_SupportedNamespaces, NS, NSLen, &pv))
	{
		Put(XMPP_SupportedNamespaces, NS, NSLen, NewStrBufPlain(NS, NSLen), HFreeStrBuf);
	}
		
	
	if (GetHash(XMPP_NameSpaces, NS, NSLen, &pv))
	{
		ThisNamespace = pv;
	}
	else
	{
		ThisNamespace = NewHash(1, NULL);
		Put(XMPP_NameSpaces, NS, NSLen, ThisNamespace, HDeleteHash);
	}

	if (GetHash(ThisNamespace, Token, TLen, &pv))
	{
		th = pv;
	}
	else
	{
		th = (TokenHandler*) malloc (sizeof(TokenHandler));
		th->GetToken = GetToken;
		th->Properties = NewHash(1, NULL);
		Put(ThisNamespace, Token, TLen, th, HDeleteTokenHandler);
	}


	if (PLen > 0)
	{
		Put(th->Properties, Property, PLen, h, HFreePropertyHandler);
	}
	else
	{
		Put(XMPP_EndToken, PayloadSubToken, pslen, h, reference_free_handler);

	}
	/*
	if (!GetHash(FlatToken, Token, TLen, &pv))
	{
		// todo mark pv as non uniq
		Put(FlatToken, Token, TLen, ThisToken, reference_free_handler);
	}	
	*/
}

void xmpp_cleanup(void)
{
	DeleteHash(&XMPP_StartHandlers);
	DeleteHash(&XMPP_EndHandlers);
	DeleteHash(&XMPP_SupportedNamespaces);
	DeleteHash(&XMPP_NameSpaces);
	DeleteHash(&FlatToken);
}

CTDL_MODULE_INIT(xmpp)
{
	if (!threading) {
		CtdlRegisterDebugFlagHook(HKEY("serv_xmpp"), LogXMPPSrvDebugEnable, &XMPPSrvDebugEnable);

		CtdlRegisterServiceHook(config.c_xmpp_c2s_port,
					NULL,
					xmpp_greeting,
					xmpp_command_loop,
					xmpp_async_loop,
					CitadelServiceXMPP);


		XMPP_StartHandlers = NewHash(1, NULL);
		XMPP_EndHandlers = NewHash(1, NULL);
		XMPP_NameSpaces = NewHash(1, NULL);
		XMPP_SupportedNamespaces = NewHash(1, NULL);
		FlatToken = NewHash(1, NULL);

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
		AddXMPPStartHandler(HKEY("auth"),	xmpp_start_auth, 0);
///		AddXMPPStartHandler(HKEY("message"),	xmpp_start_message, 0);
		AddXMPPStartHandler(HKEY("html"),	xmpp_start_html, 0);


		CtdlRegisterSessionHook(xmpp_cleanup_function, EVT_STOP, PRIO_STOP + 70);
                CtdlRegisterSessionHook(xmpp_login_hook, EVT_LOGIN, PRIO_LOGIN + 90);
                CtdlRegisterSessionHook(xmpp_logout_hook, EVT_LOGOUT, PRIO_LOGOUT + 90);
                CtdlRegisterSessionHook(xmpp_login_hook, EVT_UNSTEALTH, PRIO_UNSTEALTH + 1);
                CtdlRegisterSessionHook(xmpp_logout_hook, EVT_STEALTH, PRIO_STEALTH + 1);
		CtdlRegisterCleanupHook(xmpp_cleanup);
	}

	/* return our module name for the log */
	return "xmpp";
}


