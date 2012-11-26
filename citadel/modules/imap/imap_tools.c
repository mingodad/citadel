/*
 * Utility functions for the IMAP module.
 *
 * Copyright (c) 2001-2009 by the citadel.org team and others, except for
 * most of the UTF7 and UTF8 handling code which was lifted from Evolution.
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

#define SHOW_ME_VAPPEND_PRINTF
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <libcitadel.h>
#include "citadel.h"
#include "sysdep_decls.h"
#include "internet_addressing.h"
#include "serv_imap.h"
#include "imap_tools.h"
#include "ctdl_module.h"

/* String handling helpers */

/* This code uses some pretty nasty string manipulation. To make everything
 * manageable, we use this semi-high-level string manipulation API. Strings are
 * always \0-terminated, despite the fact that we keep track of the size.
 */
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
	char UmlChar[5];
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
		UmlChar[0] = 0xC0 | (c >> 6);
		UmlChar[1] = 0x80 | (c & 0x3F);
		len = 2;
	}
	else if (c <= 0xFFFF)
	{
		UmlChar[0] = 0xE0 | (c >> 12);
		UmlChar[1] = 0x80 | ((c >> 6) & 0x3f);
		UmlChar[2] = 0x80 | (c & 0x3f);
		len = 3;
	}
	else
	{
		UmlChar[0] = 0xf0 | c >> 18;
		UmlChar[1] = 0x80 | ((c >> 12) & 0x3f);
		UmlChar[2] = 0x80 | ((c >> 6) & 0x3f);
		UmlChar[3] = 0x80 | (c & 0x3f);
		len = 4;
	}

	string_append_sn(s, UmlChar, len);
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
	/* IMAP_syslog(LOG_DEBUG, "toimap %s", src); */

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
	/* IMAP_syslog(LOG_DEBUG, "    -> %s", destp); */
	return string_end(&dest);
}

/* Convert from an IMAP-safe name back into a Citadel name. Returns the end of the destination. */

static int cfrommap(int c);
static char* fromimap(char* destp, char* destend, const char* src)
{
	struct string dest;
	unsigned const char *p = (unsigned const char*) src;
	int v = 0;
	int i = 0;
	int state = 0;
	int c;

	*destp = 0;
	string_init(&dest, destp, destend-destp);
	/* IMAP_syslog(LOG_DEBUG, "fromimap %s", src); */

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

	/* IMAP_syslog(LOG_DEBUG, "      -> %s", destp); */
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




/* Break a command down into tokens, unquoting any escaped characters. */

void MakeStringOf(StrBuf *Buf, int skip)
{
	int i;
	citimap_command *Cmd = &IMAP->Cmd;

	for (i=skip; i<Cmd->num_parms; ++i) {
		StrBufAppendBufPlain(Buf, Cmd->Params[i].Key, Cmd->Params[i].len, 0);
		if (i < (Cmd->num_parms-1)) StrBufAppendBufPlain(Buf, HKEY(" "), 0);
	}
}


void TokenCutRight(citimap_command *Cmd, 
		   ConstStr *CutMe,
		   int n)
{
	const char *CutAt;

	if (CutMe->len < n) {
		CutAt = CutMe->Key;
		CutMe->len = 0;
	}
	else {
		CutAt = CutMe->Key + CutMe->len - n;
		CutMe->len -= n;
	}
	StrBufPeek(Cmd->CmdBuf, CutAt, -1, '\0');
}

void TokenCutLeft(citimap_command *Cmd, 
		  ConstStr *CutMe,
		  int n)
{
	if (CutMe->len < n) {
		CutMe->Key += CutMe->len;
		CutMe->len = 0;
	}
	else {
		CutMe->Key += n;
		CutMe->len -= n;
	}
}



int CmdAdjust(citimap_command *Cmd, 
	      int nArgs,
	      int Realloc)
{
	ConstStr *Params;
	if (nArgs > Cmd->avail_parms) {
		Params = (ConstStr*) malloc(sizeof(ConstStr) * nArgs);
		if (Realloc) {
			memcpy(Params, 
			       Cmd->Params, 
			       sizeof(ConstStr) * Cmd->avail_parms);

			memset(Cmd->Params + 
			       sizeof(ConstStr) * Cmd->avail_parms,
			       0, 
			       sizeof(ConstStr) * nArgs - 
			       sizeof(ConstStr) * Cmd->avail_parms 
				);
		}
		else {
			Cmd->num_parms = 0;
			memset(Params, 0, 
			       sizeof(ConstStr) * nArgs);
		}
		Cmd->avail_parms = nArgs;
		if (Cmd->Params != NULL)
			free (Cmd->Params);
		Cmd->Params = Params;
	}
	else {
		if (!Realloc) {
			memset(Cmd->Params, 
			       0,
			       sizeof(ConstStr) * Cmd->avail_parms);
			Cmd->num_parms = 0;
		}
	}
	return Cmd->avail_parms;
}

int imap_parameterize(citimap_command *Cmd)
{
	int nArgs;
	const char *In, *End;

	In = ChrPtr(Cmd->CmdBuf);
	End = In + StrLength(Cmd->CmdBuf);

	/* we start with 10 chars per arg, maybe we need to realloc later. */
	nArgs = StrLength(Cmd->CmdBuf) / 10 + 10;
	nArgs = CmdAdjust(Cmd, nArgs, 0);
	while (In < End)
	{
		/* Skip whitespace. */
		while (isspace(*In))
			In++;
		if (*In == '\0')
			break;

		/* Found the start of a token. */
		
		Cmd->Params[Cmd->num_parms].Key = In;

		/* Read in the token. */

		for (;;)
		{
			if (isspace(*In))
				break;
			
			if (*In == '\"')
			{
				/* Found a quoted section. */

				Cmd->Params[Cmd->num_parms].Key++; 
				//In++;
				for (;;)
				{
					In++;
					if (*In == '\"') {
						StrBufPeek(Cmd->CmdBuf, In, -1, '\0');
						break;
					}
					else if (*In == '\\')
						In++;

					if (*In == '\0') {
						Cmd->Params[Cmd->num_parms].len = 
							In - Cmd->Params[Cmd->num_parms].Key;
						Cmd->num_parms++;
						return Cmd->num_parms;
					}
				}
				break;
			}
			else if (*In == '\\')
			{
				In++;
			}

			if (*In == '\0') {
				Cmd->Params[Cmd->num_parms].len = 
					In - Cmd->Params[Cmd->num_parms].Key;
				Cmd->num_parms++;
				return Cmd->num_parms;
			}
			In++;
		}
		StrBufPeek(Cmd->CmdBuf, In, -1, '\0');
		Cmd->Params[Cmd->num_parms].len = 
			In - Cmd->Params[Cmd->num_parms].Key;
		if (Cmd->num_parms + 1 >= Cmd->avail_parms) {
			nArgs = CmdAdjust(Cmd, nArgs * 2, 1);
		}
		Cmd->num_parms ++;
		In++;
	}
	return Cmd->num_parms;
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
			toimap(p, bufend, "INBOX");
		else
		{
			p = toimap(p, bufend, "INBOX");
			if (p < bufend)
				*p++ = '/';
			toimap(p, bufend, qrbuf->QRname+11);
		}
	}
	else
	{
		/* Otherwise, prefix the floor name as a "public folders" moniker. */

		fl = CtdlGetCachedFloor(qrbuf->QRfloor);
		p = toimap(p, bufend, fl->f_name);
		if (p < bufend)
			*p++ = '/';
		toimap(p, bufend, qrbuf->QRname);
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

int imap_roomname(char *rbuf, int bufsize, const char *foldername)
{
	struct CitContext *CCC = CC;
	int levels;
	char floorname[ROOMNAMELEN*2];
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
		long len;
		/* Extract the main room name. */
		
		len = extract_token(floorname, rbuf, 0, FDELIM, sizeof floorname);
		if (len < 0) len = 0;
		safestrncpy(roomname, &rbuf[len  + 1], sizeof(roomname));

		/* Try and find it on any floor. */
		
		for (i = 0; i < MAXFLOORS; ++i)
		{
			fl = CtdlGetCachedFloor(i);
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
	IMAP_syslog(LOG_DEBUG, "(That translates to \"%s\")", rbuf);
	return(ret);
}

/*
 * Output a struct internet_address_list in the form an IMAP client wants
 */
void imap_ial_out(struct internet_address_list *ialist)
{
	struct internet_address_list *iptr;

	if (ialist == NULL) {
		IAPuts("NIL");
		return;
	}
	IAPuts("(");

	for (iptr = ialist; iptr != NULL; iptr = iptr->next) {
		IAPuts("(");
		plain_imap_strout(iptr->ial_name);
		IAPuts(" NIL ");
		plain_imap_strout(iptr->ial_user);
		IAPuts(" ");
		plain_imap_strout(iptr->ial_node);
		IAPuts(")");
	}

	IAPuts(")");
}



/*
 * Determine whether the supplied string is a valid message set.
 * If the string contains only numbers, colons, commas, and asterisks,
 * return 1 for a valid message set.  If any other character is found, 
 * return 0.
 */
int imap_is_message_set(const char *buf)
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
	char *text;
	char *p;
	
	/* Copy both strings and lowercase them, in order to
	 * make this entire operation case-insensitive.
	 */
	for (i=0; 
	     ((supplied_text[i] != '\0') && 
	      (i < sizeof(lcase_text)));
	     ++i)
		lcase_text[i] = tolower(supplied_text[i]);
	lcase_text[i] = '\0';

	for (i=0; 
	     ((supplied_p[i] != '\0') && 
	      (i < sizeof(lcase_p))); 
	     ++i)
		lcase_p[i] = tolower(supplied_p[i]);
	lcase_p[i] = '\0';

	/* Start matching */
	for (p = lcase_p, text = lcase_text; 
	     !IsEmptyStr(p) && !IsEmptyStr(text); 
	     text++, p++) {
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
			while (++p, (!IsEmptyStr(p) && ((*p == '*') || (*p == '%'))))
			{
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
				while (!IsEmptyStr(text)) {
					if (*text == WILDMAT_DELIM) {
						return WILDMAT_FALSE;
					}
					text++;
				}
				return WILDMAT_TRUE;
			}
			while (!IsEmptyStr(text) &&
			       /* make shure texst - 1 isn't before lcase_p */
			       ((text == lcase_text) || (*(text - 1) != WILDMAT_DELIM)))
			{
				if ((matched = do_imap_match(text++, p))
				   != WILDMAT_FALSE) {
					return matched;
				}
			}
			return WILDMAT_ABORT;
		}
	}

	if ((*text == '\0') && (*p == '\0')) return WILDMAT_TRUE;
	else return WILDMAT_FALSE;
}



/*
 * Support function for mailbox pattern name matching in LIST and LSUB
 * Returns nonzero if the supplied mailbox name matches the supplied pattern.
 */
int imap_mailbox_matches_pattern(const char *pattern, char *mailboxname)
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
int imap_datecmp(const char *datestr, time_t msgtime) {
	char daystr[256];
	char monthstr[256];
	char yearstr[256];
	int i;
	int day, month, year;
	int msgday, msgmonth, msgyear;
	struct tm msgtm;

	char *imap_datecmp_ascmonths[12] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};

	if (datestr == NULL) return(0);

	/* Expecting a date in the form dd-Mmm-yyyy */
	extract_token(daystr, datestr, 0, '-', sizeof daystr);
	extract_token(monthstr, datestr, 1, '-', sizeof monthstr);
	extract_token(yearstr, datestr, 2, '-', sizeof yearstr);

	day = atoi(daystr);
	year = atoi(yearstr);
	month = 0;
	for (i=0; i<12; ++i) {
		if (!strcasecmp(monthstr, imap_datecmp_ascmonths[i])) {
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





void IAPrintf(const char *Format, ...)
{
	va_list arg_ptr;
	
	va_start(arg_ptr, Format);
	StrBufVAppendPrintf(IMAP->Reply, Format, arg_ptr);
	va_end(arg_ptr);
}

void iaputs(const char *Str, long Len)
{
	StrBufAppendBufPlain(IMAP->Reply, Str, Len, 0);
}

void ireply(const char *Msg, long len)
{
	citimap *Imap = IMAP;

	StrBufAppendBufPlain(Imap->Reply, 
			     CKEY(Imap->Cmd.Params[0]), 0);
	StrBufAppendBufPlain(Imap->Reply, 
			     HKEY(" "), 0);
	StrBufAppendBufPlain(Imap->Reply, 
			     Msg, len, 0);
	
	StrBufAppendBufPlain(Imap->Reply, 
			     HKEY("\r\n"), 0);
	
}

void IReplyPrintf(const char *Format, ...)
{
	citimap *Imap = IMAP;
	va_list arg_ptr;
	

	StrBufAppendBufPlain(Imap->Reply, 
			     CKEY(Imap->Cmd.Params[0]), 0);

	StrBufAppendBufPlain(Imap->Reply, 
			     HKEY(" "), 0);

	va_start(arg_ptr, Format);
	StrBufVAppendPrintf(IMAP->Reply, Format, arg_ptr);
	va_end(arg_ptr);
	
	StrBufAppendBufPlain(Imap->Reply, 
			     HKEY("\r\n"), 0);
	
}



/* Output a string to the IMAP client, either as a literal or quoted.
 * (We do a literal if it has any double-quotes or backslashes.) */

void plain_imap_strout(char *buf)
{
	int i;
	int is_literal = 0;
	long Len;
	citimap *Imap = IMAP;

	if (buf == NULL) {	/* yeah, we handle this */
		IAPuts("NIL");
		return;
	}

	Len = strlen(buf);
	for (i = 0; i < Len; ++i) {
		if ((buf[i] == '\"') || (buf[i] == '\\'))
			is_literal = 1;
	}

	if (is_literal) {
		StrBufAppendPrintf(Imap->Reply, "{%ld}\r\n", Len);
		StrBufAppendBufPlain(Imap->Reply, buf, Len, 0);
	} else {
		StrBufAppendBufPlain(Imap->Reply, 
				     HKEY("\""), 0);
		StrBufAppendBufPlain(Imap->Reply, 
				     buf, Len, 0);
		StrBufAppendBufPlain(Imap->Reply, 
				     HKEY("\""), 0);
	}
}


/* Output a string to the IMAP client, either as a literal or quoted.
 * (We do a literal if it has any double-quotes or backslashes.) */


void IPutStr(const char *Msg, long Len)
{
	int i;
	int is_literal = 0;
	citimap *Imap = IMAP;

	
	if ((Msg == NULL) || (Len == 0))
	{	/* yeah, we handle this */
		StrBufAppendBufPlain(Imap->Reply, HKEY("NIL"), 0);
		return;
	}

	for (i = 0; i < Len; ++i) {
		if ((Msg[i] == '\"') || (Msg[i] == '\\'))
			is_literal = 1;
	}

	if (is_literal) {
		StrBufAppendPrintf(Imap->Reply, "{%ld}\r\n", Len);
		StrBufAppendBufPlain(Imap->Reply, Msg, Len, 0);
	} else {
		StrBufAppendBufPlain(Imap->Reply, 
				     HKEY("\""), 0);
		StrBufAppendBufPlain(Imap->Reply, 
				     Msg, Len, 0);
		StrBufAppendBufPlain(Imap->Reply, 
				     HKEY("\""), 0);
	}
}

void IUnbuffer (void)
{
	citimap *Imap = IMAP;

	cputbuf(Imap->Reply);
	FlushStrBuf(Imap->Reply);
}
