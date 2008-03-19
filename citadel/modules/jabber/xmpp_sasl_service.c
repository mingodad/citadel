/*
 * $Id$ 
 *
 * Barebones SASL authentication service for XMPP (Jabber) clients.
 *
 * Note: RFC3920 says we "must" support DIGEST-MD5 but we only support PLAIN.
 *
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


	/* Take apart the authentication string */

	CtdlDecodeBase64(decoded_authstring, authstring, strlen(authstring));
	safestrncpy(ident, decoded_authstring, sizeof ident);
	safestrncpy(user, &decoded_authstring[strlen(ident) + 1], sizeof user);
	safestrncpy(pass, &decoded_authstring[strlen(ident) + strlen(user) + 2], sizeof pass);


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
		if (CtdlTryPassword(pass) == pass_ok) {
			return(0);				/* success */
		}
	}

	return(1);						/* failure */
}


/*
 * Output the list of SASL mechanisms offered by this stream.
 */
void xmpp_output_auth_mechs(void) {
	cprintf("<mechanisms xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\">");
	cprintf("<mechanism>PLAIN</mechanism>");
	cprintf("</mechanisms>");
}

/*
 * Here we go ... client is trying to authenticate.
 */
void xmpp_sasl_auth(char *sasl_auth_mech, char *authstring) {

	if (strcasecmp(sasl_auth_mech, "PLAIN")) {
		cprintf("<failure xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\">");
		cprintf("<invalid-mechanism/>");
		cprintf("</failure>");
		return;
	}

        if (CC->logged_in) logout();  /* Client may try to log in twice.  Handle this. */

	if (xmpp_auth_plain(authstring) == 0) {
		cprintf("<success xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\"/>");
	}

	else {
		cprintf("<failure xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\">");
		cprintf("<not-authorized/>");
		cprintf("</failure>");
	}
}



/*
 * Non-SASL authentication
 */
void jabber_non_sasl_authenticate(char *iq_id, char *username, char *password, char *resource) {
	int result;

        if (CC->logged_in) logout();  /* Client may try to log in twice.  Handle this. */

	result = CtdlLoginExistingUser(NULL, username);
	if (result == login_ok) {
		result = CtdlTryPassword(password);
		if (result == pass_ok) {
			cprintf("<iq type=\"result\" id=\"%s\"></iq>", iq_id);	/* success */
			return;
		}
	}

	/* failure */
	cprintf("<iq type=\"error\" id=\"%s\">", iq_id);
	cprintf("<error code=\"401\" type=\"auth\">"
		"<not-authorized xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
		"</error>"
		"</iq>"
	);
}
