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


#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

/*
 * Output a string to the IMAP client, either as a literal or quoted.
 * (We do a literal if it has any double-quotes or backslashes.)
 */
void imap_strout(char *buf)
{
	int i;
	int is_literal = 0;

	if (buf == NULL) {	/* yeah, we handle this */
		cprintf("NIL");
		return;
	}
	for (i = 0; i < strlen(buf); ++i) {
		if ((buf[i] == '\"') || (buf[i] == '\\'))
			is_literal = 1;
	}

	if (is_literal) {
		cprintf("{%ld}\r\n%s", (long)strlen(buf), buf);
	} else {
		cprintf("\"%s\"", buf);
	}
}





/*
 * Break a command down into tokens, taking into consideration the
 * possibility of escaping spaces using quoted tokens
 */
int imap_parameterize(char **args, char *buf)
{
	int num = 0;
	int start = 0;
	int i;
	int in_quote = 0;
	int original_len;

	strcat(buf, " ");

	original_len = strlen(buf);

	for (i = 0; i < original_len; ++i) {

		if ((isspace(buf[i])) && (!in_quote)) {
			buf[i] = 0;
			args[num] = &buf[start];
			start = i + 1;
			if (args[num][0] == '\"') {
				++args[num];
				args[num][strlen(args[num]) - 1] = 0;
			}
			++num;
		} else if ((buf[i] == '\"') && (!in_quote)) {
			in_quote = 1;
		} else if ((buf[i] == '\"') && (in_quote)) {
			in_quote = 0;
		}
	}

	return (num);
}

/*
 * Convert a struct ctdlroom to an IMAP-compatible mailbox name.
 */
void imap_mailboxname(char *buf, int bufsize, struct ctdlroom *qrbuf)
{
	struct floor *fl;
	int i;

	/*
	 * For mailboxes, just do it straight.
	 * Also handle Kolab-compatible groupware folder names.
	 */
	if (qrbuf->QRflags & QR_MAILBOX) {
		safestrncpy(buf, qrbuf->QRname, bufsize);
		strcpy(buf, &buf[11]);
		if (!strcasecmp(buf, MAILROOM)) {
			strcpy(buf, "INBOX");
		}
		if (!strcasecmp(buf, USERCALENDARROOM)) {
			sprintf(buf, "INBOX|%s", USERCALENDARROOM);
		}
		if (!strcasecmp(buf, USERCONTACTSROOM)) {
			sprintf(buf, "INBOX|%s", USERCONTACTSROOM);
		}
		if (!strcasecmp(buf, USERTASKSROOM)) {
			sprintf(buf, "INBOX|%s", USERTASKSROOM);
		}
		if (!strcasecmp(buf, USERNOTESROOM)) {
			sprintf(buf, "INBOX|%s", USERNOTESROOM);
		}
	}
	/*
	 * Otherwise, prefix the floor name as a "public folders" moniker
	 */
	else {
		fl = cgetfloor(qrbuf->QRfloor);
		snprintf(buf, bufsize, "%s|%s",
			 fl->f_name,
			 qrbuf->QRname);
	}

	/*
	 * Replace delimiter characters with "|" for pseudo-folder-delimiting
	 */
	for (i=0; i<strlen(buf); ++i) {
		if (buf[i] == FDELIM) buf[i] = '|';
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
int imap_roomname(char *rbuf, int bufsize, char *foldername)
{
	int levels;
	char floorname[SIZ];
	char roomname[SIZ];
	int i;
	struct floor *fl;
	int ret = (-1);

	if (foldername == NULL) return(-1);
	levels = num_parms(foldername);

	/*
	 * Convert the crispy idiot's reserved names to our reserved names.
	 * Also handle Kolab-compatible groupware folder names.
	 */
	if (!strcasecmp(foldername, "INBOX")) {
		safestrncpy(rbuf, MAILROOM, bufsize);
		ret = (0 | IR_MAILBOX);
	}
	else if ( (!strncasecmp(foldername, "INBOX", 5))
	      && (!strcasecmp(&foldername[6], USERCALENDARROOM)) ) {
		safestrncpy(rbuf, USERCALENDARROOM, bufsize);
		ret = (0 | IR_MAILBOX);
	}
	else if ( (!strncasecmp(foldername, "INBOX", 5))
	      && (!strcasecmp(&foldername[6], USERCONTACTSROOM)) ) {
		safestrncpy(rbuf, USERCONTACTSROOM, bufsize);
		ret = (0 | IR_MAILBOX);
	}
	else if ( (!strncasecmp(foldername, "INBOX", 5))
	      && (!strcasecmp(&foldername[6], USERTASKSROOM)) ) {
		safestrncpy(rbuf, USERTASKSROOM, bufsize);
		ret = (0 | IR_MAILBOX);
	}
	else if ( (!strncasecmp(foldername, "INBOX", 5))
	      && (!strcasecmp(&foldername[6], USERNOTESROOM)) ) {
		safestrncpy(rbuf, USERNOTESROOM, bufsize);
		ret = (0 | IR_MAILBOX);
	}
	else if (levels > 1) {
		extract(floorname, foldername, 0);
		strcpy(roomname, &foldername[strlen(floorname)+1]);
		for (i = 0; i < MAXFLOORS; ++i) {
			fl = cgetfloor(i);
			if (fl->f_flags & F_INUSE) {
				if (!strcasecmp(floorname, fl->f_name)) {
					strcpy(rbuf, roomname);
					ret = i;
				}
			}
		}

		if (ret < 0) {
			/* No subfolderificationalisticism on this one... */
			safestrncpy(rbuf, foldername, bufsize);
			ret = (0 | IR_MAILBOX);
		}

	}
	else {
		safestrncpy(rbuf, foldername, bufsize);
		ret = (0 | IR_MAILBOX);
	}

	/* Undelimiterizationalize the room name (change '|') */
	for (i=0; i<strlen(rbuf); ++i) {
		if (rbuf[i] == '|') rbuf[i] = FDELIM;
	}


/*** This doesn't work.
	char buf[SIZ];
	if (ret & IR_MAILBOX) {
		if (atol(rbuf) == 0L) {
			strcpy(buf, rbuf);
			sprintf(rbuf, "%010ld.%s", CC->user.usernum, buf);
		}
	}
 ***/

	lprintf(9, "(That translates to \"%s\")\n", rbuf);
	return(ret);
}





/*
 * Output a struct internet_address_list in the form an IMAP client wants
 */
void imap_ial_out(struct internet_address_list *ialist)
{
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
int imap_is_message_set(char *buf)
{
	int i;

	if (buf == NULL)
		return (0);	/* stupidity checks */
	if (strlen(buf) == 0)
		return (0);

	if (!strcasecmp(buf, "ALL"))
		return (1);	/* macro?  why?  */

	for (i = 0; i < strlen(buf); ++i) {	/* now start the scan */
		if (
			   (!isdigit(buf[i]))
			   && (buf[i] != ':')
			   && (buf[i] != ',')
			   && (buf[i] != '*')
		    )
			return (0);
	}

	return (1);		/* looks like we're good */
}


/*
 * imap_match.c, based on wildmat.c from INN
 * hacked for Citadel/IMAP by Daniel Malament
 */

/* note: not all return statements use these; don't change them */
#define WILDMAT_TRUE	1
#define WILDMAT_FALSE	0
#define WILDMAT_ABORT	-1
#define WILDMAT_DELIM 	'|'

/*
 * Match text and p, return TRUE, FALSE, or ABORT.
 */
static int do_imap_match(const char *supplied_text, const char *supplied_p)
{
	int matched, i;
	char lcase_text[SIZ], lcase_p[SIZ];
	char *text = lcase_text;
	char *p = lcase_p;

	/* Copy both strings and lowercase them, in order to
	 * make this entire operation case-insensitive.
	 */
	for (i=0; i<=strlen(supplied_text); ++i)
		lcase_text[i] = tolower(supplied_text[i]);
	for (i=0; i<=strlen(supplied_p); ++i)
		p[i] = tolower(supplied_p[i]);

	/* Start matching */
	for (; *p; text++, p++) {
		if ((*text == '\0') && (*p != '*') && (*p != '%')) {
			return WILDMAT_ABORT;
		}
		switch (*p) {
		default:
			if (*text != *p) {
				return WILDMAT_FALSE;
			}
			continue;
		case '*':
star:
			while (++p, ((*p == '*') || (*p == '%'))) {
				/* Consecutive stars or %'s act
				 * just like one star.
				 */
				continue;
			}
			if (*p == '\0') {
				/* Trailing star matches everything. */
				return WILDMAT_TRUE;
			}
			while (*text) {
				if ((matched = do_imap_match(text++, p))
				   != WILDMAT_FALSE) {
					return matched;
				}
			}
			return WILDMAT_ABORT;
		case '%':
			while (++p, ((*p == '*') || (*p == '%'))) {
				/* Consecutive %'s act just like one, but even
				 * a single star makes the sequence act like
				 * one star, instead.
				 */
				if (*p == '*') {
					goto star;
				}
				continue;
			}
			if (*p == '\0') {
				/*
				 * Trailing % matches everything
				 * without a delimiter.
				 */
				while (*text) {
					if (*text == WILDMAT_DELIM) {
						return WILDMAT_FALSE;
					}
					text++;
				}
				return WILDMAT_TRUE;
			}
			while (*text && (*(text - 1) != WILDMAT_DELIM)) {
				if ((matched = do_imap_match(text++, p))
				   != WILDMAT_FALSE) {
					return matched;
				}
			}
			return WILDMAT_ABORT;
		}
	}

	return (*text == '\0');
}



/*
 * Support function for mailbox pattern name matching in LIST and LSUB
 * Returns nonzero if the supplied mailbox name matches the supplied pattern.
 */
int imap_mailbox_matches_pattern(char *pattern, char *mailboxname)
{
	/* handle just-star case quickly */
	if ((pattern[0] == '*') && (pattern[1] == '\0')) {
		return WILDMAT_TRUE;
	}
	return (do_imap_match(mailboxname, pattern) == WILDMAT_TRUE);
}



/*
 * Compare an IMAP date string (date only, no time) to the date found in
 * a Unix timestamp.
 */
int imap_datecmp(char *datestr, time_t msgtime) {
	char daystr[SIZ];
	char monthstr[SIZ];
	char yearstr[SIZ];
	int i;
	int day, month, year;
	int msgday, msgmonth, msgyear;
	struct tm msgtm;

	if (datestr == NULL) return(0);

	/* Expecting a date in the form dd-Mmm-yyyy */
	extract_token(daystr, datestr, 0, '-');
	extract_token(monthstr, datestr, 1, '-');
	extract_token(yearstr, datestr, 2, '-');

	day = atoi(daystr);
	year = atoi(yearstr);
	month = 0;
	for (i=0; i<12; ++i) {
		if (!strcasecmp(monthstr, ascmonths[i])) {
			month = i;
		}
	}

	/* Extract day/month/year from message timestamp */
	memcpy(&msgtm, localtime(&msgtime), sizeof(struct tm));
	msgday = msgtm.tm_mday;
	msgmonth = msgtm.tm_mon;
	msgyear = msgtm.tm_year + 1900;

	/* Now start comparing */

	if (year < msgyear) return(+1);
	if (year > msgyear) return(-1);

	if (month < msgmonth) return(+1);
	if (month > msgmonth) return(-1);

	if (day < msgday) return(+1);
	if (day > msgday) return(-1);

	return(0);
}
