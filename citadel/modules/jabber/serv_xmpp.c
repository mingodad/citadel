/*
 * $Id:  $ 
 *
 * XMPP (Jabber) service for the Citadel system
 * Copyright (c) 2007 by Art Cancro
 * This code is released under the terms of the GNU General Public License.
 *
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
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "room_ops.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "md5.h"
#include "ctdl_module.h"

#ifdef HAVE_EXPAT
#include <expat.h>
#include "serv_xmpp.h"


/* We have just received a <stream> tag from the client, so send them ours */

void xmpp_stream_start(void *data, const char *supplied_el, const char **attr)
{

	lprintf(CTDL_DEBUG, "New stream detected.\n");

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

	/* Binding... */
	cprintf("<bind xmlns=\"urn:ietf:params:xml:ns:xmpp-bind\"/>");

	/* Sessions... */
	cprintf("<session xmlns=\"urn:ietf:params:xml:ns:xmpp-session\"/>");

	/* A really bad SASL implementation... */
	xmpp_output_auth_mechs();

	/* ...and the ability to close XML tags using angle brackets.  We should patent this. */
	cprintf("</stream:features>");
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

	lprintf(CTDL_DEBUG, "XMPP ELEMENT START: <%s>\n", el);

	for (i=0; attr[i] != NULL; i+=2) {
		lprintf(CTDL_DEBUG, "                    Attribute '%s' = '%s'\n", attr[i], attr[i+1]);
	}

	if (!strcasecmp(el, "stream")) {
		xmpp_stream_start(data, supplied_el, attr);
	}

	else if (!strcasecmp(el, "iq")) {
		const char *iqtype = NULL;
		const char *iqid = NULL;
		for (i=0; attr[i] != NULL; i+=2) {
			if (!strcasecmp(attr[i], "type")) iqtype = attr[i+1];
			if (!strcasecmp(attr[i], "id")) iqid = attr[i+1];
		}
		if ((iqtype != NULL) && (iqid != NULL)) {
			if (!strcasecmp(iqtype, "set")) {
				safestrncpy(XMPP->iq_bind_id, iqid, sizeof XMPP->iq_bind_id);
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
}



void xmpp_xml_end(void *data, const char *supplied_el) {
	char el[256];
	char *sep = NULL;

	/* Axe the namespace, we don't care about it */
	safestrncpy(el, supplied_el, sizeof el);
	while (sep = strchr(el, ':'), sep) {
		strcpy(el, ++sep);
	}

	lprintf(CTDL_DEBUG, "XMPP ELEMENT END  : <%s>\n", el);
	if (XMPP->chardata_len > 0) {
		lprintf(CTDL_DEBUG, "          chardata: %s\n", XMPP->chardata);
	}

	if (!strcasecmp(el, "resource")) {
		if (XMPP->chardata_len > 0) {
			safestrncpy(XMPP->iq_client_resource, XMPP->chardata,
				sizeof XMPP->iq_client_resource);
		}
	}

	else if (!strcasecmp(el, "iq")) {


		/* If this <iq> stanza was a "bind" attempt, process it ... */

		if ( (!IsEmptyStr(XMPP->iq_bind_id)) && (!IsEmptyStr(XMPP->iq_client_resource)) ) {

			/* Generate the "full JID" of the client resource */

			snprintf(XMPP->client_jid, sizeof XMPP->client_jid,
				"%d@%s/%s",
				CC->cs_pid,
				config.c_fqdn,
				XMPP->iq_client_resource
			);

			/* Tell the client what its JID is */

			cprintf("<iq type=\"result\" id=\"%s\">", XMPP->iq_bind_id);
			cprintf("<bind xmlns=\"urn:ietf:params:xml:ns:xmpp-bind\">");
			cprintf("<jid>%s</jid>", XMPP->client_jid);
			cprintf("</bind>");
			cprintf("</iq>");
		}

		else if (XMPP->iq_session) {
			cprintf("<iq type=\"result\" id=\"%s\">", XMPP->iq_bind_id);
			cprintf("</iq>");
		}

		else {
			cprintf("<iq type=\"error\" id=\"%s\">", XMPP->iq_bind_id);
			cprintf("<error></error>");
			cprintf("</iq>");
		}

		/* Now clear these fields out so they don't get used by a future stanza */
		XMPP->iq_bind_id[0] = 0;
		XMPP->iq_client_resource[0] = 0;
		XMPP->iq_session = 1;
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

	lprintf(CTDL_DEBUG, "Performing XMPP cleanup hook\n");
	if (XMPP->chardata != NULL) {
		free(XMPP->chardata);
		XMPP->chardata = NULL;
		XMPP->chardata_len = 0;
		XMPP->chardata_alloc = 0;
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

	/* XMPP does not use a greeting, but we still have to initialize some things. */

	XMPP->xp = XML_ParserCreateNS("UTF-8", ':');
	if (XMPP->xp == NULL) {
		lprintf(CTDL_ALERT, "Cannot create XML parser!\n");
		CC->kill_me = 1;
		return;
	}

	XML_SetElementHandler(XMPP->xp, xmpp_xml_start, xmpp_xml_end);
	XML_SetCharacterDataHandler(XMPP->xp, xmpp_xml_chardata);
	// XML_SetUserData(XMPP->xp, something...);
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
		lprintf(CTDL_ERR, "Client disconnected: ending session.\r\n");
		CC->kill_me = 1;
		return;
	}

	/* FIXME ... this is woefully inefficient. */

	XML_Parse(XMPP->xp, cmdbuf, 1, 0);
}

const char *CitadelServiceXMPP="XMPP";

#endif	/* HAVE_EXPAT */

CTDL_MODULE_INIT(jabber)
{
#ifdef HAVE_EXPAT
	if (!threading) {
		/* CtdlRegisterServiceHook(config.c_xmpp_port,		FIXME	*/
		CtdlRegisterServiceHook(5222,
					NULL,
					xmpp_greeting,
					xmpp_command_loop,
					NULL,
					CitadelServiceXMPP);
		CtdlRegisterSessionHook(xmpp_cleanup_function, EVT_STOP);
	#else
		lprintf(CTDL_INFO, "This server is missing the Expat XML parser.  Jabber service will be disabled.\n");
#endif
	}

	/* return our Subversion id for the Log */
	return "$Id: $";
}
