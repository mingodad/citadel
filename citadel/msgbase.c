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
#include "html.h"

#define desired_section ((char *)CtdlGetUserData(SYM_DESIRED_SECTION))
#define ma ((struct ma_info *)CtdlGetUserData(SYM_MA_INFO))

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



void simple_listing(long msgnum)
{
	cprintf("%ld\n", msgnum);
}


/*
 * API function to perform an operation for each qualifying message in the
 * current room.
 */
void CtdlForEachMessage(int mode, long ref,
			char *content_type,
			void (*CallBack) (long msgnum))
{

	int a;
	struct visit vbuf;
	struct cdbdata *cdbfr;
	long *msglist = NULL;
	int num_msgs = 0;
	long thismsg;
	struct SuppMsgInfo smi;

	/* Learn about the user and room in question */
	get_mm();
	getuser(&CC->usersupp, CC->curr_user);
	CtdlGetRelationship(&vbuf, &CC->usersupp, &CC->quickroom);

	/* Load the message list */
	cdbfr = cdb_fetch(CDB_MSGLISTS, &CC->quickroom.QRnumber, sizeof(long));
	if (cdbfr != NULL) {
		msglist = mallok(cdbfr->len);
		memcpy(msglist, cdbfr->ptr, cdbfr->len);
		num_msgs = cdbfr->len / sizeof(long);
		cdb_free(cdbfr);
	} else {
		return;		/* No messages at all?  No further action. */
	}


	/* If the caller is looking for a specific MIME type, then filter
	 * out all messages which are not of the type requested.
	 */
	if (num_msgs > 0)
		if (content_type != NULL)
			if (strlen(content_type) > 0)
				for (a = 0; a < num_msgs; ++a) {
					GetSuppMsgInfo(&smi, msglist[a]);
					if (strcasecmp(smi.smi_content_type, content_type)) {
						msglist[a] = 0L;
					}
				}

	num_msgs = sort_msglist(msglist, num_msgs);
	
	/*
	 * Now iterate through the message list, according to the
	 * criteria supplied by the caller.
	 */
	if (num_msgs > 0)
		for (a = 0; a < num_msgs; ++a) {
			thismsg = msglist[a];
			if ((thismsg > 0)
			    && (

				       (mode == MSGS_ALL)
				       || ((mode == MSGS_OLD) && (thismsg <= vbuf.v_lastseen))
				       || ((mode == MSGS_NEW) && (thismsg > vbuf.v_lastseen))
				       || ((mode == MSGS_NEW) && (thismsg >= vbuf.v_lastseen)
				    && (CC->usersupp.flags & US_LASTOLD))
				       || ((mode == MSGS_LAST) && (a >= (num_msgs - ref)))
				   || ((mode == MSGS_FIRST) && (a < ref))
				|| ((mode == MSGS_GT) && (thismsg > ref))
			    )
			    ) {
				CallBack(thismsg);
			}
		}
	phree(msglist);		/* Clean up */
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
	CtdlForEachMessage(mode, cm_ref, NULL, simple_listing);
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
	} else {
		ch = *mptr++;
	}

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
	char *ptr;

	if (!strcasecmp(cbtype, "multipart/alternative")) {
		strcpy(ma->prefix, partnum);
		strcat(ma->prefix, ".");
		ma->is_ma = 1;
		ma->did_print = 0;
		return;
	}

	if ( (!strncasecmp(partnum, ma->prefix, strlen(ma->prefix)))
	   && (ma->is_ma == 1) 
	   && (ma->did_print == 1) ) {
		lprintf(9, "Skipping part %s (%s)\n", partnum, cbtype);
		return;
	}

	ma->did_print = 1;

	if (!strcasecmp(cbtype, "text/plain")) {
		client_write(content, length);
	}
	else if (!strcasecmp(cbtype, "text/html")) {
		ptr = html_to_ascii(content, 80, 0);
		client_write(ptr, strlen(ptr));
		phree(ptr);
	}
	else if (strncasecmp(cbtype, "multipart/", 10)) {
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
 * Load a message from disk into memory.
 * This is used by output_message() and other fetch functions.
 *
 * NOTE: Caller is responsible for freeing the returned CtdlMessage struct
 *       using the CtdlMessageFree() function.
 */
struct CtdlMessage *CtdlFetchMessage(long msgnum)
{
	struct cdbdata *dmsgtext;
	struct CtdlMessage *ret = NULL;
	char *mptr;
	CIT_UBYTE ch;
	CIT_UBYTE field_header;
	size_t field_length;


	dmsgtext = cdb_fetch(CDB_MSGMAIN, &msgnum, sizeof(long));
	if (dmsgtext == NULL) {
		lprintf(9, "CtdlFetchMessage(%ld) failed.\n");
		return NULL;
	}
	mptr = dmsgtext->ptr;

	/* Parse the three bytes that begin EVERY message on disk.
	 * The first is always 0xFF, the on-disk magic number.
	 * The second is the anonymous/public type byte.
	 * The third is the format type byte (vari, fixed, or MIME).
	 */
	ch = *mptr++;
	if (ch != 255) {
		lprintf(5, "Message %ld appears to be corrupted.\n", msgnum);
		cdb_free(dmsgtext);
		return NULL;
	}
	ret = (struct CtdlMessage *) mallok(sizeof(struct CtdlMessage));
	memset(ret, 0, sizeof(struct CtdlMessage));

	ret->cm_magic = CTDLMESSAGE_MAGIC;
	ret->cm_anon_type = *mptr++;	/* Anon type byte */
	ret->cm_format_type = *mptr++;	/* Format type byte */

	/*
	 * The rest is zero or more arbitrary fields.  Load them in.
	 * We're done when we encounter either a zero-length field or
	 * have just processed the 'M' (message text) field.
	 */
	do {
		field_length = strlen(mptr);
		if (field_length == 0)
			break;
		field_header = *mptr++;
		ret->cm_fields[field_header] = mallok(field_length);
		strcpy(ret->cm_fields[field_header], mptr);

		while (*mptr++ != 0);	/* advance to next field */

	} while ((field_length > 0) && (field_header != 'M'));

	cdb_free(dmsgtext);

	/* Perform "before read" hooks (aborting if any return nonzero) */
	if (PerformMessageHooks(ret, EVT_BEFOREREAD) > 0) {
		CtdlFreeMessage(ret);
		return NULL;
	}

	return (ret);
}


/*
 * Returns 1 if the supplied pointer points to a valid Citadel message.
 * If the pointer is NULL or the magic number check fails, returns 0.
 */
int is_valid_message(struct CtdlMessage *msg) {
	if (msg == NULL)
		return 0;
	if ((msg->cm_magic) != CTDLMESSAGE_MAGIC) {
		lprintf(3, "is_valid_message() -- self-check failed\n");
		return 0;
	}
	return 1;
}


/*
 * 'Destructor' for struct CtdlMessage
 */
void CtdlFreeMessage(struct CtdlMessage *msg)
{
	int i;

	if (is_valid_message(msg) == 0) return;

	for (i = 0; i < 256; ++i)
		if (msg->cm_fields[i] != NULL)
			phree(msg->cm_fields[i]);

	msg->cm_magic = 0;	/* just in case */
	phree(msg);
}



/*
 * Get a message off disk.  (return value is the message's timestamp)
 * 
 */
void output_message(char *msgid, int mode, int headers_only)
{
	long msg_num;
	int a, i;
	char buf[1024];
	time_t xtime;
	CIT_UBYTE ch;

	struct CtdlMessage *TheMessage = NULL;

	char *mptr;

	/* buffers needed for RFC822 translation */
	char suser[256];
	char luser[256];
	char snode[256];
	char lnode[256];
	char mid[256];
	/*                                       */

	msg_num = atol(msgid);

	if ((!(CC->logged_in)) && (!(CC->internal_pgm))) {
		cprintf("%d Not logged in.\n", ERROR + NOT_LOGGED_IN);
		return;
	}

	/* FIX ... small security issue
	 * We need to check to make sure the requested message is actually
	 * in the current room, and set msg_ok to 1 only if it is.  This
	 * functionality is currently missing because I'm in a hurry to replace
	 * broken production code with nonbroken pre-beta code.  :(   -- ajc
	 *
	 if (!msg_ok) {
	 cprintf("%d Message %ld is not in this room.\n",
	 ERROR, msg_num);
	 return;
	 }
	 */

	/*
	 * Fetch the message from disk
	 */
	TheMessage = CtdlFetchMessage(msg_num);
	if (TheMessage == NULL) {
		cprintf("%d Can't locate msg %ld on disk\n", ERROR, msg_num);
		return;
	}

	/* Are we downloading a MIME component? */
	if (mode == MT_DOWNLOAD) {
		if (TheMessage->cm_format_type != 4) {
			cprintf("%d This is not a MIME message.\n",
				ERROR);
		} else if (CC->download_fp != NULL) {
			cprintf("%d You already have a download open.\n",
				ERROR);
		} else {
			/* Parse the message text component */
			mptr = TheMessage->cm_fields['M'];
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
		CtdlFreeMessage(TheMessage);
		return;
	}

	/* now for the user-mode message reading loops */
	cprintf("%d Message %ld:\n", LISTING_FOLLOWS, msg_num);

	/* Tell the client which format type we're using.  If this is a
	 * MIME message, *lie* about it and tell the user it's fixed-format.
	 */
	if (mode == MT_CITADEL) {
		if (TheMessage->cm_format_type == 4)
			cprintf("type=1\n");
		else
			cprintf("type=%d\n", TheMessage->cm_format_type);
	}

	/* nhdr=yes means that we're only displaying headers, no body */
	if ((TheMessage->cm_anon_type == MES_ANON) && (mode == MT_CITADEL)) {
		cprintf("nhdr=yes\n");
	}

	/* begin header processing loop for Citadel message format */

	if ((mode == MT_CITADEL) || (mode == MT_MIME)) {

		if (TheMessage->cm_fields['P']) {
			cprintf("path=%s\n", TheMessage->cm_fields['P']);
		}
		if (TheMessage->cm_fields['I']) {
			cprintf("msgn=%s\n", TheMessage->cm_fields['I']);
		}
		if (TheMessage->cm_fields['T']) {
			cprintf("time=%s\n", TheMessage->cm_fields['T']);
		}
		if (TheMessage->cm_fields['A']) {
			strcpy(buf, TheMessage->cm_fields['A']);
			PerformUserHooks(buf, (-1L), EVT_OUTPUTMSG);
			if (TheMessage->cm_anon_type == MES_ANON)
				cprintf("from=****");
			else if (TheMessage->cm_anon_type == MES_AN2)
				cprintf("from=anonymous");
			else
				cprintf("from=%s", buf);
			if ((is_room_aide())
			    && ((TheMessage->cm_anon_type == MES_ANON)
			     || (TheMessage->cm_anon_type == MES_AN2))) {
				cprintf(" [%s]", buf);
			}
			cprintf("\n");
		}
		if (TheMessage->cm_fields['O']) {
			cprintf("room=%s\n", TheMessage->cm_fields['O']);
		}
		if (TheMessage->cm_fields['N']) {
			cprintf("node=%s\n", TheMessage->cm_fields['N']);
		}
		if (TheMessage->cm_fields['H']) {
			cprintf("hnod=%s\n", TheMessage->cm_fields['H']);
		}
		if (TheMessage->cm_fields['R']) {
			cprintf("rcpt=%s\n", TheMessage->cm_fields['R']);
		}
		if (TheMessage->cm_fields['U']) {
			cprintf("subj=%s\n", TheMessage->cm_fields['U']);
		}
		if (TheMessage->cm_fields['Z']) {
			cprintf("zaps=%s\n", TheMessage->cm_fields['Z']);
		}
	}

	/* begin header processing loop for RFC822 transfer format */

	strcpy(suser, "");
	strcpy(luser, "");
	strcpy(snode, NODENAME);
	strcpy(lnode, HUMANNODE);
	if (mode == MT_RFC822) {
		for (i = 0; i < 256; ++i) {
			if (TheMessage->cm_fields[i]) {
				mptr = TheMessage->cm_fields[i];

				if (i == 'A') {
					strcpy(luser, mptr);
				} else if (i == 'P') {
					cprintf("Path: %s\n", mptr);
					for (a = 0; a < strlen(mptr); ++a) {
						if (mptr[a] == '!') {
							strcpy(mptr, &mptr[a + 1]);
							a = 0;
						}
					}
					strcpy(suser, mptr);
				} else if (i == 'U')
					cprintf("Subject: %s\n", mptr);
				else if (i == 'I')
					strcpy(mid, mptr);
				else if (i == 'H')
					strcpy(lnode, mptr);
				else if (i == 'O')
					cprintf("X-Citadel-Room: %s\n", mptr);
				else if (i == 'N')
					strcpy(snode, mptr);
				else if (i == 'R')
					cprintf("To: %s\n", mptr);
				else if (i == 'T') {
					xtime = atol(mptr);
					cprintf("Date: %s", asctime(localtime(&xtime)));
				}
			}
		}
	}

	if (mode == MT_RFC822) {
		if (!strcasecmp(snode, NODENAME)) {
			strcpy(snode, FQDN);
		}
		cprintf("Message-ID: <%s@%s>\n", mid, snode);
		PerformUserHooks(luser, (-1L), EVT_OUTPUTMSG);
		cprintf("From: %s@%s (%s)\n", suser, snode, luser);
		cprintf("Organization: %s\n", lnode);
	}

	/* end header processing loop ... at this point, we're in the text */

	mptr = TheMessage->cm_fields['M'];

	/* Tell the client about the MIME parts in this message */
	if (TheMessage->cm_format_type == 4) {	/* legacy textual dump */
		if (mode == MT_CITADEL) {
			mime_parser(mptr, NULL, *list_this_part);
		}
		else if (mode == MT_MIME) {	/* list parts only */
			mime_parser(mptr, NULL, *list_this_part);
			cprintf("000\n");
			CtdlFreeMessage(TheMessage);
			return;
		}
	}

	if (headers_only) {
		cprintf("000\n");
		CtdlFreeMessage(TheMessage);
		return;
	}

	/* signify start of msg text */
	if (mode == MT_CITADEL)
		cprintf("text\n");
	if ((mode == MT_RFC822) && (TheMessage->cm_format_type != 4))
		cprintf("\n");

	/* If the format type on disk is 1 (fixed-format), then we want
	 * everything to be output completely literally ... regardless of
	 * what message transfer format is in use.
	 */
	if (TheMessage->cm_format_type == 1) {
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
	if (TheMessage->cm_format_type == 0) {
		memfmout(80, mptr, 0);
	}

	/* If the message on disk is format 4 (MIME), we've gotta hand it
	 * off to the MIME parser.  The client has already been told that
	 * this message is format 1 (fixed format), so the callback function
	 * we use will display those parts as-is.
	 */
	if (TheMessage->cm_format_type == 4) {
		CtdlAllocUserData(SYM_MA_INFO, sizeof(struct ma_info));
		memset(ma, 0, sizeof(struct ma_info));
		mime_parser(mptr, NULL, *fixed_output);
	}

	/* now we're done */
	cprintf("000\n");
	CtdlFreeMessage(TheMessage);
	return;
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
	long msgnum;
	struct CtdlMessage *msg;
	struct ser_ret smr;

	if (CC->internal_pgm == 0) {
		cprintf("%d This command is for internal programs only.\n",
			ERROR);
		return;
	}

	msgnum = extract_long(cmdbuf, 0);
	msg = CtdlFetchMessage(msgnum);
	if (msg == NULL) {
		cprintf("%d Message %ld not found.\n", 
			ERROR, msgnum);
		return;
	}

	serialize_message(&smr, msg);
	CtdlFreeMessage(msg);

	if (smr.len == 0) {
		cprintf("%d Unable to serialize message\n",
			ERROR+INTERNAL_ERROR);
		return;
	}

	cprintf("%d %ld\n", BINARY_FOLLOWS, smr.len);
	client_write(smr.ser, smr.len);
	phree(smr.ser);
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
 *
 * This is the back end for CtdlSaveMsg() and should not be directly
 * called by server-side modules.
 *
 */
long send_message(struct CtdlMessage *msg,	/* pointer to buffer */
		int generate_id,		/* generate 'I' field? */
		FILE *save_a_copy)		/* save a copy to disk? */
{
	long newmsgid;
	long retval;
	char msgidbuf[32];
        struct ser_ret smr;

	/* Get a new message number */
	newmsgid = get_new_message_number();
	sprintf(msgidbuf, "%ld", newmsgid);

	if (generate_id) {
		msg->cm_fields['I'] = strdoop(msgidbuf);
	}
	
        serialize_message(&smr, msg);

        if (smr.len == 0) {
                cprintf("%d Unable to serialize message\n",
                        ERROR+INTERNAL_ERROR);
                return (-1L);
        }

	/* Write our little bundle of joy into the message base */
	begin_critical_section(S_MSGMAIN);
	if (cdb_store(CDB_MSGMAIN, &newmsgid, sizeof(long),
		      smr.ser, smr.len) < 0) {
		lprintf(2, "Can't store message\n");
		retval = 0L;
	} else {
		retval = newmsgid;
	}
	end_critical_section(S_MSGMAIN);

	/* If the caller specified that a copy should be saved to a particular
	 * file handle, do that now too.
	 */
	if (save_a_copy != NULL) {
		fwrite(smr.ser, smr.len, 1, save_a_copy);
	}

	/* Free the memory we used for the serialized message */
        phree(smr.ser);

	/* Return the *local* message ID to the caller
	 * (even if we're storing an incoming network message)
	 */
	return(retval);
}



/*
 * Serialize a struct CtdlMessage into the format used on disk and network.
 * 
 * This function loads up a "struct ser_ret" (defined in server.h) which
 * contains the length of the serialized message and a pointer to the
 * serialized message in memory.  THE LATTER MUST BE FREED BY THE CALLER.
 */
void serialize_message(struct ser_ret *ret,		/* return values */
			struct CtdlMessage *msg)	/* unserialized msg */
{
	size_t wlen;
	int i;
	static char *forder = FORDER;

	lprintf(9, "serialize_message() called\n");

	if (is_valid_message(msg) == 0) return;		/* self check */

	lprintf(9, "magic number check OK.\n");

	ret->len = 3;
	for (i=0; i<26; ++i) if (msg->cm_fields[(int)forder[i]] != NULL)
		ret->len = ret->len +
			strlen(msg->cm_fields[(int)forder[i]]) + 2;

	lprintf(9, "message is %d bytes\n", ret->len);

	lprintf(9, "calling malloc\n");
	ret->ser = mallok(ret->len);
	if (ret->ser == NULL) {
		ret->len = 0;
		return;
	}

	ret->ser[0] = 0xFF;
	ret->ser[1] = msg->cm_anon_type;
	ret->ser[2] = msg->cm_format_type;
	wlen = 3;

	lprintf(9, "stuff\n");
	for (i=0; i<26; ++i) if (msg->cm_fields[(int)forder[i]] != NULL) {
		ret->ser[wlen++] = (char)forder[i];
		strcpy(&ret->ser[wlen], msg->cm_fields[(int)forder[i]]);
		wlen = wlen + strlen(msg->cm_fields[(int)forder[i]]) + 1;
	}
	if (ret->len != wlen) lprintf(3, "ERROR: len=%d wlen=%d\n",
		ret->len, wlen);
	lprintf(9, "done serializing\n");

	return;
}



/*
 * Save a message to disk
 */
void CtdlSaveMsg(struct CtdlMessage *msg,	/* message to save */
		char *rec,			/* Recipient (mail) */
		char *force,			/* force a particular room? */
		int mailtype,			/* local or remote type */
		int generate_id)		/* 1 = generate 'I' field */
{
	char aaa[100];
	char hold_rm[ROOMNAMELEN];
	char actual_rm[ROOMNAMELEN];
	char force_room[ROOMNAMELEN];
	char content_type[256];	/* We have to learn this */
	char recipient[256];
	long newmsgid;
	char *mptr;
	struct usersupp userbuf;
	int a;
	int successful_local_recipients = 0;
	struct quickroom qtemp;
	struct SuppMsgInfo smi;
	FILE *network_fp = NULL;
	static int seqnum = 1;

	lprintf(9, "CtdlSaveMsg() called\n");
	if (is_valid_message(msg) == 0) return;		/* self check */

	/* If this message has no timestamp, we take the liberty of
	 * giving it one, right now.
	 */
	if (msg->cm_fields['T'] == NULL) {
		sprintf(aaa, "%ld", time(NULL));
		msg->cm_fields['T'] = strdoop(aaa);
	}

	/* If this message has no path, we generate one.
	 */
	if (msg->cm_fields['P'] == NULL) {
		msg->cm_fields['P'] = strdoop(msg->cm_fields['A']);
		for (a=0; a<strlen(msg->cm_fields['P']); ++a) {
			if (isspace(msg->cm_fields['P'][a])) {
				msg->cm_fields['P'][a] = ' ';
			}
		}
	}

	strcpy(force_room, force);

	/* Strip non-printable characters out of the recipient name */
	strcpy(recipient, rec);
	for (a = 0; a < strlen(recipient); ++a)
		if (!isprint(recipient[a]))
			strcpy(&recipient[a], &recipient[a + 1]);

	/* Learn about what's inside, because it's what's inside that counts */

	switch (msg->cm_format_type) {
	case 0:
		strcpy(content_type, "text/x-citadel-variformat");
		break;
	case 1:
		strcpy(content_type, "text/plain");
		break;
	case 4:
		strcpy(content_type, "text/plain");
		/* advance past header fields */
		mptr = msg->cm_fields['M'];
		a = strlen(mptr);
		while (--a) {
			if (!strncasecmp(mptr, "Content-type: ", 14)) {
				safestrncpy(content_type, mptr,
					    sizeof(content_type));
				strcpy(content_type, &content_type[14]);
				for (a = 0; a < strlen(content_type); ++a)
					if ((content_type[a] == ';')
					    || (content_type[a] == ' ')
					    || (content_type[a] == 13)
					    || (content_type[a] == 10))
						content_type[a] = 0;
				break;
			}
			++mptr;
		}
	}

	/* Perform "before save" hooks (aborting if any return nonzero) */
	if (PerformMessageHooks(msg, EVT_BEFORESAVE) > 0) return;

	/* Network mail - send a copy to the network program. */
	if ((strlen(recipient) > 0) && (mailtype != MES_LOCAL)) {
		sprintf(aaa, "./network/spoolin/netmail.%04lx.%04x.%04x",
			(long) getpid(), CC->cs_pid, ++seqnum);
		lprintf(9, "Saving a copy to %s\n", aaa);
		network_fp = fopen(aaa, "ab+");
		if (network_fp == NULL)
			lprintf(2, "ERROR: %s\n", strerror(errno));
	}

	/* Save it to disk */
	newmsgid = send_message(msg, generate_id, network_fp);
	if (network_fp != NULL) {
		fclose(network_fp);
		system("exec nohup ./netproc -i >/dev/null 2>&1 &");
	}
	if (newmsgid <= 0L)
		return;

	strcpy(actual_rm, CC->quickroom.QRname);
	strcpy(hold_rm, "");

	/* If this is being done by the networker delivering a private
	 * message, we want to BYPASS saving the sender's copy (because there
	 * is no local sender; it would otherwise go to the Trashcan).
	 */
	if ((!CC->internal_pgm) || (strlen(recipient) == 0)) {
		/* If the user is a twit, move to the twit room for posting */
		if (TWITDETECT)
			if (CC->usersupp.axlevel == 2) {
				strcpy(hold_rm, actual_rm);
				strcpy(actual_rm, config.c_twitroom);
			}
		/* ...or if this message is destined for Aide> then go there. */
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

	/* Write a supplemental message info record.  This doesn't have to
	 * be a critical section because nobody else knows about this message
	 * yet.
	 */
	memset(&smi, 0, sizeof(struct SuppMsgInfo));
	smi.smi_msgnum = newmsgid;
	smi.smi_refcount = successful_local_recipients;
	safestrncpy(smi.smi_content_type, content_type, 64);
	PutSuppMsgInfo(&smi);

	/* Perform "after save" hooks */
	PerformMessageHooks(msg, EVT_AFTERSAVE);
}



/*
 * Convenience function for generating small administrative messages.
 */
void quickie_message(char *from, char *to, char *room, char *text)
{
	struct CtdlMessage *msg;

	msg = mallok(sizeof(struct CtdlMessage));
	memset(msg, 0, sizeof(struct CtdlMessage));
	msg->cm_magic = CTDLMESSAGE_MAGIC;
	msg->cm_anon_type = MES_NORMAL;
	msg->cm_format_type = 0;
	msg->cm_fields['A'] = strdoop(from);
	msg->cm_fields['O'] = strdoop(room);
	msg->cm_fields['N'] = strdoop(NODENAME);
	if (to != NULL)
		msg->cm_fields['R'] = strdoop(to);
	msg->cm_fields['M'] = strdoop(text);

	CtdlSaveMsg(msg, "", room, MES_LOCAL, 1);
	CtdlFreeMessage(msg);
	syslog(LOG_NOTICE, text);
}


/*
 * Build a binary message to be saved on disk.
 */

struct CtdlMessage *make_message(
	struct usersupp *author,	/* author's usersupp structure */
	char *recipient,		/* NULL if it's not mail */
	char *room,			/* room where it's going */
	int type,			/* see MES_ types in header file */
	int net_type,			/* see MES_ types in header file */
	int format_type,		/* local or remote (see citadel.h) */
	char *fake_name)		/* who we're masquerading as */
{

	int a;
	char dest_node[32];
	char buf[256];
	size_t message_len = 0;
	size_t buffer_len = 0;
	char *ptr;
	struct CtdlMessage *msg;

	msg = mallok(sizeof(struct CtdlMessage));
	memset(msg, 0, sizeof(struct CtdlMessage));
	msg->cm_magic = CTDLMESSAGE_MAGIC;
	msg->cm_anon_type = type;
	msg->cm_format_type = format_type;

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

	sprintf(buf, "cit%ld", author->usernum);		/* Path */
	msg->cm_fields['P'] = strdoop(buf);

	sprintf(buf, "%ld", time(NULL));			/* timestamp */
	msg->cm_fields['T'] = strdoop(buf);

	if (fake_name[0])					/* author */
		msg->cm_fields['A'] = strdoop(fake_name);
	else
		msg->cm_fields['A'] = strdoop(author->fullname);

	if (CC->quickroom.QRflags & QR_MAILBOX) 		/* room */
		msg->cm_fields['O'] = strdoop(&CC->quickroom.QRname[11]);
	else
		msg->cm_fields['O'] = strdoop(CC->quickroom.QRname);

	msg->cm_fields['N'] = strdoop(NODENAME);		/* nodename */
	msg->cm_fields['H'] = strdoop(HUMANNODE);		/* hnodename */

	if (recipient[0] != 0)
		msg->cm_fields['R'] = strdoop(recipient);
	if (dest_node[0] != 0)
		msg->cm_fields['D'] = strdoop(dest_node);

	msg->cm_fields['M'] = mallok(4096);
	if (msg->cm_fields['M'] == NULL) {
		while (client_gets(buf), strcmp(buf, "000")) ;;	/* flush */
		return(msg);
	} else {
		buffer_len = 4096;
		msg->cm_fields['M'][0] = 0;
		message_len = 0;
	}

	/* read in the lines of message text one by one */
	while (client_gets(buf), strcmp(buf, "000")) {

		/* augment the buffer if we have to */
		if ((message_len + strlen(buf) + 2) > buffer_len) {
			ptr = reallok(msg->cm_fields['M'], (buffer_len * 2) );
			if (ptr == NULL) {	/* flush if can't allocate */
				while (client_gets(buf), strcmp(buf, "000")) ;;
				return(msg);
			} else {
				buffer_len = (buffer_len * 2);
				msg->cm_fields['M'] = ptr;
			}
		}

		strcat(msg->cm_fields['M'], buf);
		strcat(msg->cm_fields['M'], "\n");

		/* if we've hit the max msg length, flush the rest */
		if (message_len >= config.c_maxmsglen) {
			while (client_gets(buf), strcmp(buf, "000")) ;;
			return(msg);
		}
	}

	return(msg);
}





/*
 * message entry  -  mode 0 (normal)
 */
void cmd_ent0(char *entargs)
{
	int post = 0;
	char recipient[256];
	int anon_flag = 0;
	int format_type = 0;
	char newusername[256];
	struct CtdlMessage *msg;
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


	if (post == 2) {
		if (CC->usersupp.axlevel < 6) {
			cprintf("%d You don't have permission to masquerade.\n",
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
		e = alias(buf);	/* alias and mail type */
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

SKFALL:	b = MES_NORMAL;
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

	/* Read in the message from the client. */
	if (CC->fake_postname[0])
		msg = make_message(&CC->usersupp, buf,
			CC->quickroom.QRname, b, e, format_type,
			CC->fake_postname);
	else if (CC->fake_username[0])
		msg = make_message(&CC->usersupp, buf,
			CC->quickroom.QRname, b, e, format_type,
			CC->fake_username);
	else
		msg = make_message(&CC->usersupp, buf,
			CC->quickroom.QRname, b, e, format_type, "");

	if (msg != NULL)
		CtdlSaveMsg(msg, buf, (mtsflag ? AIDEROOM : ""), e, 1);
		CtdlFreeMessage(msg);
	CC->fake_postname[0] = '\0';
	return;
}



/* 
 * message entry - mode 3 (raw)
 */
void cmd_ent3(char *entargs)
{
	char recp[256];
	int a;
	int e = 0;
	unsigned char ch, which_field;
	struct usersupp tempUS;
	long msglen;
	struct CtdlMessage *msg;
	char *tempbuf;

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

	msglen = extract_long(entargs, 2);
	msg = mallok(sizeof(struct CtdlMessage));
	if (msg == NULL) {
		cprintf("%d Out of memory\n", ERROR+INTERNAL_ERROR);
		return;
	}

	memset(msg, 0, sizeof(struct CtdlMessage));
	tempbuf = mallok(msglen);
	if (tempbuf == NULL) {
		cprintf("%d Out of memory\n", ERROR+INTERNAL_ERROR);
		phree(msg);
		return;
	}

	cprintf("%d %ld\n", SEND_BINARY, msglen);

	client_read(&ch, 1);				/* 0xFF magic number */
	msg->cm_magic = CTDLMESSAGE_MAGIC;
	client_read(&ch, 1);				/* anon type */
	msg->cm_anon_type = ch;
	client_read(&ch, 1);				/* format type */
	msg->cm_format_type = ch;
	msglen = msglen - 3;

	while (msglen > 0) {
		client_read(&which_field, 1);
		--msglen;
		tempbuf[0] = 0;
		do {
			client_read(&ch, 1);
			--msglen;
			a = strlen(tempbuf);
			tempbuf[a+1] = 0;
			tempbuf[a] = ch;
		} while ( (ch != 0) && (msglen > 0) );
		msg->cm_fields[which_field] = strdoop(tempbuf);
	}

	CtdlSaveMsg(msg, recp, "", e, 0);
	CtdlFreeMessage(msg);
	phree(tempbuf);
}


/*
 * API function to delete messages which match a set of criteria
 * (returns the actual number of messages deleted)
 */
int CtdlDeleteMessages(char *room_name,		/* which room */
		       long dmsgnum,		/* or "0" for any */
		       char *content_type	/* or NULL for any */
)
{

	struct quickroom qrbuf;
	struct cdbdata *cdbfr;
	long *msglist = NULL;
	int num_msgs = 0;
	int i;
	int num_deleted = 0;
	int delete_this;
	struct SuppMsgInfo smi;

	lprintf(9, "CtdlDeleteMessages(%s, %ld, %s)\n",
		room_name, dmsgnum, content_type);

	/* get room record, obtaining a lock... */
	if (lgetroom(&qrbuf, room_name) != 0) {
		lprintf(7, "CtdlDeleteMessages(): Room <%s> not found\n",
			room_name);
		return (0);	/* room not found */
	}
	cdbfr = cdb_fetch(CDB_MSGLISTS, &qrbuf.QRnumber, sizeof(long));

	if (cdbfr != NULL) {
		msglist = mallok(cdbfr->len);
		memcpy(msglist, cdbfr->ptr, cdbfr->len);
		num_msgs = cdbfr->len / sizeof(long);
		cdb_free(cdbfr);
	}
	if (num_msgs > 0) {
		for (i = 0; i < num_msgs; ++i) {
			delete_this = 0x00;

			/* Set/clear a bit for each criterion */

			if ((dmsgnum == 0L) || (msglist[i] == dmsgnum)) {
				delete_this |= 0x01;
			}
			if (content_type == NULL) {
				delete_this |= 0x02;
			} else {
				GetSuppMsgInfo(&smi, msglist[i]);
				if (!strcasecmp(smi.smi_content_type,
						content_type)) {
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
			  msglist, (num_msgs * sizeof(long)));

		qrbuf.QRhighest = msglist[num_msgs - 1];
		phree(msglist);
	}
	lputroom(&qrbuf);
	lprintf(9, "%d message(s) deleted.\n", num_deleted);
	return (num_deleted);
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
			num_deleted, ((num_deleted != 1) ? "s" : ""));
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
	/* Bump the reference count, otherwise the message will be deleted
	 * from disk when we remove it from the source room.
	 */
	AdjRefCount(num, 1);

	/* yank the message out of the current room... */
	foundit = CtdlDeleteMessages(CC->quickroom.QRname, num, NULL);

	if (foundit) {
		/* put the message into the target room */
		lgetroom(&qtemp, targ);
		qtemp.QRhighest = AddMessageToRoom(&qtemp, num);
		lputroom(&qtemp);
		cprintf("%d Message moved.\n", OK);
	} else {
		AdjRefCount(num, (-1));		/* oops */
		cprintf("%d msg %ld does not exist.\n", ERROR, num);
	}
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

/*
 * Write a generic object to this room
 *
 * Note: this could be much more efficient.  Right now we use two temporary
 * files, and still pull the message into memory as with all others.
 */
void CtdlWriteObject(char *req_room,		/* Room to stuff it in */
			char *content_type,	/* MIME type of this object */
			char *tempfilename,	/* Where to fetch it from */
			struct usersupp *is_mailbox,	/* Mailbox room? */
			int is_binary,		/* Is encoding necessary? */
			int is_unique,		/* Del others of this type? */
			unsigned int flags	/* Internal save flags */
			)
{

	FILE *fp, *tempfp;
	char filename[PATH_MAX];
	char cmdbuf[256];
	int ch;
	struct quickroom qrbuf;
	char roomname[ROOMNAMELEN];
	struct CtdlMessage *msg;
	size_t len;

	if (is_mailbox != NULL)
		MailboxName(roomname, is_mailbox, req_room);
	else
		safestrncpy(roomname, req_room, sizeof(roomname));
	lprintf(9, "CtdlWriteObject() to <%s> (flags=%d)\n", roomname, flags);

	strcpy(filename, tmpnam(NULL));
	fp = fopen(filename, "w");
	if (fp == NULL)
		return;

	tempfp = fopen(tempfilename, "r");
	if (tempfp == NULL) {
		fclose(fp);
		unlink(filename);
		return;
	}

	fprintf(fp, "Content-type: %s\n", content_type);
	lprintf(9, "Content-type: %s\n", content_type);

	if (is_binary == 0) {
		fprintf(fp, "Content-transfer-encoding: 7bit\n\n");
		while (ch = getc(tempfp), ch > 0)
			putc(ch, fp);
		fclose(tempfp);
		putc(0, fp);
		fclose(fp);
	} else {
		fprintf(fp, "Content-transfer-encoding: base64\n\n");
		fclose(tempfp);
		fclose(fp);
		sprintf(cmdbuf, "./base64 -e <%s >>%s",
			tempfilename, filename);
		system(cmdbuf);
	}

	lprintf(9, "Allocating\n");
	msg = mallok(sizeof(struct CtdlMessage));
	memset(msg, 0, sizeof(struct CtdlMessage));
	msg->cm_magic = CTDLMESSAGE_MAGIC;
	msg->cm_anon_type = MES_NORMAL;
	msg->cm_format_type = 4;
	msg->cm_fields['A'] = strdoop(CC->usersupp.fullname);
	msg->cm_fields['O'] = strdoop(req_room);
	msg->cm_fields['N'] = strdoop(config.c_nodename);
	msg->cm_fields['H'] = strdoop(config.c_humannode);
	msg->cm_flags = flags;
	
	lprintf(9, "Loading\n");
	fp = fopen(filename, "rb");
	fseek(fp, 0L, SEEK_END);
	len = ftell(fp);
	rewind(fp);
	msg->cm_fields['M'] = mallok(len);
	fread(msg->cm_fields['M'], len, 1, fp);
	fclose(fp);
	unlink(filename);

	/* Create the requested room if we have to. */
	if (getroom(&qrbuf, roomname) != 0) {
		create_room(roomname, 4, "", 0);
	}
	/* If the caller specified this object as unique, delete all
	 * other objects of this type that are currently in the room.
	 */
	if (is_unique) {
		lprintf(9, "Deleted %d other msgs of this type\n",
			CtdlDeleteMessages(roomname, 0L, content_type));
	}
	/* Now write the data */
	CtdlSaveMsg(msg, "", roomname, MES_LOCAL, 1);
	CtdlFreeMessage(msg);
}
