/*
 * $Id$ 
 *
 * XMPP (Jabber) service for the Citadel system
 * Copyright (c) 2007-2009 by Art Cancro
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
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
#include <libcitadel.h>
#include <expat.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "md5.h"
#include "ctdl_module.h"
#include "serv_xmpp.h"

struct xmpp_event *xmpp_queue = NULL;

/* We have just received a <stream> tag from the client, so send them ours */

void xmpp_stream_start(void *data, const char *supplied_el, const char **attr)
{
	while (*attr) {
		if (!strcasecmp(attr[0], "to")) {
			safestrncpy(XMPP->server_name, attr[1], sizeof XMPP->server_name);
		}
		attr += 2;
	}

	cprintf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");

   	cprintf("<stream:stream ");
	cprintf("from=\"%s\" ", XMPP->server_name);
	cprintf("id=\"%08x\" ", CC->cs_pid);
	cprintf("version=\"1.0\" ");
	cprintf("xmlns:stream=\"http://etherx.jabber.org/streams\" ");
	cprintf("xmlns=\"jabber:client\">");

	/* The features of this stream are... */
	cprintf("<stream:features>");

#ifdef HAVE_OPENSSL_XXXX_COMMENTED_OUT
	/* TLS encryption (but only if it isn't already active) */
	if (!CC->redirect_ssl) {
		cprintf("<starttls xmlns='urn:ietf:params:xml:ns:xmpp-tls'></starttls>");
	}
#endif

	if (!CC->logged_in) {
		/* If we're not logged in yet, offer SASL as our feature set */
		xmpp_output_auth_mechs();

		/* Also offer non-SASL authentication */
		cprintf("<auth xmlns=\"http://jabber.org/features/iq-auth\"/>");
	}

	/* Offer binding and sessions as part of our feature set */
	cprintf("<bind xmlns=\"urn:ietf:params:xml:ns:xmpp-bind\"/>");
	cprintf("<session xmlns=\"urn:ietf:params:xml:ns:xmpp-session\"/>");

	cprintf("</stream:features>");

	CC->is_async = 1;		/* XMPP sessions are inherently async-capable */
}


void xmpp_xml_start(void *data, const char *supplied_el, const char **attr) {
	char el[256];
	char *sep = NULL;
	int i;

	/* Axe the namespace, we don't care about it */
	safestrncpy(el, supplied_el, sizeof el);
	while (sep = strchr(el, ':'), sep) {
		strcpy(el, ++sep);
	}

	/*
	CtdlLogPrintf(CTDL_DEBUG, "XMPP ELEMENT START: <%s>\n", el);
	for (i=0; attr[i] != NULL; i+=2) {
		CtdlLogPrintf(CTDL_DEBUG, "                    Attribute '%s' = '%s'\n", attr[i], attr[i+1]);
	}
	uncomment for more verbosity */

	if (!strcasecmp(el, "stream")) {
		xmpp_stream_start(data, supplied_el, attr);
	}

	else if (!strcasecmp(el, "query")) {
		XMPP->iq_query_xmlns[0] = 0;
		safestrncpy(XMPP->iq_query_xmlns, supplied_el, sizeof XMPP->iq_query_xmlns);
	}

	else if (!strcasecmp(el, "bind")) {
		XMPP->bind_requested = 1;
	}

	else if (!strcasecmp(el, "iq")) {
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

	else if (!strcasecmp(el, "auth")) {
		XMPP->sasl_auth_mech[0] = 0;
		for (i=0; attr[i] != NULL; i+=2) {
			if (!strcasecmp(attr[i], "mechanism")) {
				safestrncpy(XMPP->sasl_auth_mech, attr[i+1], sizeof XMPP->sasl_auth_mech);
			}
		}
	}

	else if (!strcasecmp(el, "message")) {
		for (i=0; attr[i] != NULL; i+=2) {
			if (!strcasecmp(attr[i], "to")) {
				safestrncpy(XMPP->message_to, attr[i+1], sizeof XMPP->message_to);
			}
		}
	}

	else if (!strcasecmp(el, "html")) {
		++XMPP->html_tag_level;
	}
}



void xmpp_xml_end(void *data, const char *supplied_el) {
	char el[256];
	char *sep = NULL;

	/* Axe the namespace, we don't care about it */
	safestrncpy(el, supplied_el, sizeof el);
	while (sep = strchr(el, ':'), sep) {
		strcpy(el, ++sep);
	}

	/*
	CtdlLogPrintf(CTDL_DEBUG, "XMPP ELEMENT END  : <%s>\n", el);
	if (XMPP->chardata_len > 0) {
		CtdlLogPrintf(CTDL_DEBUG, "          chardata: %s\n", XMPP->chardata);
	}
	uncomment for more verbosity */

	if (!strcasecmp(el, "resource")) {
		if (XMPP->chardata_len > 0) {
			safestrncpy(XMPP->iq_client_resource, XMPP->chardata,
				sizeof XMPP->iq_client_resource);
			striplt(XMPP->iq_client_resource);
		}
	}

	if (!strcasecmp(el, "username")) {		/* NON SASL ONLY */
		if (XMPP->chardata_len > 0) {
			safestrncpy(XMPP->iq_client_username, XMPP->chardata,
				sizeof XMPP->iq_client_username);
			striplt(XMPP->iq_client_username);
		}
	}

	if (!strcasecmp(el, "password")) {		/* NON SASL ONLY */
		if (XMPP->chardata_len > 0) {
			safestrncpy(XMPP->iq_client_password, XMPP->chardata,
				sizeof XMPP->iq_client_password);
			striplt(XMPP->iq_client_password);
		}
	}

	else if (!strcasecmp(el, "iq")) {

		/*
		 * iq type="get" (handle queries)
		 */
		if (!strcasecmp(XMPP->iq_type, "get")) {

			/*
			 * Query on a namespace
			 */
			if (!IsEmptyStr(XMPP->iq_query_xmlns)) {
				xmpp_query_namespace(XMPP->iq_id, XMPP->iq_from,
						XMPP->iq_to, XMPP->iq_query_xmlns);
			}

			/*
			 * Unknown queries ... return the XML equivalent of a blank stare
			 */
			else {
				cprintf("<iq type=\"result\" id=\"%s\">", XMPP->iq_id);
				cprintf("</iq>");
			}
		}

		/*
		 * Non SASL authentication
		 */
		else if (
			(!strcasecmp(XMPP->iq_type, "set"))
			&& (!strcasecmp(XMPP->iq_query_xmlns, "jabber:iq:auth:query"))
			) {

			jabber_non_sasl_authenticate(
				XMPP->iq_id,
				XMPP->iq_client_username,
				XMPP->iq_client_password,
				XMPP->iq_client_resource
			);
		}	

		/*
		 * If this <iq> stanza was a "bind" attempt, process it ...
		 */
		else if (
			(XMPP->bind_requested)
			&& (!IsEmptyStr(XMPP->iq_id))
			&& (!IsEmptyStr(XMPP->iq_client_resource))
			&& (CC->logged_in)
			) {

			/* Generate the "full JID" of the client resource */

			snprintf(XMPP->client_jid, sizeof XMPP->client_jid,
				"%s/%s",
				CC->cs_inet_email,
				XMPP->iq_client_resource
			);

			/* Tell the client what its JID is */

			cprintf("<iq type=\"result\" id=\"%s\">", XMPP->iq_id);
			cprintf("<bind xmlns=\"urn:ietf:params:xml:ns:xmpp-bind\">");
			cprintf("<jid>%s</jid>", XMPP->client_jid);
			cprintf("</bind>");
			cprintf("</iq>");
		}

		else if (XMPP->iq_session) {
			cprintf("<iq type=\"result\" id=\"%s\">", XMPP->iq_id);
			cprintf("</iq>");
		}

		else {
			cprintf("<iq type=\"error\" id=\"%s\">", XMPP->iq_id);
			cprintf("<error></error>");
			cprintf("</iq>");
		}

		/* Now clear these fields out so they don't get used by a future stanza */
		XMPP->iq_id[0] = 0;
		XMPP->iq_from[0] = 0;
		XMPP->iq_to[0] = 0;
		XMPP->iq_type[0] = 0;
		XMPP->iq_client_resource[0] = 0;
		XMPP->iq_session = 0;
		XMPP->iq_query_xmlns[0] = 0;
		XMPP->bind_requested = 0;
	}

	else if (!strcasecmp(el, "auth")) {

		/* Try to authenticate (this function is responsible for the output stanza) */
		xmpp_sasl_auth(XMPP->sasl_auth_mech, (XMPP->chardata != NULL ? XMPP->chardata : "") );

		/* Now clear these fields out so they don't get used by a future stanza */
		XMPP->sasl_auth_mech[0] = 0;
	}

	else if (!strcasecmp(el, "session")) {
		XMPP->iq_session = 1;
	}

	else if (!strcasecmp(el, "presence")) {

		/* Respond to a <presence> update by firing back with presence information
		 * on the entire wholist.  Check this assumption, it's probably wrong.
		 */
		jabber_wholist_presence_dump();
	}

	else if ( (!strcasecmp(el, "body")) && (XMPP->html_tag_level == 0) ) {
		if (XMPP->message_body != NULL) {
			free(XMPP->message_body);
			XMPP->message_body = NULL;
		}
		if (XMPP->chardata_len > 0) {
			XMPP->message_body = strdup(XMPP->chardata);
		}
	}

	else if (!strcasecmp(el, "message")) {
		jabber_send_message(XMPP->message_to, XMPP->message_body);
		XMPP->html_tag_level = 0;
	}

	else if (!strcasecmp(el, "html")) {
		--XMPP->html_tag_level;
	}

	else if (!strcasecmp(el, "starttls")) {
#ifdef HAVE_OPENSSL
	cprintf("<proceed xmlns='urn:ietf:params:xml:ns:xmpp-tls'/>");
	CtdlModuleStartCryptoMsgs(NULL, NULL, NULL);
	if (!CC->redirect_ssl) CC->kill_me = 1;
#else
	cprintf("<failure xmlns='urn:ietf:params:xml:ns:xmpp-tls'/>");
	CC->kill_me = 1;
#endif
	}

	XMPP->chardata_len = 0;
	if (XMPP->chardata_alloc > 0) {
		XMPP->chardata[0] = 0;
	}
}


void xmpp_xml_chardata(void *data, const XML_Char *s, int len)
{
	struct citxmpp *X = XMPP;

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
	strcpy(CC->cs_clientname, "Jabber session");
	CC->session_specific_data = malloc(sizeof(struct citxmpp));
	memset(XMPP, 0, sizeof(struct citxmpp));
	XMPP->last_event_processed = queue_event_seq;

	/* XMPP does not use a greeting, but we still have to initialize some things. */

	XMPP->xp = XML_ParserCreateNS("UTF-8", ':');
	if (XMPP->xp == NULL) {
		CtdlLogPrintf(CTDL_ALERT, "Cannot create XML parser!\n");
		CC->kill_me = 1;
		return;
	}

	XML_SetElementHandler(XMPP->xp, xmpp_xml_start, xmpp_xml_end);
	XML_SetCharacterDataHandler(XMPP->xp, xmpp_xml_chardata);
	// XML_SetUserData(XMPP->xp, something...);

	CC->can_receive_im = 1;		/* This protocol is capable of receiving instant messages */
}


/* 
 * Main command loop for XMPP sessions.
 */
void xmpp_command_loop(void) {
	char cmdbuf[16];
	int retval;

	time(&CC->lastcmd);
	memset(cmdbuf, 0, sizeof cmdbuf); /* Clear it, just in case */
	retval = client_read(cmdbuf, 1);
	if (retval != 1) {
		CtdlLogPrintf(CTDL_ERR, "Client disconnected: ending session.\r\n");
		CC->kill_me = 1;
		return;
	}

	/* FIXME ... this is woefully inefficient. */

	XML_Parse(XMPP->xp, cmdbuf, 1, 0);
}


/*
 * Async loop for XMPP sessions (handles the transmission of unsolicited stanzas)
 */
void xmpp_async_loop(void) {
	xmpp_process_events();
	jabber_output_incoming_messages();
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


const char *CitadelServiceXMPP="XMPP";

CTDL_MODULE_INIT(jabber)
{
	if (!threading) {
		CtdlRegisterServiceHook(config.c_xmpp_c2s_port,
					NULL,
					xmpp_greeting,
					xmpp_command_loop,
					xmpp_async_loop,
					CitadelServiceXMPP);
		CtdlRegisterSessionHook(xmpp_cleanup_function, EVT_STOP);
                CtdlRegisterSessionHook(xmpp_login_hook, EVT_LOGIN);
                CtdlRegisterSessionHook(xmpp_logout_hook, EVT_LOGOUT);
                CtdlRegisterSessionHook(xmpp_login_hook, EVT_UNSTEALTH);
                CtdlRegisterSessionHook(xmpp_logout_hook, EVT_STEALTH);
	}

	/* return our Subversion id for the Log */
	return "$Id$";
}
