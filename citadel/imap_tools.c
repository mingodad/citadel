/*
 * $Id$
 *
 * Utility functions for the IMAP module.
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include "citadel.h"
#include "tools.h"
#include "imap_tools.h"

/*
 * Break a command down into tokens, taking into consideration the
 * possibility of escaping spaces using quoted tokens
 */
int imap_parameterize(char **args, char *buf) {
	int num = 0;
	int start = 0;
	int i;
	int in_quote = 0;
	int original_len;

	strcat(buf, " ");

	original_len = strlen(buf);

	for (i=0; i<original_len; ++i) {

		if ( (isspace(buf[i])) && (!in_quote) ) {
			buf[i] = 0;
			args[num] = &buf[start];
			start = i+1;
			if (args[num][0] == '\"') {
				++args[num];
				args[num][strlen(args[num])-1] = 0;
			}
			++num;
		}

		else if ( (buf[i] == '\"') && (!in_quote) ) {
			in_quote = 1;
		}

		else if ( (buf[i] == '\"') && (in_quote) ) {
			in_quote = 0;
		}

	}

	return(num);
}
			
/*
 * Convert a struct quickroom to an IMAP-compatible mailbox name.
 */
void imap_mailboxname(char *buf, int bufsize, struct quickroom *qrbuf) {

	safestrncpy(buf, qrbuf->QRname, bufsize);
	if (qrbuf->QRflags & QR_MAILBOX) {
		strcpy(buf, &buf[11]);
		if (!strcasecmp(buf, MAILROOM)) strcpy(buf, "INBOX");
	}
}


/*
 * Parse a parenthesized list of items (as with the FETCH command)
 */
int imap_extract_data_items(char *items) {
	int num_items = 0;
	int i;

	/* First, convert all whitespace to ordinary space characters */
	for (i=0; i<strlen(items); ++i) {
		if (isspace(items[i])) items[i] = ' ';
	}

	return num_items;
}


