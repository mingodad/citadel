/*
 * $Id$
 *
 * Utility functions for the IMAP module.
 *
 * Note: most of the UTF7 and UTF8 handling in here was lifted from Evolution.
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <libcitadel.h>
#include "citadel.h"
#include "sysdep_decls.h"
#include "room_ops.h"
#include "internet_addressing.h"
#include "imap_tools.h"


#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

/* String handling helpers */

/* This code uses some pretty narsty string manipulation. To make everything
 * manageable, we use this semi-high-level string manipulation API. Strings are
 * always \0-terminated, despite the fact that we keep track of the size. */

struct string {
	char* buffer;
	int maxsize;
	int size;
};

static void string_init(struct string* s, char* buf, int bufsize)
{
	s->buffer = buf;
	s->maxsize = bufsize-1;
	s->size = strlen(buf);
}

static char* string_end(struct string* s)
{
	return s->buffer + s->size;
}

/* Append a UTF8 string of a particular length (in bytes). -1 to autocalculate. */

static void string_append_sn(struct string* s, char* p, int len)
{
	if (len == -1)
		len = strlen(p);
	if ((s->size+len) > s->maxsize)
		len = s->maxsize - s->size;
	memcpy(s->buffer + s->size, p, len);
	s->size += len;
	s->buffer[s->size] = '\0';
}

/* As above, always autocalculate. */

#define string_append_s(s, p) string_append_sn((s), (p), -1)

/* Appends a UTF8 character --- which may make the size change by more than 1!
 * If the string overflows, the last character may become invalid. */

static void string_append_c(struct string* s, int c)
{
	char buf[5];
	int len = 0;

	/* Don't do anything if there's no room. */

	if (s->size == s->maxsize)
		return;

	if (c <= 0x7F)
	{
		/* This is the most common case, so we optimise it. */

		s->buffer[s->size++] = c;
		s->buffer[s->size] = 0;
		return;
	}
	else if (c <= 0x7FF)
	{
		buf[0] = 0xC0 | (c >> 6);
		buf[1] = 0x80 | (c & 0x3F);
		len = 2;
	}
	else if (c <= 0xFFFF)
	{
		buf[0] = 0xE0 | (c >> 12);
		buf[1] = 0x80 | ((c >> 6) & 0x3f);
		buf[2] = 0x80 | (c & 0x3f);
		len = 3;
	}
	else
	{
		buf[0] = 0xf0 | c >> 18;
		buf[1] = 0x80 | ((c >> 12) & 0x3f);
		buf[2] = 0x80 | ((c >> 6) & 0x3f);
		buf[3] = 0x80 | (c & 0x3f);
		len = 4;
	}

	string_append_sn(s, buf, len);
}	

/* Reads a UTF8 character from a char*, advancing the pointer. */

int utf8_getc(char** ptr)
{
	unsigned char* p = (unsigned char*) *ptr;
	unsigned char c, r;
	int v, m;

	for (;;)
	{
		r = *p++;
	loop:
		if (r < 0x80)
		{
			*ptr = (char*) p;
			v = r;
			break;
		}
		else if (r < 0xf8)
		{
			/* valid start char? (max 4 octets) */
			v = r;
			m = 0x7f80;	/* used to mask out the length bits */
			do {
				c = *p++;
				if ((c & 0xc0) != 0x80)
				{
					r = c;
					goto loop;
				}
				v = (v<<6) | (c & 0x3f);
				r<<=1;
				m<<=5;
			} while (r & 0x40);
			
			*ptr = (char*)p;

			v &= ~m;
			break;
		}
	}

	return v;
}

/* IMAP name safety */

/* IMAP has certain special requirements in its character set, which means we
 * have to do a fair bit of work to convert Citadel's UTF8 strings to IMAP
 * strings. The next two routines (and their data tables) do that.
 */

static char *utf7_alphabet =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+,";

static unsigned char utf7_rank[256] = {
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x3E,0x3F,0xFF,0xFF,0xFF,
	0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,
	0x0F,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,
	0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,0x30,0x31,0x32,0x33,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
};

/* Base64 helpers. */

static void utf7_closeb64(struct string* out, int v, int i)
{
	int x;

	if (i > 0)
	{
		x = (v << (6-i)) & 0x3F;
		string_append_c(out, utf7_alphabet[x]);
	}
	string_append_c(out, '-');
}

/* Convert from a Citadel name to an IMAP-safe name. Returns the end
 * of the destination.
 */
static char* toimap(char* destp, char* destend, char* src)
{
	struct string dest;
	int state = 0;
	int v = 0;
	int i = 0;

	*destp = 0;
	string_init(&dest, destp, destend-destp);
	/* lprintf(CTDL_DEBUG, "toimap %s\r\n", src); */

	for (;;)
	{
		int c = utf8_getc(&src);
		if (c == '\0')
			break;

		if (c >= 0x20 && c <= 0x7e)
		{
			if (state == 1)
			{
				utf7_closeb64(&dest, v, i);
				state = 0;
				i = 0;
			}

			switch (c)
			{
				case '&':
					string_append_sn(&dest, "&-", 2);
					break;

				case '/':
					/* Citadel extension: / becomes |, because /
					 * isn't valid as part of an IMAP name. */

					c = '|';
					goto defaultcase;

				case '\\':
					/* Citadel extension: backslashes mark folder
					 * seperators in the IMAP subfolder emulation
					 * hack. We turn them into / characters,
					 * *except* if it's the last character in the
					 * string. */

					if (*src != '\0')
						c = '/';
					/* fall through */

				default:
				defaultcase:
					string_append_c(&dest, c);
			}
		}
		else
		{
			if (state == 0)
			{
				string_append_c(&dest, '&');
				state = 1;
			}
			v = (v << 16) | c;
			i += 16;
			while (i >= 6)
			{
				int x = (v >> (i-6)) & 0x3f;
				string_append_c(&dest, utf7_alphabet[x]);
				i -= 6;
			}
		}
	}

	if (state == 1)
		utf7_closeb64(&dest, v, i);
	/* lprintf(CTDL_DEBUG, "    -> %s\r\n", destp); */
	return string_end(&dest);
}

/* Convert from an IMAP-safe name back into a Citadel name. Returns the end of the destination. */

static int cfrommap(int c);
static char* fromimap(char* destp, char* destend, char* src)
{
	struct string dest;
	unsigned char *p = (unsigned char*) src;
	int v = 0;
	int i = 0;
	int state = 0;
	int c;

	*destp = 0;
	string_init(&dest, destp, destend-destp);
	/* lprintf(CTDL_DEBUG, "fromimap %s\r\n", src); */

	do {
		c = *p++;
		switch (state)
		{
			case 0:
				/* US-ASCII characters. */
				
				if (c == '&')
					state = 1;
				else
					string_append_c(&dest, cfrommap(c));
				break;

			case 1:
				if (c == '-')
				{
					string_append_c(&dest, '&');
					state = 0;
				}
				else if (utf7_rank[c] != 0xff)
				{
					v = utf7_rank[c];
					i = 6;
					state = 2;
				}
				else
				{
					/* invalid char */
					string_append_sn(&dest, "&-", 2);
					state = 0;
				}
				break;
				
			case 2:
				if (c == '-')
					state = 0;
				else if (utf7_rank[c] != 0xFF)
				{
					v = (v<<6) | utf7_rank[c];
					i += 6;
					if (i >= 16)
					{
						int x = (v >> (i-16)) & 0xFFFF;
						string_append_c(&dest, cfrommap(x));
						i -= 16;
					}
				}
				else
				{
					string_append_c(&dest, cfrommap(c));
					state = 0;
				}
				break;
			}
	} while (c != '\0');

	/* lprintf(CTDL_DEBUG, "      -> %s\r\n", destp); */
	return string_end(&dest);
}

/* Undoes the special character conversion. */

static int cfrommap(int c)
{
	switch (c)
	{
		case '|':	return '/';
		case '/':	return '\\';
	}
	return c;		
}

/* Output a string to the IMAP client, either as a literal or quoted.
 * (We do a literal if it has any double-quotes or backslashes.) */

void imap_strout(char *buf)
{
	int i;
	int is_literal = 0;
	long len;

	if (buf == NULL) {	/* yeah, we handle this */
		cprintf("NIL");
		return;
	}

	len = strlen(buf);
	for (i = 0; i < len; ++i) {
		if ((buf[i] == '\"') || (buf[i] == '\\'))
			is_literal = 1;
	}

	if (is_literal) {
		cprintf("{%ld}\r\n%s", len, buf);
	} else {
		cprintf("\"%s\"", buf);
	}
}

/* Break a command down into tokens, unquoting any escaped characters. */

int imap_parameterize(char** args, char* in)
{
	char* out = in;
	int num = 0;

	for (;;)
	{
		/* Skip whitespace. */

		while (isspace(*in))
			in++;
		if (*in == 0)
			break;

		/* Found the start of a token. */
		
		args[num++] = out;

		/* Read in the token. */

		for (;;)
		{
			int c = *in++;
			if (isspace(c))
				break;
			
			if (c == '\"')
			{
				/* Found a quoted section. */

				for (;;)
				{
					c = *in++;
					if (c == '\"')
						break;
					else if (c == '\\')
						c = *in++;

					*out++ = c;
					if (c == 0)
						return num;
				}
			}
			else if (c == '\\')
			{
				c = *in++;
				*out++ = c;
			}
			else
				*out++ = c;

			if (c == 0)
				return num;
		}
		*out++ = '\0';
	}

	return num;
}

/* Convert a struct ctdlroom to an IMAP-compatible mailbox name. */

void imap_mailboxname(char *buf, int bufsize, struct ctdlroom *qrbuf)
{
	char* bufend = buf+bufsize;
	struct floor *fl;
	char* p = buf;

	/* For mailboxes, just do it straight.
	 * Do the Cyrus-compatible thing: all private folders are
	 * subfolders of INBOX. */

	if (qrbuf->QRflags & QR_MAILBOX)
	{
		if (strcasecmp(qrbuf->QRname+11, MAILROOM) == 0)
			p = toimap(p, bufend, "INBOX");
		else
		{
			p = toimap(p, bufend, "INBOX");
			if (p < bufend)
				*p++ = '/';
			p = toimap(p, bufend, qrbuf->QRname+11);
		}
	}
	else
	{
		/* Otherwise, prefix the floor name as a "public folders" moniker. */

		fl = cgetfloor(qrbuf->QRfloor);
		p = toimap(p, bufend, fl->f_name);
		if (p < bufend)
			*p++ = '/';
		p = toimap(p, bufend, qrbuf->QRname);
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
	char floorname[256];
	char roomname[ROOMNAMELEN];
	int i;
	struct floor *fl;
	int ret = (-1);

	if (foldername == NULL)
		return(-1);

	/* Unmunge the entire string into the output buffer. */

	fromimap(rbuf, rbuf+bufsize, foldername);

	/* Is this an IMAP inbox? */

	if (strncasecmp(rbuf, "INBOX", 5) == 0)
	{
		if (rbuf[5] == 0)
		{
			/* It's the system inbox. */

			safestrncpy(rbuf, MAILROOM, bufsize);
			ret = (0 | IR_MAILBOX);
			goto exit;
		}
		else if (rbuf[5] == FDELIM)
		{
			/* It's another personal mail folder. */

			safestrncpy(rbuf, rbuf+6, bufsize);
			ret = (0 | IR_MAILBOX);
			goto exit;
		}

		/* If we get here, the folder just happens to start with INBOX
		 * --- fall through. */
	}

	/* Is this a multi-level room name? */

	levels = num_tokens(rbuf, FDELIM);
	if (levels > 1)
	{
		/* Extract the main room name. */
		
		extract_token(floorname, rbuf, 0, FDELIM, sizeof floorname);
		strcpy(roomname, &rbuf[strlen(floorname)+1]);

		/* Try and find it on any floor. */
		
		for (i = 0; i < MAXFLOORS; ++i)
		{
			fl = cgetfloor(i);
			if (fl->f_flags & F_INUSE)
			{
				if (strcasecmp(floorname, fl->f_name) == 0)
				{
					/* Got it! */

					safestrncpy(rbuf, roomname, bufsize);
					ret = i;
					goto exit;
				}
			}
		}
	}

	/* Meh. It's either not a multi-level room name, or else we
	 * couldn't find it.
	 */
	ret = (0 | IR_MAILBOX);

exit:
	lprintf(CTDL_DEBUG, "(That translates to \"%s\")\n", rbuf);
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
	if (IsEmptyStr(buf))
		return (0);

	if (!strcasecmp(buf, "ALL"))
		return (1);	/* macro?  why?  */

	for (i = 0; buf[i]; ++i) {	/* now start the scan */
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
#define WILDMAT_DELIM 	'/'

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
	char daystr[256];
	char monthstr[256];
	char yearstr[256];
	int i;
	int day, month, year;
	int msgday, msgmonth, msgyear;
	struct tm msgtm;

	if (datestr == NULL) return(0);

	/* Expecting a date in the form dd-Mmm-yyyy */
	extract_token(daystr, datestr, 0, '-', sizeof daystr);
	extract_token(monthstr, datestr, 1, '-', sizeof monthstr);
	extract_token(yearstr, datestr, 2, '-', sizeof yearstr);

	day = atoi(daystr);
	year = atoi(yearstr);
	month = 0;
	for (i=0; i<12; ++i) {
		if (!strcasecmp(monthstr, ascmonths[i])) {
			month = i;
		}
	}

	/* Extract day/month/year from message timestamp */
	localtime_r(&msgtime, &msgtm);
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

