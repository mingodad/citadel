/* $Id$ */
#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <syslog.h>
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#include <limits.h>
#include "citadel.h"
#include "server.h"
#include <errno.h>
#include <sys/stat.h>
#include "database.h"
#include "msgbase.h"
#include "support.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "room_ops.h"
#include "user_ops.h"
#include "file_ops.h"
#include "control.h"
#include "dynloader.h"
#include "tools.h"
#include "mime_parser.h"

#define MSGS_ALL	0
#define MSGS_OLD	1
#define MSGS_NEW	2
#define MSGS_FIRST	3
#define MSGS_LAST	4
#define MSGS_GT		5

#define desired_section ((char *)CtdlGetUserData(SYM_DESIRED_SECTION))

extern struct config config;


/*
 * This function is self explanatory.
 * (What can I say, I'm in a weird mood today...)
 */
void remove_any_whitespace_to_the_left_or_right_of_at_symbol(char *name)
{
	int i;

	for (i = 0; i < strlen(name); ++i)
		if (name[i] == '@') {
			if (i > 0)
				if (isspace(name[i - 1])) {
					strcpy(&name[i - 1], &name[i]);
					i = 0;
				}
			while (isspace(name[i + 1])) {
				strcpy(&name[i + 1], &name[i + 2]);
			}
		}
}


/*
 * Aliasing for network mail.
 * (Error messages have been commented out, because this is a server.)
 */
int alias(char *name)
{				/* process alias and routing info for mail */
	FILE *fp;
	int a, b;
	char aaa[300], bbb[300];

	lprintf(9, "alias() called for <%s>\n", name);

	remove_any_whitespace_to_the_left_or_right_of_at_symbol(name);

	fp = fopen("network/mail.aliases", "r");
	if (fp == NULL)
		fp = fopen("/dev/null", "r");
	if (fp == NULL)
		return (MES_ERROR);
	strcpy(aaa, "");
	strcpy(bbb, "");
	while (fgets(aaa, sizeof aaa, fp) != NULL) {
		while (isspace(name[0]))
			strcpy(name, &name[1]);
		aaa[strlen(aaa) - 1] = 0;
		strcpy(bbb, "");
		for (a = 0; a < strlen(aaa); ++a) {
			if (aaa[a] == ',') {
				strcpy(bbb, &aaa[a + 1]);
				aaa[a] = 0;
			}
		}
		if (!strcasecmp(name, aaa))
			strcpy(name, bbb);
	}
	fclose(fp);
	lprintf(7, "Mail is being forwarded to %s\n", name);

	/* determine local or remote type, see citadel.h */
	for (a = 0; a < strlen(name); ++a)
		if (name[a] == '!')
			return (MES_INTERNET);
	for (a = 0; a < strlen(name); ++a)
		if (name[a] == '@')
			for (b = a; b < strlen(name); ++b)
				if (name[b] == '.')
					return (MES_INTERNET);
	b = 0;
	for (a = 0; a < strlen(name); ++a)
		if (name[a] == '@')
			++b;
	if (b > 1) {
		lprintf(7, "Too many @'s in address\n");
		return (MES_ERROR);
	}
	if (b == 1) {
		for (a = 0; a < strlen(name); ++a)
			if (name[a] == '@')
				strcpy(bbb, &name[a + 1]);
		while (bbb[0] == 32)
			strcpy(bbb, &bbb[1]);
		fp = fopen("network/mail.sysinfo", "r");
		if (fp == NULL)
			return (MES_ERROR);
	      GETSN:do {
			a = getstring(fp, aaa);
		} while ((a >= 0) && (strcasecmp(aaa, bbb)));
		a = getstring(fp, aaa);
		if (!strncmp(aaa, "use ", 4)) {
			strcpy(bbb, &aaa[4]);
			fseek(fp, 0L, 0);
			goto GETSN;
		}
		fclose(fp);
		if (!strncmp(aaa, "uum", 3)) {
			strcpy(bbb, name);
			for (a = 0; a < strlen(bbb); ++a) {
				if (bbb[a] == '@')
					bbb[a] = 0;
				if (bbb[a] == ' ')
					bbb[a] = '_';
			}
			while (bbb[strlen(bbb) - 1] == '_')
				bbb[strlen(bbb) - 1] = 0;
			sprintf(name, &aaa[4], bbb);
			return (MES_INTERNET);
		}
		if (!strncmp(aaa, "bin", 3)) {
			strcpy(aaa, name);
			strcpy(bbb, name);
			while (aaa[strlen(aaa) - 1] != '@')
				aaa[strlen(aaa) - 1] = 0;
			aaa[strlen(aaa) - 1] = 0;
			while (aaa[strlen(aaa) - 1] == ' ')
				aaa[strlen(aaa) - 1] = 0;
			while (bbb[0] != '@')
				strcpy(bbb, &bbb[1]);
			strcpy(bbb, &bbb[1]);
			while (bbb[0] == ' ')
				strcpy(bbb, &bbb[1]);
			sprintf(name, "%s @%s", aaa, bbb);
			return (MES_BINARY);
		}
		return (MES_ERROR);
	}
	return (MES_LOCAL);
}


void get_mm(void)
{
	FILE *fp;

	fp = fopen("citadel.control", "r");
	fread((char *) &CitControl, sizeof(struct CitControl), 1, fp);
	fclose(fp);
}



void simple_listing(long msgnum) {
	cprintf("%ld\n", msgnum);
}


/*
 * API function to perform an operation for each qualifying message in the
 * current room.
 */
void CtdlForEachMessage(int mode, long ref,
			void (*CallBack) (long msgnum) ) {

	int a;
	struct visit vbuf;

	get_mm();
	get_msglist(&CC->quickroom);
	getuser(&CC->usersupp, CC->curr_user);
	CtdlGetRelationship(&vbuf, &CC->usersupp, &CC->quickroom);

	if (CC->num_msgs != 0) for (a = 0; a < (CC->num_msgs); ++a) {
		if ((MessageFromList(a) >= 0)
		    && (

			       (mode == MSGS_ALL)
			       || ((mode == MSGS_OLD) && (MessageFromList(a) <= vbuf.v_lastseen))
			       || ((mode == MSGS_NEW) && (MessageFromList(a) > vbuf.v_lastseen))
			       || ((mode == MSGS_NEW) && (MessageFromList(a) >= vbuf.v_lastseen)
			    && (CC->usersupp.flags & US_LASTOLD))
			       || ((mode == MSGS_LAST) && (a >= (CC->num_msgs - ref)))
		    || ((mode == MSGS_FIRST) && (a < ref))
			       || ((mode == MSGS_GT) && (MessageFromList(a) > ref))
		    )
		    ) {
			CallBack(MessageFromList(a));
		}
	}
}



/*
 * cmd_msgs()  -  get list of message #'s in this room
 *                implements the MSGS server command using CtdlForEachMessage()
 */
void cmd_msgs(char *cmdbuf)
{
	int mode = 0;
	char which[256];
	int cm_ref = 0;

	extract(which, cmdbuf, 0);
	cm_ref = extract_int(cmdbuf, 1);

	mode = MSGS_ALL;
	strcat(which, "   ");
	if (!strncasecmp(which, "OLD", 3))
		mode = MSGS_OLD;
	else if (!strncasecmp(which, "NEW", 3))
		mode = MSGS_NEW;
	else if (!strncasecmp(which, "FIRST", 5))
		mode = MSGS_FIRST;
	else if (!strncasecmp(which, "LAST", 4))
		mode = MSGS_LAST;
	else if (!strncasecmp(which, "GT", 2))
		mode = MSGS_GT;

	if ((!(CC->logged_in)) && (!(CC->internal_pgm))) {
		cprintf("%d not logged in\n", ERROR + NOT_LOGGED_IN);
		return;
	}

	cprintf("%d Message list...\n", LISTING_FOLLOWS);
	CtdlForEachMessage(mode, cm_ref, simple_listing);
	cprintf("000\n");
}




/* 
 * help_subst()  -  support routine for help file viewer
 */
void help_subst(char *strbuf, char *source, char *dest)
{
	char workbuf[256];
	int p;

	while (p = pattern2(strbuf, source), (p >= 0)) {
		strcpy(workbuf, &strbuf[p + strlen(source)]);
		strcpy(&strbuf[p], dest);
		strcat(strbuf, workbuf);
	}
}


void do_help_subst(char *buffer)
{
	char buf2[16];

	help_subst(buffer, "^nodename", config.c_nodename);
	help_subst(buffer, "^humannode", config.c_humannode);
	help_subst(buffer, "^fqdn", config.c_fqdn);
	help_subst(buffer, "^username", CC->usersupp.fullname);
	sprintf(buf2, "%ld", CC->usersupp.usernum);
	help_subst(buffer, "^usernum", buf2);
	help_subst(buffer, "^sysadm", config.c_sysadm);
	help_subst(buffer, "^variantname", CITADEL);
	sprintf(buf2, "%d", config.c_maxsessions);
	help_subst(buffer, "^maxsessions", buf2);
}



/*
 * memfmout()  -  Citadel text formatter and paginator.
 *             Although the original purpose of this routine was to format
 *             text to the reader's screen width, all we're really using it
 *             for here is to format text out to 80 columns before sending it
 *             to the client.  The client software may reformat it again.
 */
void memfmout(int width, char *mptr, char subst)
			/* screen width to use */
			/* where are we going to get our text from? */
			/* nonzero if we should use hypertext mode */
{
	int a, b, c;
	int real = 0;
	int old = 0;
	CIT_UBYTE ch;
	char aaa[140];
	char buffer[256];

	strcpy(aaa, "");
	old = 255;
	strcpy(buffer, "");
	c = 1;			/* c is the current pos */

FMTA:	if (subst) {
		while (ch = *mptr, ((ch != 0) && (strlen(buffer) < 126))) {
			ch = *mptr++;
			buffer[strlen(buffer) + 1] = 0;
			buffer[strlen(buffer)] = ch;
		}

		if (buffer[0] == '^')
			do_help_subst(buffer);

		buffer[strlen(buffer) + 1] = 0;
		a = buffer[0];
		strcpy(buffer, &buffer[1]);
	} else
		ch = *mptr++;

	old = real;
	real = ch;
	if (ch <= 0)
		goto FMTEND;

	if (((ch == 13) || (ch == 10)) && (old != 13) && (old != 10))
		ch = 32;
	if (((old == 13) || (old == 10)) && (isspace(real))) {
		cprintf("\n");
		c = 1;
	}
	if (ch > 126)
		goto FMTA;

	if (ch > 32) {
		if (((strlen(aaa) + c) > (width - 5)) && (strlen(aaa) > (width - 5))) {
			cprintf("\n%s", aaa);
			c = strlen(aaa);
			aaa[0] = 0;
		}
		b = strlen(aaa);
		aaa[b] = ch;
		aaa[b + 1] = 0;
	}
	if (ch == 32) {
		if ((strlen(aaa) + c) > (width - 5)) {
			cprintf("\n");
			c = 1;
		}
		cprintf("%s ", aaa);
		++c;
		c = c + strlen(aaa);
		strcpy(aaa, "");
		goto FMTA;
	}
	if ((ch == 13) || (ch == 10)) {
		cprintf("%s\n", aaa);
		c = 1;
		strcpy(aaa, "");
		goto FMTA;
	}
	goto FMTA;

FMTEND:	cprintf("%s\n", aaa);
}



/*
 * Callback function for mime parser that simply lists the part
 */
void list_this_part(char *name, char *filename, char *partnum, char *disp,
		    void *content, char *cbtype, size_t length)
{

	cprintf("part=%s|%s|%s|%s|%s|%d\n",
		name, filename, partnum, disp, cbtype, length);
}


/*
 * Callback function for mime parser that wants to display text
 */
void fixed_output(char *name, char *filename, char *partnum, char *disp,
		  void *content, char *cbtype, size_t length)
{

	if (!strcasecmp(cbtype, "text/plain")) {
		client_write(content, length);
	} else {
		cprintf("Part %s: %s (%s) (%d bytes)\n",
			partnum, filename, cbtype, length);
	}
}


/*
 * Callback function for mime parser that opens a section for downloading
 */
void mime_download(char *name, char *filename, char *partnum, char *disp,
		   void *content, char *cbtype, size_t length)
{

	/* Silently go away if there's already a download open... */
	if (CC->download_fp != NULL)
		return;

	/* ...or if this is not the desired section */
	if (strcasecmp(desired_section, partnum))
		return;

	CC->download_fp = tmpfile();
	if (CC->download_fp == NULL)
		return;

	fwrite(content, length, 1, CC->download_fp);
	fflush(CC->download_fp);
	rewind(CC->download_fp);

	OpenCmdResult(filename, cbtype);
}



/*
 * Get a message off disk.  (return value is the message's timestamp)
 * 
 */
time_t output_message(char *msgid, int mode, int headers_only)
{
	long msg_num;
	int a;
	CIT_UBYTE ch, rch;
	CIT_UBYTE format_type, anon_flag;
	char buf[1024];
	long msg_len;
	int msg_ok = 0;

	struct cdbdata *dmsgtext;
	char *mptr;

	/* buffers needed for RFC822 translation */
	char suser[256];
	char luser[256];
	char snode[256];
	char lnode[256];
	char mid[256];
	time_t xtime = 0L;
	/*                                       */

	msg_num = atol(msgid);


	if ((!(CC->logged_in)) && (!(CC->internal_pgm)) && (mode != MT_DATE)) {
		cprintf("%d Not logged in.\n", ERROR + NOT_LOGGED_IN);
		return (xtime);
	}
	/* We used to need to check in the current room's message list
	 * to determine where the message's disk position.  We no longer need
	 * to do this, but we do it anyway as a security measure, in order to
	 * prevent rogue clients from reading messages not in the current room.
	 */

	msg_ok = 0;
	if (CC->num_msgs > 0) {
		for (a = 0; a < CC->num_msgs; ++a) {
			if (MessageFromList(a) == msg_num) {
				msg_ok = 1;
			}
		}
	}
	if (!msg_ok) {
		if (mode != MT_DATE)
			cprintf("%d Message %ld is not in this room.\n",
				ERROR, msg_num);
		return (xtime);
	}
	dmsgtext = cdb_fetch(CDB_MSGMAIN, &msg_num, sizeof(long));

	if (dmsgtext == NULL) {
		if (mode != MT_DATE)
			cprintf("%d Can't find message %ld\n",
				ERROR + INTERNAL_ERROR);
		return (xtime);
	}
	msg_len = (long) dmsgtext->len;
	mptr = dmsgtext->ptr;

	/* this loop spews out the whole message if we're doing raw format */
	if (mode == MT_RAW) {
		cprintf("%d %ld\n", BINARY_FOLLOWS, msg_len);
		client_write(dmsgtext->ptr, (int) msg_len);
		cdb_free(dmsgtext);
		return (xtime);
	}
	/* Otherwise, we'll start parsing it field by field... */
	ch = *mptr++;
	if (ch != 255) {
		cprintf("%d Illegal message format on disk\n",
			ERROR + INTERNAL_ERROR);
		cdb_free(dmsgtext);
		return (xtime);
	}
	anon_flag = *mptr++;
	format_type = *mptr++;

	/* Are we downloading a MIME component? */
	if (mode == MT_DOWNLOAD) {
		if (format_type != 4) {
			cprintf("%d This is not a MIME message.\n",
				ERROR);
		} else if (CC->download_fp != NULL) {
			cprintf("%d You already have a download open.\n",
				ERROR);
		} else {
			/* Skip to the message body */
			while (ch = *mptr++, (ch != 'M' && ch != 0)) {
				buf[0] = 0;
				do {
					buf[strlen(buf) + 1] = 0;
					rch = *mptr++;
					buf[strlen(buf)] = rch;
				} while (rch > 0);
			}
			/* Now parse it */
			mime_parser(mptr, NULL, *mime_download);
			/* If there's no file open by this time, the requested
			 * section wasn't found, so print an error
			 */
			if (CC->download_fp == NULL) {
				cprintf("%d Section %s not found.\n",
					ERROR + FILE_NOT_FOUND,
					desired_section);
			}
		}
		cdb_free(dmsgtext);
		return (xtime);
	}
	/* Are we just looking for the message date? */
	if (mode == MT_DATE)
		while (ch = *mptr++, (ch != 'M' && ch != 0)) {
			buf[0] = 0;
			do {
				buf[strlen(buf) + 1] = 0;
				rch = *mptr++;
				buf[strlen(buf)] = rch;
			} while (rch > 0);

			if (ch == 'T') {
				xtime = atol(buf);
				cdb_free(dmsgtext);
				return (xtime);
			}
		}
	/* now for the user-mode message reading loops */
	cprintf("%d Message %ld:\n", LISTING_FOLLOWS, msg_num);

	if (mode == MT_CITADEL)
		cprintf("type=%d\n", format_type);

	if ((anon_flag == MES_ANON) && (mode == MT_CITADEL)) {
		cprintf("nhdr=yes\n");
	}
	/* begin header processing loop for Citadel message format */

	if ((mode == MT_CITADEL) || (mode == MT_MIME))
		while (ch = *mptr++, (ch != 'M' && ch != 0)) {
			buf[0] = 0;
			do {
				buf[strlen(buf) + 1] = 0;
				rch = *mptr++;
				buf[strlen(buf)] = rch;
			} while (rch > 0);

			if (ch == 'A') {
				PerformUserHooks(buf, (-1L), EVT_OUTPUTMSG);
				if (anon_flag == MES_ANON)
					cprintf("from=****");
				else if (anon_flag == MES_AN2)
					cprintf("from=anonymous");
				else
					cprintf("from=%s", buf);
				if ((is_room_aide()) && ((anon_flag == MES_ANON)
					      || (anon_flag == MES_AN2)))
					cprintf(" [%s]", buf);
				cprintf("\n");
			} else if (ch == 'P')
				cprintf("path=%s\n", buf);
			else if (ch == 'U')
				cprintf("subj=%s\n", buf);
			else if (ch == 'I')
				cprintf("msgn=%s\n", buf);
			else if (ch == 'H')
				cprintf("hnod=%s\n", buf);
			else if (ch == 'O')
				cprintf("room=%s\n", buf);
			else if (ch == 'N')
				cprintf("node=%s\n", buf);
			else if (ch == 'R')
				cprintf("rcpt=%s\n", buf);
			else if (ch == 'T')
				cprintf("time=%s\n", buf);
			/* else cprintf("fld%c=%s\n",ch,buf); */
		}
	/* begin header processing loop for RFC822 transfer format */

	strcpy(suser, "");
	strcpy(luser, "");
	strcpy(snode, NODENAME);
	strcpy(lnode, HUMANNODE);
	if (mode == MT_RFC822)
		while (ch = *mptr++, (ch != 'M' && ch != 0)) {
			buf[0] = 0;
			do {
				buf[strlen(buf) + 1] = 0;
				rch = *mptr++;
				buf[strlen(buf)] = rch;
			} while (rch > 0);

			if (ch == 'A')
				strcpy(luser, buf);
			else if (ch == 'P') {
				cprintf("Path: %s\n", buf);
				for (a = 0; a < strlen(buf); ++a) {
					if (buf[a] == '!') {
						strcpy(buf, &buf[a + 1]);
						a = 0;
					}
				}
				strcpy(suser, buf);
			} else if (ch == 'U')
				cprintf("Subject: %s\n", buf);
			else if (ch == 'I')
				strcpy(mid, buf);
			else if (ch == 'H')
				strcpy(lnode, buf);
			else if (ch == 'O')
				cprintf("X-Citadel-Room: %s\n", buf);
			else if (ch == 'N')
				strcpy(snode, buf);
			else if (ch == 'R')
				cprintf("To: %s\n", buf);
			else if (ch == 'T') {
				xtime = atol(buf);
				cprintf("Date: %s", asctime(localtime(&xtime)));
			}
		}
	if (mode == MT_RFC822) {
		if (!strcasecmp(snode, NODENAME)) {
			strcpy(snode, FQDN);
		}
		cprintf("Message-ID: <%s@%s>\n", mid, snode);
		PerformUserHooks(luser, (-1L), EVT_OUTPUTMSG);
		cprintf("From: %s@%s (%s)\n",
			suser, snode, luser);
		cprintf("Organization: %s\n", lnode);
	}
	/* end header processing loop ... at this point, we're in the text */

	if (ch == 0) {
		cprintf("text\n*** ?Message truncated\n000\n");
		cdb_free(dmsgtext);
		return (xtime);
	}
	/* do some sort of MIME output */
	if (format_type == 4) {
		if ((mode == MT_CITADEL) || (mode == MT_MIME)) {
			mime_parser(mptr, NULL, *list_this_part);
		}
		if (mode == MT_MIME) {	/* If MT_MIME then it's parts only */
			cprintf("000\n");
			cdb_free(dmsgtext);
			return (xtime);
		}
	}
	if (headers_only) {
		/* give 'em a length */
		msg_len = 0L;
		while (ch = *mptr++, ch > 0) {
			++msg_len;
		}
		cprintf("mlen=%ld\n", msg_len);
		cprintf("000\n");
		cdb_free(dmsgtext);
		return (xtime);
	}
	/* signify start of msg text */
	if (mode == MT_CITADEL)
		cprintf("text\n");
	if ((mode == MT_RFC822) && (format_type != 4))
		cprintf("\n");

	/* If the format type on disk is 1 (fixed-format), then we want
	 * everything to be output completely literally ... regardless of
	 * what message transfer format is in use.
	 */
	if (format_type == 1) {
		strcpy(buf, "");
		while (ch = *mptr++, ch > 0) {
			if (ch == 13)
				ch = 10;
			if ((ch == 10) || (strlen(buf) > 250)) {
				cprintf("%s\n", buf);
				strcpy(buf, "");
			} else {
				buf[strlen(buf) + 1] = 0;
				buf[strlen(buf)] = ch;
			}
		}
		if (strlen(buf) > 0)
			cprintf("%s\n", buf);
	}
	/* If the message on disk is format 0 (Citadel vari-format), we
	 * output using the formatter at 80 columns.  This is the final output
	 * form if the transfer format is RFC822, but if the transfer format
	 * is Citadel proprietary, it'll still work, because the indentation
	 * for new paragraphs is correct and the client will reformat the
	 * message to the reader's screen width.
	 */
	if (format_type == 0) {
		memfmout(80, mptr, 0);
	}
	/* If the message on disk is format 4 (MIME), we've gotta hand it
	 * off to the MIME parser.  The client has already been told that
	 * this message is format 1 (fixed format), so the callback function
	 * we use will display those parts as-is.
	 */
	if (format_type == 4) {
		mime_parser(mptr, NULL, *fixed_output);
	}
	/* now we're done */
	cprintf("000\n");
	cdb_free(dmsgtext);
	return (xtime);
}


/*
 * display a message (mode 0 - Citadel proprietary)
 */
void cmd_msg0(char *cmdbuf)
{
	char msgid[256];
	int headers_only = 0;

	extract(msgid, cmdbuf, 0);
	headers_only = extract_int(cmdbuf, 1);

	output_message(msgid, MT_CITADEL, headers_only);
	return;
}


/*
 * display a message (mode 2 - RFC822)
 */
void cmd_msg2(char *cmdbuf)
{
	char msgid[256];
	int headers_only = 0;

	extract(msgid, cmdbuf, 0);
	headers_only = extract_int(cmdbuf, 1);

	output_message(msgid, MT_RFC822, headers_only);
}

/* 
 * display a message (mode 3 - IGnet raw format - internal programs only)
 */
void cmd_msg3(char *cmdbuf)
{
	char msgid[256];
	int headers_only = 0;

	if (CC->internal_pgm == 0) {
		cprintf("%d This command is for internal programs only.\n",
			ERROR);
		return;
	}
	extract(msgid, cmdbuf, 0);
	headers_only = extract_int(cmdbuf, 1);

	output_message(msgid, MT_RAW, headers_only);
}

/* 
 * display a message (mode 4 - MIME) (FIX ... still evolving, not complete)
 */
void cmd_msg4(char *cmdbuf)
{
	char msgid[256];

	extract(msgid, cmdbuf, 0);

	output_message(msgid, MT_MIME, 0);
}



/*
 * Open a component of a MIME message as a download file 
 */
void cmd_opna(char *cmdbuf)
{
	char msgid[256];

	CtdlAllocUserData(SYM_DESIRED_SECTION, 64);

	extract(msgid, cmdbuf, 0);
	extract(desired_section, cmdbuf, 1);

	output_message(msgid, MT_DOWNLOAD, 0);
}



/*
 * Message base operation to send a message to the master file
 * (returns new message number)
 */
long send_message(char *message_in_memory,	/* pointer to buffer */
		  size_t message_length,	/* length of buffer */
		  int generate_id)
{				/* 1 to generate an I field */

	long newmsgid;
	char *actual_message;
	size_t actual_length;
	long retval;
	char msgidbuf[32];

	/* Get a new message number */
	newmsgid = get_new_message_number();

	if (generate_id) {
		sprintf(msgidbuf, "I%ld", newmsgid);
		actual_length = message_length + strlen(msgidbuf) + 1;
		actual_message = mallok(actual_length);
		memcpy(actual_message, message_in_memory, 3);
		memcpy(&actual_message[3], msgidbuf, (strlen(msgidbuf) + 1));
		memcpy(&actual_message[strlen(msgidbuf) + 4],
		       &message_in_memory[3], message_length - 3);
	} else {
		actual_message = message_in_memory;
		actual_length = message_length;
	}

	/* Write our little bundle of joy into the message base */
	begin_critical_section(S_MSGMAIN);
	if (cdb_store(CDB_MSGMAIN, &newmsgid, sizeof(long),
		      actual_message, actual_length) < 0) {
		lprintf(2, "Can't store message\n");
		retval = 0L;
	} else {
		retval = newmsgid;
	}
	end_critical_section(S_MSGMAIN);

	if (generate_id) {
		phree(actual_message);
	}
	/* Finally, return the pointers */
	return (retval);
}



/*
 * this is a simple file copy routine.
 */
void copy_file(char *from, char *to)
{
	FILE *ffp, *tfp;
	int a;

	ffp = fopen(from, "r");
	if (ffp == NULL)
		return;
	tfp = fopen(to, "w");
	if (tfp == NULL) {
		fclose(ffp);
		return;
	}
	while (a = getc(ffp), a >= 0) {
		putc(a, tfp);
	}
	fclose(ffp);
	fclose(tfp);
	return;
}



/*
 * message base operation to save a message and install its pointers
 */
void save_message(char *mtmp,	/* file containing proper message */
		  char *rec,	/* Recipient (if mail) */
		  char *force,	/* if non-zero length, force a room */
		  int mailtype,	/* local or remote type, see citadel.h */
		  int generate_id)
{				/* set to 1 to generate an 'I' field */
	char aaa[100];
	char hold_rm[ROOMNAMELEN];
	char actual_rm[ROOMNAMELEN];
	char force_room[ROOMNAMELEN];
	char content_type[256];		/* We have to learn this */
	char ch, rch;
	char recipient[256];
	long newmsgid;
	char *message_in_memory;
	char *mptr;
	struct stat statbuf;
	size_t templen;
	FILE *fp;
	struct usersupp userbuf;
	int a;
	static int seqnum = 0;
	int successful_local_recipients = 0;
	struct quickroom qtemp;
	struct SuppMsgInfo smi;

	lprintf(9, "save_message(%s,%s,%s,%d,%d)\n",
		mtmp, rec, force, mailtype, generate_id);

	strcpy(force_room, force);

	/* Strip non-printable characters out of the recipient name */
	strcpy(recipient, rec);
	for (a = 0; a < strlen(recipient); ++a)
		if (!isprint(recipient[a]))
			strcpy(&recipient[a], &recipient[a + 1]);

	/* Measure the message */
	stat(mtmp, &statbuf);
	templen = statbuf.st_size;

	/* Now read it into memory */
	message_in_memory = (char *) mallok(templen);
	if (message_in_memory == NULL) {
		lprintf(2, "Can't allocate memory to save message!\n");
		return;
	}
	fp = fopen(mtmp, "rb");
	fread(message_in_memory, templen, 1, fp);
	fclose(fp);

	/* Learn about what's inside, because it's what's inside that counts */
	mptr = message_in_memory;
	++mptr;	/* advance past 0xFF header */
	++mptr;	/* advance past anon flag */
	ch = *mptr++;
	switch(ch) {
	 case 0:
		strcpy(content_type, "text/x-citadel-variformat");
		break;
	 case 1:
		strcpy(content_type, "text/plain");
		break;
	 case 4:
		strcpy(content_type, "text/plain");
		/* advance past header fields */
		while (ch = *mptr++, (ch != 'M' && ch != 0)) {
			do {
				rch = *mptr++;
			} while (rch > 0);
		}
		a = strlen(mptr);
		while (--a) {
			if (!strncasecmp(mptr, "Content-type: ", 14)) {
				lprintf(9, "%s\n", content_type);
				strcpy(content_type, &content_type[14]);
				for (a=0; a<strlen(content_type); ++a)
					if (  (content_type[a]==';')
					   || (content_type[a]==' ')
					   || (content_type[a]==13)
					   || (content_type[a]==10) )
						content_type[a] = 0;
				break;
			}
			++mptr;
		}
	}
	lprintf(9, "Content type is <%s>\n", content_type);

	/* Save it to disk */
	newmsgid = send_message(message_in_memory, templen, generate_id);
	phree(message_in_memory);
	if (newmsgid <= 0L)
		return;

	strcpy(actual_rm, CC->quickroom.QRname);
	strcpy(hold_rm, "");

	/* If this is being done by the networker delivering a private
	 * message, we want to BYPASS saving the sender's copy (because there
	 * is no local sender; it would otherwise go to the Trashcan).
	 */
	if (!CC->internal_pgm) {
		/* If the user is a twit, move to the twit room for posting */
		if (TWITDETECT)
			if (CC->usersupp.axlevel == 2) {
				strcpy(hold_rm, actual_rm);
				strcpy(actual_rm, config.c_twitroom);
			}
		/* ...or if this message is destined for Aide> then go there. */
		lprintf(9, "actual room forcing loop\n");
		if (strlen(force_room) > 0) {
			strcpy(hold_rm, actual_rm);
			strcpy(actual_rm, force_room);
		}
		/* This call to usergoto() changes rooms if necessary.  It also
	 	* causes the latest message list to be read into memory.
	 	*/
		usergoto(actual_rm, 0);
	
		/* read in the quickroom record, obtaining a lock... */
		lgetroom(&CC->quickroom, actual_rm);
	
		/* Fix an obscure bug */
		if (!strcasecmp(CC->quickroom.QRname, AIDEROOM)) {
			CC->quickroom.QRflags =
				CC->quickroom.QRflags & ~QR_MAILBOX;
		}

		/* Add the message pointer to the room */
		CC->quickroom.QRhighest =
			AddMessageToRoom(&CC->quickroom, newmsgid);
	
		/* update quickroom */
		lputroom(&CC->quickroom);
		++successful_local_recipients;
	}

	/* Network mail - send a copy to the network program. */
	if ((strlen(recipient) > 0) && (mailtype != MES_LOCAL)) {
		sprintf(aaa, "./network/spoolin/netmail.%04lx.%04x.%04x",
			(long) getpid(), CC->cs_pid, ++seqnum);
		copy_file(mtmp, aaa);
		system("exec nohup ./netproc -i >/dev/null 2>&1 &");
	}
	/* Bump this user's messages posted counter. */
	lgetuser(&CC->usersupp, CC->curr_user);
	CC->usersupp.posted = CC->usersupp.posted + 1;
	lputuser(&CC->usersupp);

	/* If this is private, local mail, make a copy in the
	 * recipient's mailbox and bump the reference count.
	 */
	if ((strlen(recipient) > 0) && (mailtype == MES_LOCAL)) {
		if (getuser(&userbuf, recipient) == 0) {
			MailboxName(actual_rm, &userbuf, MAILROOM);
			lprintf(9, "Targeting mailbox: <%s>\n", actual_rm);
			if (lgetroom(&qtemp, actual_rm) == 0) {
				qtemp.QRhighest =
					AddMessageToRoom(&qtemp, newmsgid);
				lputroom(&qtemp);
				++successful_local_recipients;
			}
		}
	}
	/* If we've posted in a room other than the current room, then we
	 * have to now go back to the current room...
	 */
	if (strlen(hold_rm) > 0) {
		usergoto(hold_rm, 0);
	}
	unlink(mtmp);		/* delete the temporary file */

	/* Write a supplemental message info record.  This doesn't have to
	 * be a critical section because nobody else knows about this message
	 * yet.
	 */
	memset(&smi, 0, sizeof(struct SuppMsgInfo));
	smi.smi_msgnum = newmsgid;
	smi.smi_refcount = successful_local_recipients;
	safestrncpy(smi.smi_content_type, content_type, 64);
	PutSuppMsgInfo(&smi);
}


/*
 * Generate an administrative message and post it in the Aide> room.
 */
void aide_message(char *text)
{
	FILE *fp;

	fp = fopen(CC->temp, "wb");
	fprintf(fp, "%c%c%c", 255, MES_NORMAL, 0);
	fprintf(fp, "Psysop%c", 0);
	fprintf(fp, "T%ld%c", (long) time(NULL), 0);
	fprintf(fp, "ACitadel%c", 0);
	fprintf(fp, "OAide%c", 0);
	fprintf(fp, "N%s%c", NODENAME, 0);
	fprintf(fp, "M%s\n%c", text, 0);
	fclose(fp);
	save_message(CC->temp, "", AIDEROOM, MES_LOCAL, 1);
	syslog(LOG_NOTICE, text);
}



/*
 * Build a binary message to be saved on disk.
 */
void make_message(
			 char *filename,	/* temporary file name */
			 struct usersupp *author,	/* author's usersupp structure */
			 char *recipient,	/* NULL if it's not mail */
			 char *room,	/* room where it's going */
			 int type,	/* see MES_ types in header file */
			 int net_type,	/* see MES_ types in header file */
			 int format_type,	/* local or remote (see citadel.h) */
			 char *fake_name)
{				/* who we're masquerading as */

	FILE *fp;
	int a;
	time_t now;
	char dest_node[32];
	char buf[256];

	/* Don't confuse the poor folks if it's not routed mail. */
	strcpy(dest_node, "");


	/* If net_type is MES_BINARY, split out the destination node. */
	if (net_type == MES_BINARY) {
		strcpy(dest_node, NODENAME);
		for (a = 0; a < strlen(recipient); ++a) {
			if (recipient[a] == '@') {
				recipient[a] = 0;
				strcpy(dest_node, &recipient[a + 1]);
			}
		}
	}
	/* if net_type is MES_INTERNET, set the dest node to 'internet' */
	if (net_type == MES_INTERNET) {
		strcpy(dest_node, "internet");
	}
	while (isspace(recipient[strlen(recipient) - 1]))
		recipient[strlen(recipient) - 1] = 0;

	time(&now);
	fp = fopen(filename, "w");
	putc(255, fp);
	putc(type, fp);		/* Normal or anonymous, see MES_ flags */
	putc(format_type, fp);	/* Formatted or unformatted */
	fprintf(fp, "Pcit%ld%c", author->usernum, 0);	/* path */
	fprintf(fp, "T%ld%c", (long) now, 0);	/* date/time */
	if (fake_name[0])
		fprintf(fp, "A%s%c", fake_name, 0);
	else
		fprintf(fp, "A%s%c", author->fullname, 0);	/* author */

	if (CC->quickroom.QRflags & QR_MAILBOX) {	/* room */
		fprintf(fp, "O%s%c", &CC->quickroom.QRname[11], 0);
	} else {
		fprintf(fp, "O%s%c", CC->quickroom.QRname, 0);
	}

	fprintf(fp, "N%s%c", NODENAME, 0);	/* nodename */
	fprintf(fp, "H%s%c", HUMANNODE, 0);	/* human nodename */

	if (recipient[0] != 0)
		fprintf(fp, "R%s%c", recipient, 0);
	if (dest_node[0] != 0)
		fprintf(fp, "D%s%c", dest_node, 0);

	putc('M', fp);

	while (client_gets(buf), strcmp(buf, "000")) {
		fprintf(fp, "%s\n", buf);
	}
	putc(0, fp);
	fclose(fp);
}





/*
 * message entry  -  mode 0 (normal) <bc>
 */
void cmd_ent0(char *entargs)
{
	int post = 0;
	char recipient[256];
	int anon_flag = 0;
	int format_type = 0;
	char newusername[256];	/* <bc> */

	int a, b;
	int e = 0;
	int mtsflag = 0;
	struct usersupp tempUS;
	char buf[256];

	post = extract_int(entargs, 0);
	extract(recipient, entargs, 1);
	anon_flag = extract_int(entargs, 2);
	format_type = extract_int(entargs, 3);

	/* first check to make sure the request is valid. */

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n", ERROR + NOT_LOGGED_IN);
		return;
	}
	if ((CC->usersupp.axlevel < 2) && ((CC->quickroom.QRflags & QR_MAILBOX) == 0)) {
		cprintf("%d Need to be validated to enter ",
			ERROR + HIGHER_ACCESS_REQUIRED);
		cprintf("(except in %s> to sysop)\n", MAILROOM);
		return;
	}
	if ((CC->usersupp.axlevel < 4) && (CC->quickroom.QRflags & QR_NETWORK)) {
		cprintf("%d Need net privileges to enter here.\n",
			ERROR + HIGHER_ACCESS_REQUIRED);
		return;
	}
	if ((CC->usersupp.axlevel < 6) && (CC->quickroom.QRflags & QR_READONLY)) {
		cprintf("%d Sorry, this is a read-only room.\n",
			ERROR + HIGHER_ACCESS_REQUIRED);
		return;
	}
	mtsflag = 0;


	if (post == 2) {	/* <bc> */
		if (CC->usersupp.axlevel < 6) {
			cprintf("%d You don't have permission to do an aide post.\n",
				ERROR + HIGHER_ACCESS_REQUIRED);
			return;
		}
		extract(newusername, entargs, 4);
		memset(CC->fake_postname, 0, 32);
		strcpy(CC->fake_postname, newusername);
		cprintf("%d Ok\n", OK);
		return;
	}
	CC->cs_flags |= CS_POSTING;

	buf[0] = 0;
	if (CC->quickroom.QRflags & QR_MAILBOX) {
		if (CC->usersupp.axlevel >= 2) {
			strcpy(buf, recipient);
		} else
			strcpy(buf, "sysop");
		lprintf(9, "calling alias()\n");
		e = alias(buf);	/* alias and mail type */
		lprintf(9, "alias() returned %d\n", e);
		if ((buf[0] == 0) || (e == MES_ERROR)) {
			cprintf("%d Unknown address - cannot send message.\n",
				ERROR + NO_SUCH_USER);
			return;
		}
		if ((e != MES_LOCAL) && (CC->usersupp.axlevel < 4)) {
			cprintf("%d Net privileges required for network mail.\n",
				ERROR + HIGHER_ACCESS_REQUIRED);
			return;
		}
		if ((RESTRICT_INTERNET == 1) && (e == MES_INTERNET)
		    && ((CC->usersupp.flags & US_INTERNET) == 0)
		    && (!CC->internal_pgm)) {
			cprintf("%d You don't have access to Internet mail.\n",
				ERROR + HIGHER_ACCESS_REQUIRED);
			return;
		}
		if (!strcasecmp(buf, "sysop")) {
			mtsflag = 1;
			goto SKFALL;
		}
		if (e != MES_LOCAL)
			goto SKFALL;	/* don't search local file  */
		if (!strcasecmp(buf, CC->usersupp.fullname)) {
			cprintf("%d Can't send mail to yourself!\n",
				ERROR + NO_SUCH_USER);
			return;
		}
		/* Check to make sure the user exists; also get the correct
		   * upper/lower casing of the name. 
		 */
		a = getuser(&tempUS, buf);
		if (a != 0) {
			cprintf("%d No such user.\n", ERROR + NO_SUCH_USER);
			return;
		}
		strcpy(buf, tempUS.fullname);
	}
      SKFALL:b = MES_NORMAL;
	if (CC->quickroom.QRflags & QR_ANONONLY)
		b = MES_ANON;
	if (CC->quickroom.QRflags & QR_ANONOPT) {
		if (anon_flag == 1)
			b = MES_AN2;
	}
	if ((CC->quickroom.QRflags & QR_MAILBOX) == 0)
		buf[0] = 0;

	/* If we're only checking the validity of the request, return
	 * success without creating the message.
	 */
	if (post == 0) {
		cprintf("%d %s\n", OK, buf);
		return;
	}
	cprintf("%d send message\n", SEND_LISTING);
	if (CC->fake_postname[0])
		make_message(CC->temp, &CC->usersupp, buf, CC->quickroom.QRname, b, e, format_type, CC->fake_postname);
	else if (CC->fake_username[0])
		make_message(CC->temp, &CC->usersupp, buf, CC->quickroom.QRname, b, e, format_type, CC->fake_username);
	else
		make_message(CC->temp, &CC->usersupp, buf, CC->quickroom.QRname, b, e, format_type, "");
	save_message(CC->temp, buf, (mtsflag ? AIDEROOM : ""), e, 1);
	CC->fake_postname[0] = '\0';
	return;
}



/* 
 * message entry - mode 3 (raw)
 */
void cmd_ent3(char *entargs)
{
	char recp[256];
	char buf[256];
	int a;
	int e = 0;
	struct usersupp tempUS;
	long msglen;
	long bloklen;
	FILE *fp;

	if (CC->internal_pgm == 0) {
		cprintf("%d This command is for internal programs only.\n",
			ERROR);
		return;
	}
	/* See if there's a recipient, but make sure it's a real one */
	extract(recp, entargs, 1);
	for (a = 0; a < strlen(recp); ++a)
		if (!isprint(recp[a]))
			strcpy(&recp[a], &recp[a + 1]);
	while (isspace(recp[0]))
		strcpy(recp, &recp[1]);
	while (isspace(recp[strlen(recp) - 1]))
		recp[strlen(recp) - 1] = 0;

	/* If we're in Mail, check the recipient */
	if (strlen(recp) > 0) {
		e = alias(recp);	/* alias and mail type */
		if ((recp[0] == 0) || (e == MES_ERROR)) {
			cprintf("%d Unknown address - cannot send message.\n",
				ERROR + NO_SUCH_USER);
			return;
		}
		if (e == MES_LOCAL) {
			a = getuser(&tempUS, recp);
			if (a != 0) {
				cprintf("%d No such user.\n",
					ERROR + NO_SUCH_USER);
				return;
			}
		}
	}
	/* At this point, message has been approved. */
	if (extract_int(entargs, 0) == 0) {
		cprintf("%d OK to send\n", OK);
		return;
	}
	/* open a temp file to hold the message */
	fp = fopen(CC->temp, "wb");
	if (fp == NULL) {
		cprintf("%d Cannot open %s: %s\n",
			ERROR + INTERNAL_ERROR,
			CC->temp, strerror(errno));
		return;
	}
	msglen = extract_long(entargs, 2);
	cprintf("%d %ld\n", SEND_BINARY, msglen);
	while (msglen > 0L) {
		bloklen = ((msglen >= 255L) ? 255 : msglen);
		client_read(buf, (int) bloklen);
		fwrite(buf, (int) bloklen, 1, fp);
		msglen = msglen - bloklen;
	}
	fclose(fp);

	save_message(CC->temp, recp, "", e, 0);
}


/*
 * API function to delete messages which match a set of criteria
 * (returns the actual number of messages deleted)
 * FIX ... still need to implement delete by content type
 */
int CtdlDeleteMessages(	char *room_name,	/* which room */
			long dmsgnum,		/* or "0" for any */
			char *content_type	/* or NULL for any */
			) {

	struct quickroom qrbuf;
        struct cdbdata *cdbfr;
	long *msglist = NULL;
	int num_msgs = 0;
	int i;
	int num_deleted = 0;
	int delete_this;
	struct SuppMsgInfo smi;

	/* get room record, obtaining a lock... */
	if (lgetroom(&qrbuf, room_name) != 0) {
		lprintf(7, "CtdlDeleteMessages(): Room <%s> not found\n",
			room_name);
		return(0);	/* room not found */
	}

        cdbfr = cdb_fetch(CDB_MSGLISTS, &qrbuf.QRnumber, sizeof(long));

        if (cdbfr != NULL) {
        	msglist = mallok(cdbfr->len);
        	memcpy(msglist, cdbfr->ptr, cdbfr->len);
        	num_msgs = cdbfr->len / sizeof(long);
        	cdb_free(cdbfr);
	}

	if (num_msgs > 0) for (i=0; i<num_msgs; ++i) {
		delete_this = 0x00;

		/* Set/clear a bit for each criterion */

		if ( (dmsgnum == 0L) || (msglist[i]==dmsgnum) ) {
			delete_this  |= 0x01;
		}

		if (content_type == NULL) {
			delete_this |= 0x02;
		} else {
			GetSuppMsgInfo(&smi, msglist[i]);
			if (!strcasecmp(smi.smi_content_type, content_type)) {
				delete_this |= 0x02;
			}
		}

		/* Delete message only if all bits are set */
		if (delete_this == 0x03) {
			AdjRefCount(msglist[i], -1);
			msglist[i] = 0L;
			++num_deleted;
		}
	}

	num_msgs = sort_msglist(msglist, num_msgs);
	cdb_store(CDB_MSGLISTS, &qrbuf.QRnumber, sizeof(long),
		msglist, (num_msgs * sizeof(long)) );

	qrbuf.QRhighest = msglist[num_msgs - 1];
	lputroom(&qrbuf);
	lprintf(9, "%d message(s) deleted.\n", num_deleted);
	return(num_deleted);
}



/*
 * Delete message from current room
 */
void cmd_dele(char *delstr)
{
	long delnum;
	int num_deleted;

	getuser(&CC->usersupp, CC->curr_user);
	if ((CC->usersupp.axlevel < 6)
	    && (CC->usersupp.usernum != CC->quickroom.QRroomaide)
	    && ((CC->quickroom.QRflags & QR_MAILBOX) == 0)) {
		cprintf("%d Higher access required.\n",
			ERROR + HIGHER_ACCESS_REQUIRED);
		return;
	}
	delnum = extract_long(delstr, 0);

	num_deleted = CtdlDeleteMessages(CC->quickroom.QRname, delnum, NULL);

	if (num_deleted) {
		cprintf("%d %d message%s deleted.\n", OK,
			num_deleted, ((num_deleted!=1) ? "s" : "") );
	} else {
		cprintf("%d Message %ld not found.\n", ERROR, delnum);
	}
}


/*
 * move a message to another room
 */
void cmd_move(char *args)
{
	long num;
	char targ[32];
	int a;
	struct quickroom qtemp;
	int foundit;

	num = extract_long(args, 0);
	extract(targ, args, 1);

	getuser(&CC->usersupp, CC->curr_user);
	if ((CC->usersupp.axlevel < 6)
	    && (CC->usersupp.usernum != CC->quickroom.QRroomaide)) {
		cprintf("%d Higher access required.\n",
			ERROR + HIGHER_ACCESS_REQUIRED);
		return;
	}
	if (getroom(&qtemp, targ) != 0) {
		cprintf("%d '%s' does not exist.\n", ERROR, targ);
		return;
	}
	/* yank the message out of the current room... */
	lgetroom(&CC->quickroom, CC->quickroom.QRname);
	get_msglist(&CC->quickroom);

	foundit = 0;
	for (a = 0; a < (CC->num_msgs); ++a) {
		if (MessageFromList(a) == num) {
			foundit = 1;
			SetMessageInList(a, 0L);
		}
	}
	if (foundit) {
		CC->num_msgs = sort_msglist(CC->msglist, CC->num_msgs);
		put_msglist(&CC->quickroom);
		CC->quickroom.QRhighest = MessageFromList((CC->num_msgs) - 1);
	}
	lputroom(&CC->quickroom);
	if (!foundit) {
		cprintf("%d msg %ld does not exist.\n", ERROR, num);
		return;
	}
	/* put the message into the target room */
	lgetroom(&qtemp, targ);
	qtemp.QRhighest = AddMessageToRoom(&qtemp, num);
	lputroom(&qtemp);

	cprintf("%d Message moved.\n", OK);
}



/*
 * GetSuppMsgInfo()  -  Get the supplementary record for a message
 */
void GetSuppMsgInfo(struct SuppMsgInfo *smibuf, long msgnum)
{

	struct cdbdata *cdbsmi;
	long TheIndex;

	memset(smibuf, 0, sizeof(struct SuppMsgInfo));
	smibuf->smi_msgnum = msgnum;
	smibuf->smi_refcount = 1;	/* Default reference count is 1 */

	/* Use the negative of the message number for its supp record index */
	TheIndex = (0L - msgnum);

	cdbsmi = cdb_fetch(CDB_MSGMAIN, &TheIndex, sizeof(long));
	if (cdbsmi == NULL) {
		return;		/* record not found; go with defaults */
	}
	memcpy(smibuf, cdbsmi->ptr,
	       ((cdbsmi->len > sizeof(struct SuppMsgInfo)) ?
		sizeof(struct SuppMsgInfo) : cdbsmi->len));
	cdb_free(cdbsmi);
	return;
}


/*
 * PutSuppMsgInfo()  -  (re)write supplementary record for a message
 */
void PutSuppMsgInfo(struct SuppMsgInfo *smibuf)
{
	long TheIndex;

	/* Use the negative of the message number for its supp record index */
	TheIndex = (0L - smibuf->smi_msgnum);

	lprintf(9, "PuttSuppMsgInfo(%ld) - ref count is %d\n",
		smibuf->smi_msgnum, smibuf->smi_refcount);

	cdb_store(CDB_MSGMAIN,
		  &TheIndex, sizeof(long),
		  smibuf, sizeof(struct SuppMsgInfo));

}



/*
 * AdjRefCount  -  change the reference count for a message;
 *                 delete the message if it reaches zero
 */
void AdjRefCount(long msgnum, int incr)
{

	struct SuppMsgInfo smi;
	long delnum;

	/* This is a *tight* critical section; please keep it that way, as
	 * it may get called while nested in other critical sections.  
	 * Complicating this any further will surely cause deadlock!
	 */
	begin_critical_section(S_SUPPMSGMAIN);
	GetSuppMsgInfo(&smi, msgnum);
	smi.smi_refcount += incr;
	PutSuppMsgInfo(&smi);
	end_critical_section(S_SUPPMSGMAIN);

	lprintf(9, "Ref count for message <%ld> after write is <%d>\n",
		msgnum, smi.smi_refcount);

	/* If the reference count is now zero, delete the message
	 * (and its supplementary record as well).
	 */
	if (smi.smi_refcount == 0) {
		lprintf(9, "Deleting message <%ld>\n", msgnum);
		delnum = msgnum;
		cdb_delete(CDB_MSGMAIN, &delnum, sizeof(long));
		delnum = (0L - msgnum);
		cdb_delete(CDB_MSGMAIN, &delnum, sizeof(long));
	}
}
