/*
 * $Id$
 *
 * Utility functions for the IMAP module.
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "citadel.h"
#include "sysdep_decls.h"
#include "tools.h"
#include "room_ops.h"
#include "internet_addressing.h"
#include "imap_tools.h"


/*
 * Output a string to the IMAP client, either as a literal or quoted.
 * (We do a literal if it has any double-quotes or backslashes.)
 */
void imap_strout(char *buf) {
	int i;
	int is_literal = 0;

	if (buf == NULL) {		/* yeah, we handle this */
		cprintf("NIL");
		return;
	}

	for (i=0; i<strlen(buf); ++i) {
		if ( (buf[i]=='\"') || (buf[i]=='\\') ) is_literal = 1;
	}

	if (is_literal) {
		cprintf("{%d}\r\n%s", strlen(buf), buf);
	}

	else {
		cprintf("\"%s\"", buf);
	}
}

	



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
	struct floor *fl;

	/*
	 * For mailboxes, just do it straight...
	 */
	if (qrbuf->QRflags & QR_MAILBOX) {
		safestrncpy(buf, qrbuf->QRname, bufsize);
		strcpy(buf, &buf[11]);
		if (!strcasecmp(buf, MAILROOM)) strcpy(buf, "INBOX");
	}

	/*
	 * Otherwise, prefix the floor name as a "public folders" moniker
	 */
	else {
		fl = cgetfloor(qrbuf->QRfloor);
		lprintf(9, "Floor %d: \n", qrbuf->QRfloor);
		lprintf(9, "          %s\n", fl->f_name); /* FIXME take out */
		snprintf(buf, bufsize, "%s|%s",
			fl->f_name,
			qrbuf->QRname);
	}
}


/*
 * Convert an inputted folder name to our best guess as to what an equivalent
 * room name should be.
 *
 * If an error occurs, it returns -1.  Otherwise...
 *
 * The lower eight bits of the return value are the floor number on which the
 * room most likely resides.   The upper eight bits may contain flags,
 * including IR_MAILBOX if we're dealing with a personal room.
 *
 */
int imap_roomname(char *rbuf, int bufsize, char *foldername) {
	int levels;
	char buf[SIZ];
	int i;
	struct floor *fl;

	if (foldername == NULL) return(-1);
	levels = num_parms(foldername);

	/* When we can support hierarchial mailboxes, take this out. */
	if (levels > 2) return(-1);

	/*
	 * Convert the crispy idiot's reserved names to our reserved names.
	 */
	if (!strcasecmp(foldername, "INBOX")) {
		safestrncpy(rbuf, MAILROOM, bufsize);
		return(0 | IR_MAILBOX);
	}

	if (levels > 1) {
		extract(buf, foldername, 0);
		for (i=0; i<MAXFLOORS; ++i) {
			fl = cgetfloor(i);
			lprintf(9, "floor %d: %s\n", i, fl->f_name); /* FIXME take out */
			if (fl->f_flags & F_INUSE) {
				if (!strcasecmp(buf, fl->f_name)) {
					extract(rbuf, foldername, 1);
					return(i);
				}
			}
		}

		/* since we don't allow multi-level yet, fail.
		extract(rbuf, buf, 1);
		return(0);
		*/
		return(-1);
	}

	safestrncpy(rbuf, foldername, bufsize);
	return(0 | IR_MAILBOX);
}





/*
 * Output a struct internet_address_list in the form an IMAP client wants
 */
void imap_ial_out(struct internet_address_list *ialist) {
	struct internet_address_list *iptr;

	if (ialist == NULL) {
		cprintf("NIL");
		return;
	}

	cprintf("(");	

	for (iptr = ialist; iptr != NULL; iptr = iptr->next) {
		cprintf("(");	
		imap_strout(iptr->ial_name);
		cprintf(" NIL ");
		imap_strout(iptr->ial_user);
		cprintf(" ");
		imap_strout(iptr->ial_node);
		cprintf(")");	
	}

	cprintf(")");
}



/*
 * Determine whether the supplied string is a valid message set.
 * If the string contains only numbers, colons, commas, and asterisks,
 * return 1 for a valid message set.  If any other character is found, 
 * return 0.
 */
int imap_is_message_set(char *buf) {
	int i;

	if (buf == NULL) return(0);		/* stupidity checks */
	if (strlen(buf) == 0) return(0);

	if (!strcasecmp(buf, "ALL")) return(1);	/* macro?  why?  */

	for (i=0; i<strlen(buf); ++i)  {	/* now start the scan */
		if (
			(!isdigit(buf[i]))
			&& (buf[i] != ':')
			&& (buf[i] != ',')
			&& (buf[i] != '*')
		) return(0);
	}

	return(1);				/* looks like we're good */
}
