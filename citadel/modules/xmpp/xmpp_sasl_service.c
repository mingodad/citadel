/*
 * Barebones SASL authentication service for XMPP (Jabber) clients.
 *
 * Note: RFC3920 says we "must" support DIGEST-MD5 but we only support PLAIN.
 *
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
#include <expat.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "user_ops.h"
#include "internet_addressing.h"
#include "md5.h"
#include "ctdl_module.h"
#include "serv_xmpp.h"


/*
 * PLAIN authentication.  Returns zero on success, nonzero on failure.
 */
int xmpp_auth_plain(char *authstring)
{
	char decoded_authstring[1024];
	char ident[256];
	char user[256];
	char pass[256];
	int result;
	long len;


	/* Take apart the authentication string */
	memset(pass, 0, sizeof(pass));

	CtdlDecodeBase64(decoded_authstring, authstring, strlen(authstring));
	safestrncpy(ident, decoded_authstring, sizeof ident);
	safestrncpy(user, &decoded_authstring[strlen(ident) + 1], sizeof user);
	len = safestrncpy(pass, &decoded_authstring[strlen(ident) + strlen(user) + 2], sizeof pass);
	if (len < 0)
		len = -len;

	/* If there are underscores in either string, change them to spaces.  Some clients
	 * do not allow spaces so we can tell the user to substitute underscores if their
	 * login name contains spaces.
	 */
	convert_spaces_to_underscores(ident);
	convert_spaces_to_underscores(user);

	/* Now attempt authentication */

	if (!IsEmptyStr(ident)) {
		result = CtdlLoginExistingUser(user, ident);
	}
	else {
		result = CtdlLoginExistingUser(NULL, user);
	}

	if (result == login_ok) {
		if (CtdlTryPassword(pass, len) == pass_ok) {
			return(0);				/* success */
		}
	}

	return(1);						/* failure */
}


/*
 * Output the list of SASL mechanisms offered by this stream.
 */
void xmpp_output_auth_mechs(void) {
	XPUT("<mechanisms xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\">"
	     "<mechanism>PLAIN</mechanism>"
	     "</mechanisms>");
}

/*
 * Here we go ... client is trying to authenticate.
 */
void xmpp_sasl_auth(char *sasl_auth_mech, char *authstring) {

	if (strcasecmp(sasl_auth_mech, "PLAIN")) {
		XPUT("<failure xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\">"
		     "<invalid-mechanism/>"
		     "</failure>");
		return;
	}

        if (CC->logged_in) CtdlUserLogout();  /* Client may try to log in twice.  Handle this. */

	if (CC->nologin) {
		XPUT("<failure xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\">"
		     "<system-shutdown/>"
		     "</failure>");
	}

	else if (xmpp_auth_plain(authstring) == 0) {
		XPUT("<success xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\"/>");
	}

	else {
		XPUT("<failure xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\">"
		     "<not-authorized/>"
		     "</failure>");
	}
}



/*
 * Non-SASL authentication
 */
void xmpp_non_sasl_authenticate(char *iq_id, char *username, char *password, char *resource) {
	int result;

        if (CC->logged_in) CtdlUserLogout();  /* Client may try to log in twice.  Handle this. */

	result = CtdlLoginExistingUser(NULL, username);
	if (result == login_ok) {
		result = CtdlTryPassword(password, strlen(password));
		if (result == pass_ok) {
			XPUT("<iq type=\"result\" id=\"");
			XPutProp(iq_id, strlen(iq_id));
			XPUT("\"></iq>"); /* success */
			return;
		}
	}

	/* failure */
	XPUT("<iq type=\"error\" id=\"");
	XPutProp(iq_id, strlen(iq_id));
	XPUT("\">"
	     "<error code=\"401\" type=\"auth\">"
	     "<not-authorized xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
	     "</error>"
	     "</iq>"
	);
}
