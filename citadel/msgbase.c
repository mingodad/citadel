/*
 * $Id$
 *
 * Implements the message store.
 *
 */

#ifdef DLL_EXPORT
#define IN_LIBCIT
#endif

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

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

#include <ctype.h>
#include <string.h>
#include <syslog.h>
#include <limits.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include "citadel.h"
#include "server.h"
#include "dynloader.h"
#include "database.h"
#include "msgbase.h"
#include "support.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "room_ops.h"
#include "user_ops.h"
#include "file_ops.h"
#include "control.h"
#include "tools.h"
#include "mime_parser.h"
#include "html.h"
#include "genstamp.h"
#include "internet_addressing.h"

#define desired_section ((char *)CtdlGetUserData(SYM_DESIRED_SECTION))
#define ma ((struct ma_info *)CtdlGetUserData(SYM_MA_INFO))
#define msg_repl ((struct repl *)CtdlGetUserData(SYM_REPL))

extern struct config config;
long config_msgnum;

char *msgkeys[] = {
	"", "", "", "", "", "", "", "", 
	"", "", "", "", "", "", "", "", 
	"", "", "", "", "", "", "", "", 
	"", "", "", "", "", "", "", "", 
	"", "", "", "", "", "", "", "", 
	"", "", "", "", "", "", "", "", 
	"", "", "", "", "", "", "", "", 
	"", "", "", "", "", "", "", "", 
	"", 
	"from",
	"", "", "",
	"exti",
	"rfca",
	"", 
	"hnod",
	"msgn",
	"", "", "",
	"text",
	"node",
	"room",
	"path",
	"",
	"rcpt",
	"spec",
	"time",
	"subj",
	"",
	"",
	"",
	"",
	""
};

/*
 * This function is self explanatory.
 * (What can I say, I'm in a weird mood today...)
 */
void remove_any_whitespace_to_the_left_or_right_of_at_symbol(char *name)
{
	int i;

	for (i = 0; i < strlen(name); ++i) {
		if (name[i] == '@') {
			while (isspace(name[i - 1]) && i > 0) {
				strcpy(&name[i - 1], &name[i]);
				--i;
			}
			while (isspace(name[i + 1])) {
				strcpy(&name[i + 1], &name[i + 2]);
			}
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

	/* Change "user @ xxx" to "user" if xxx is an alias for this host */
	for (a=0; a<strlen(name); ++a) {
		if (name[a] == '@') {
			if (CtdlHostAlias(&name[a+1]) == hostalias_localhost) {
				name[a] = 0;
				lprintf(7, "Changed to <%s>\n", name);
			}
		}
	}

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
GETSN:		do {
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
			lprintf(9, "returning MES_INTERNET\n");
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
			lprintf(9, "returning MES_BINARY\n");
			return (MES_BINARY);
		}
		return (MES_ERROR);
	}
	lprintf(9, "returning MES_LOCAL\n");
	return (MES_LOCAL);
}


void get_mm(void)
{
	FILE *fp;

	fp = fopen("citadel.control", "r");
	fread((char *) &CitControl, sizeof(struct CitControl), 1, fp);
	fclose(fp);
}



void simple_listing(long msgnum, void *userdata)
{
	cprintf("%ld\n", msgnum);
}



/* Determine if a given message matches the fields in a message template.
 * Return 0 for a successful match.
 */
int CtdlMsgCmp(struct CtdlMessage *msg, struct CtdlMessage *template) {
	int i;

	/* If there aren't any fields in the template, all messages will
	 * match.
	 */
	if (template == NULL) return(0);

	/* Null messages are bogus. */
	if (msg == NULL) return(1);

	for (i='A'; i<='Z'; ++i) {
		if (template->cm_fields[i] != NULL) {
			if (msg->cm_fields[i] == NULL) {
				return 1;
			}
			if (strcasecmp(msg->cm_fields[i],
				template->cm_fields[i])) return 1;
		}
	}

	/* All compares succeeded: we have a match! */
	return 0;
}


/*
 * Manipulate the "seen msgs" string.
 */
void CtdlSetSeen(long target_msgnum, int target_setting) {
	char newseen[SIZ];
	struct cdbdata *cdbfr;
	int i;
	int is_seen = 0;
	int was_seen = 1;
	long lo = (-1L);
	long hi = (-1L);
	struct visit vbuf;
	long *msglist;
	int num_msgs = 0;

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
		return;	/* No messages at all?  No further action. */
	}

	lprintf(9, "before optimize: %s\n", vbuf.v_seen);
	strcpy(newseen, "");

	for (i=0; i<num_msgs; ++i) {
		is_seen = 0;

		if (msglist[i] == target_msgnum) {
			is_seen = target_setting;
		}
		else {
			if (is_msg_in_mset(vbuf.v_seen, msglist[i])) {
				is_seen = 1;
			}
		}

		if (is_seen == 1) {
			if (lo < 0L) lo = msglist[i];
			hi = msglist[i];
		}
		if (  ((is_seen == 0) && (was_seen == 1))
		   || ((is_seen == 1) && (i == num_msgs-1)) ) {
			if ( (strlen(newseen) + 20) > SIZ) {
				strcpy(newseen, &newseen[20]);
				newseen[0] = '*';
			}
			if (strlen(newseen) > 0) strcat(newseen, ",");
			if (lo == hi) {
				sprintf(&newseen[strlen(newseen)], "%ld", lo);
			}
			else {
				sprintf(&newseen[strlen(newseen)], "%ld:%ld",
					lo, hi);
			}
			lo = (-1L);
			hi = (-1L);
		}
		was_seen = is_seen;
	}

	safestrncpy(vbuf.v_seen, newseen, SIZ);
	lprintf(9, " after optimize: %s\n", vbuf.v_seen);
	phree(msglist);
	CtdlSetRelationship(&vbuf, &CC->usersupp, &CC->quickroom);
}


/*
 * API function to perform an operation for each qualifying message in the
 * current room.  (Returns the number of messages processed.)
 */
int CtdlForEachMessage(int mode, long ref,
			int moderation_level,
			char *content_type,
			struct CtdlMessage *compare,
			void (*CallBack) (long, void *),
			void *userdata)
{

	int a;
	struct visit vbuf;
	struct cdbdata *cdbfr;
	long *msglist = NULL;
	int num_msgs = 0;
	int num_processed = 0;
	long thismsg;
	struct MetaData smi;
	struct CtdlMessage *msg;
	int is_seen;
	long lastold = 0L;
	int printed_lastold = 0;

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
		return 0;	/* No messages at all?  No further action. */
	}


	/*
	 * Now begin the traversal.
	 */
	if (num_msgs > 0) for (a = 0; a < num_msgs; ++a) {
		GetMetaData(&smi, msglist[a]);

		/* Filter out messages that are moderated below the level
		 * currently being viewed at.
		 */
		if (smi.smi_mod < moderation_level) {
			msglist[a] = 0L;
		}

		/* If the caller is looking for a specific MIME type, filter
		 * out all messages which are not of the type requested.
	 	 */
		if (content_type != NULL) if (strlen(content_type) > 0) {
			if (strcasecmp(smi.smi_content_type, content_type)) {
				msglist[a] = 0L;
			}
		}
	}

	num_msgs = sort_msglist(msglist, num_msgs);

	/* If a template was supplied, filter out the messages which
	 * don't match.  (This could induce some delays!)
	 */
	if (num_msgs > 0) {
		if (compare != NULL) {
			for (a = 0; a < num_msgs; ++a) {
				msg = CtdlFetchMessage(msglist[a]);
				if (msg != NULL) {
					if (CtdlMsgCmp(msg, compare)) {
						msglist[a] = 0L;
					}
					CtdlFreeMessage(msg);
				}
			}
		}
	}

	
	/*
	 * Now iterate through the message list, according to the
	 * criteria supplied by the caller.
	 */
	if (num_msgs > 0)
		for (a = 0; a < num_msgs; ++a) {
			thismsg = msglist[a];
			is_seen = is_msg_in_mset(vbuf.v_seen, thismsg);
			if (is_seen) lastold = thismsg;
			if ((thismsg > 0L)
			    && (

				       (mode == MSGS_ALL)
				       || ((mode == MSGS_OLD) && (is_seen))
				       || ((mode == MSGS_NEW) && (!is_seen))
				       || ((mode == MSGS_LAST) && (a >= (num_msgs - ref)))
				   || ((mode == MSGS_FIRST) && (a < ref))
				|| ((mode == MSGS_GT) && (thismsg > ref))
				|| ((mode == MSGS_EQ) && (thismsg == ref))
			    )
			    ) {
				if ((mode == MSGS_NEW) && (CC->usersupp.flags & US_LASTOLD) && (lastold > 0L) && (printed_lastold == 0) && (!is_seen)) {
					if (CallBack)
						CallBack(lastold, userdata);
					printed_lastold = 1;
					++num_processed;
				}
				if (CallBack) CallBack(thismsg, userdata);
				++num_processed;
			}
		}
	phree(msglist);		/* Clean up */
	return num_processed;
}



/*
 * cmd_msgs()  -  get list of message #'s in this room
 *                implements the MSGS server command using CtdlForEachMessage()
 */
void cmd_msgs(char *cmdbuf)
{
	int mode = 0;
	char which[SIZ];
	char buf[SIZ];
	char tfield[SIZ];
	char tvalue[SIZ];
	int cm_ref = 0;
	int i;
	int with_template = 0;
	struct CtdlMessage *template = NULL;

	extract(which, cmdbuf, 0);
	cm_ref = extract_int(cmdbuf, 1);
	with_template = extract_int(cmdbuf, 2);

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

	if (with_template) {
		cprintf("%d Send template then receive message list\n",
			START_CHAT_MODE);
		template = (struct CtdlMessage *)
			mallok(sizeof(struct CtdlMessage));
		memset(template, 0, sizeof(struct CtdlMessage));
		while(client_gets(buf), strcmp(buf,"000")) {
			extract(tfield, buf, 0);
			extract(tvalue, buf, 1);
			for (i='A'; i<='Z'; ++i) if (msgkeys[i]!=NULL) {
				if (!strcasecmp(tfield, msgkeys[i])) {
					template->cm_fields[i] =
						strdoop(tvalue);
				}
			}
		}
	}
	else {
		cprintf("%d Message list...\n", LISTING_FOLLOWS);
	}

	CtdlForEachMessage(mode, cm_ref,
		CC->usersupp.moderation_filter,
		NULL, template, simple_listing, NULL);
	if (template != NULL) CtdlFreeMessage(template);
	cprintf("000\n");
}




/* 
 * help_subst()  -  support routine for help file viewer
 */
void help_subst(char *strbuf, char *source, char *dest)
{
	char workbuf[SIZ];
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
void memfmout(
	int width,		/* screen width to use */
	char *mptr,		/* where are we going to get our text from? */
	char subst,		/* nonzero if we should do substitutions */
	char *nl)		/* string to terminate lines with */
{
	int a, b, c;
	int real = 0;
	int old = 0;
	CIT_UBYTE ch;
	char aaa[140];
	char buffer[SIZ];

	strcpy(aaa, "");
	old = 255;
	strcpy(buffer, "");
	c = 1;			/* c is the current pos */

	do {
		if (subst) {
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

		if (((ch == 13) || (ch == 10)) && (old != 13) && (old != 10))
			ch = 32;
		if (((old == 13) || (old == 10)) && (isspace(real))) {
			cprintf("%s", nl);
			c = 1;
		}
		if (ch > 126)
			continue;

		if (ch > 32) {
			if (((strlen(aaa) + c) > (width - 5)) && (strlen(aaa) > (width - 5))) {
				cprintf("%s%s", nl, aaa);
				c = strlen(aaa);
				aaa[0] = 0;
			}
			b = strlen(aaa);
			aaa[b] = ch;
			aaa[b + 1] = 0;
		}
		if (ch == 32) {
			if ((strlen(aaa) + c) > (width - 5)) {
				cprintf("%s", nl);
				c = 1;
			}
			cprintf("%s ", aaa);
			++c;
			c = c + strlen(aaa);
			strcpy(aaa, "");
		}
		if ((ch == 13) || (ch == 10)) {
			cprintf("%s%s", aaa, nl);
			c = 1;
			strcpy(aaa, "");
		}

	} while (ch > 0);

	cprintf("%s%s", aaa, nl);
}



/*
 * Callback function for mime parser that simply lists the part
 */
void list_this_part(char *name, char *filename, char *partnum, char *disp,
		    void *content, char *cbtype, size_t length, char *encoding,
		    void *cbuserdata)
{

	cprintf("part=%s|%s|%s|%s|%s|%ld\n",
		name, filename, partnum, disp, cbtype, (long)length);
}


/*
 * Callback function for mime parser that opens a section for downloading
 */
void mime_download(char *name, char *filename, char *partnum, char *disp,
		   void *content, char *cbtype, size_t length, char *encoding,
		   void *cbuserdata)
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
 * This is used by CtdlOutputMsg() and other fetch functions.
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

	/* Always make sure there's something in the msg text field */
	if (ret->cm_fields['M'] == NULL)
		ret->cm_fields['M'] = strdoop("<no text>\n");

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
		if (msg->cm_fields[i] != NULL) {
			phree(msg->cm_fields[i]);
		}

	msg->cm_magic = 0;	/* just in case */
	phree(msg);
}


/*
 * Pre callback function for multipart/alternative
 *
 * NOTE: this differs from the standard behavior for a reason.  Normally when
 *       displaying multipart/alternative you want to show the _last_ usable
 *       format in the message.  Here we show the _first_ one, because it's
 *       usually text/plain.  Since this set of functions is designed for text
 *       output to non-MIME-aware clients, this is the desired behavior.
 *
 */
void fixed_output_pre(char *name, char *filename, char *partnum, char *disp,
	  	void *content, char *cbtype, size_t length, char *encoding,
		void *cbuserdata)
{
		lprintf(9, "fixed_output_pre() type=<%s>\n", cbtype);	
		if (!strcasecmp(cbtype, "multipart/alternative")) {
			ma->is_ma = 1;
			ma->did_print = 0;
			return;
		}
}

/*
 * Post callback function for multipart/alternative
 */
void fixed_output_post(char *name, char *filename, char *partnum, char *disp,
	  	void *content, char *cbtype, size_t length, char *encoding,
		void *cbuserdata)
{
		lprintf(9, "fixed_output_post() type=<%s>\n", cbtype);	
		if (!strcasecmp(cbtype, "multipart/alternative")) {
			ma->is_ma = 0;
			ma->did_print = 0;
			return;
		}
}

/*
 * Inline callback function for mime parser that wants to display text
 */
void fixed_output(char *name, char *filename, char *partnum, char *disp,
	  	void *content, char *cbtype, size_t length, char *encoding,
		void *cbuserdata)
	{
		char *ptr;
		char *wptr;
		size_t wlen;
		CIT_UBYTE ch = 0;

		lprintf(9, "fixed_output() type=<%s>\n", cbtype);	

		/*
		 * If we're in the middle of a multipart/alternative scope and
		 * we've already printed another section, skip this one.
		 */	
	   	if ( (ma->is_ma == 1) && (ma->did_print == 1) ) {
			lprintf(9, "Skipping part %s (%s)\n", partnum, cbtype);
			return;
		}
		ma->did_print = 1;
	
		if ( (!strcasecmp(cbtype, "text/plain")) 
		   || (strlen(cbtype)==0) ) {
			wlen = length;
			wptr = content;
			while (wlen--) {
				ch = *wptr++;
				/**********
				if (ch==10) cprintf("\r\n");
				else cprintf("%c", ch);
				 **********/
				cprintf("%c", ch);
			}
			if (ch != '\n') cprintf("\n");
		}
		else if (!strcasecmp(cbtype, "text/html")) {
			ptr = html_to_ascii(content, 80, 0);
			wlen = strlen(ptr);
			wptr = ptr;
			while (wlen--) {
				ch = *wptr++;
				if (ch==10) cprintf("\r\n");
				else cprintf("%c", ch);
			}
			phree(ptr);
		}
		else if (strncasecmp(cbtype, "multipart/", 10)) {
			cprintf("Part %s: %s (%s) (%ld bytes)\r\n",
				partnum, filename, cbtype, (long)length);
		}
	}


/*
 * Get a message off disk.  (returns om_* values found in msgbase.h)
 * 
 */
int CtdlOutputMsg(long msg_num,		/* message number (local) to fetch */
		int mode,		/* how would you like that message? */
		int headers_only,	/* eschew the message body? */
		int do_proto,		/* do Citadel protocol responses? */
		int crlf		/* Use CRLF newlines instead of LF? */
) {
	struct CtdlMessage *TheMessage;
	int retcode;

	lprintf(7, "CtdlOutputMsg() msgnum=%ld, mode=%d\n", 
		msg_num, mode);

	TheMessage = NULL;

	if ((!(CC->logged_in)) && (!(CC->internal_pgm))) {
		if (do_proto) cprintf("%d Not logged in.\n",
			ERROR + NOT_LOGGED_IN);
		return(om_not_logged_in);
	}

	/* FIXME ... small security issue
	 * We need to check to make sure the requested message is actually
	 * in the current room, and set msg_ok to 1 only if it is.  This
	 * functionality is currently missing because I'm in a hurry to replace
	 * broken production code with nonbroken pre-beta code.  :(   -- ajc
	 *
	 if (!msg_ok) {
	 if (do_proto) cprintf("%d Message %ld is not in this room.\n",
	 ERROR, msg_num);
	 return(om_no_such_msg);
	 }
	 */

	/*
	 * Fetch the message from disk
	 */
	TheMessage = CtdlFetchMessage(msg_num);
	if (TheMessage == NULL) {
		if (do_proto) cprintf("%d Can't locate msg %ld on disk\n",
			ERROR, msg_num);
		return(om_no_such_msg);
	}
	
	retcode = CtdlOutputPreLoadedMsg(
			TheMessage, msg_num, mode,
			headers_only, do_proto, crlf);

	CtdlFreeMessage(TheMessage);
	return(retcode);
}


/*
 * Get a message off disk.  (returns om_* values found in msgbase.h)
 * 
 */
int CtdlOutputPreLoadedMsg(struct CtdlMessage *TheMessage,
		long msg_num,
		int mode,		/* how would you like that message? */
		int headers_only,	/* eschew the message body? */
		int do_proto,		/* do Citadel protocol responses? */
		int crlf		/* Use CRLF newlines instead of LF? */
) {
	int i, k;
	char buf[1024];
	CIT_UBYTE ch;
	char allkeys[SIZ];
	char display_name[SIZ];
	char *mptr;
	char *nl;	/* newline string */

	/* buffers needed for RFC822 translation */
	char suser[SIZ];
	char luser[SIZ];
	char fuser[SIZ];
	char snode[SIZ];
	char lnode[SIZ];
	char mid[SIZ];
	char datestamp[SIZ];
	/*                                       */

	sprintf(mid, "%ld", msg_num);
	nl = (crlf ? "\r\n" : "\n");

	if (!is_valid_message(TheMessage)) {
		lprintf(1, "ERROR: invalid preloaded message for output\n");
	 	return(om_no_such_msg);
	}

	/* Are we downloading a MIME component? */
	if (mode == MT_DOWNLOAD) {
		if (TheMessage->cm_format_type != FMT_RFC822) {
			if (do_proto)
				cprintf("%d This is not a MIME message.\n",
				ERROR);
		} else if (CC->download_fp != NULL) {
			if (do_proto) cprintf(
				"%d You already have a download open.\n",
				ERROR);
		} else {
			/* Parse the message text component */
			mptr = TheMessage->cm_fields['M'];
			mime_parser(mptr, NULL,
				*mime_download, NULL, NULL,
				NULL, 0);
			/* If there's no file open by this time, the requested
			 * section wasn't found, so print an error
			 */
			if (CC->download_fp == NULL) {
				if (do_proto) cprintf(
					"%d Section %s not found.\n",
					ERROR + FILE_NOT_FOUND,
					desired_section);
			}
		}
		return((CC->download_fp != NULL) ? om_ok : om_mime_error);
	}

	/* now for the user-mode message reading loops */
	if (do_proto) cprintf("%d Message %ld:\n", LISTING_FOLLOWS, msg_num);

	/* Tell the client which format type we're using.  If this is a
	 * MIME message, *lie* about it and tell the user it's fixed-format.
	 */
	if (mode == MT_CITADEL) {
		if (TheMessage->cm_format_type == FMT_RFC822) {
			if (do_proto) cprintf("type=1\n");
		}
		else {
			if (do_proto) cprintf("type=%d\n",
				TheMessage->cm_format_type);
		}
	}

	/* nhdr=yes means that we're only displaying headers, no body */
	if ((TheMessage->cm_anon_type == MES_ANON) && (mode == MT_CITADEL)) {
		if (do_proto) cprintf("nhdr=yes\n");
	}

	/* begin header processing loop for Citadel message format */

	if ((mode == MT_CITADEL) || (mode == MT_MIME)) {

		strcpy(display_name, "<unknown>");
		if (TheMessage->cm_fields['A']) {
			strcpy(buf, TheMessage->cm_fields['A']);
			PerformUserHooks(buf, (-1L), EVT_OUTPUTMSG);
			if (TheMessage->cm_anon_type == MES_ANON)
				strcpy(display_name, "****");
			else if (TheMessage->cm_anon_type == MES_AN2)
				strcpy(display_name, "anonymous");
			else
				strcpy(display_name, buf);
			if ((is_room_aide())
			    && ((TheMessage->cm_anon_type == MES_ANON)
			     || (TheMessage->cm_anon_type == MES_AN2))) {
				sprintf(&display_name[strlen(display_name)],
					" [%s]", buf);
			}
		}

		strcpy(allkeys, FORDER);
		for (i=0; i<strlen(allkeys); ++i) {
			k = (int) allkeys[i];
			if (k != 'M') {
				if (TheMessage->cm_fields[k] != NULL) {
					if (k == 'A') {
						if (do_proto) cprintf("%s=%s\n",
							msgkeys[k],
							display_name);
					}
					else {
						if (do_proto) cprintf("%s=%s\n",
							msgkeys[k],
							TheMessage->cm_fields[k]
					);
					}
				}
			}
		}

	}

	/* begin header processing loop for RFC822 transfer format */

	strcpy(suser, "");
	strcpy(luser, "");
	strcpy(fuser, "");
	strcpy(snode, NODENAME);
	strcpy(lnode, HUMANNODE);
	if (mode == MT_RFC822) {
		cprintf("X-UIDL: %ld%s", msg_num, nl);
		for (i = 0; i < 256; ++i) {
			if (TheMessage->cm_fields[i]) {
				mptr = TheMessage->cm_fields[i];

				if (i == 'A') {
					strcpy(luser, mptr);
					strcpy(suser, mptr);
				}
/****
 "Path:" removed for now because it confuses brain-dead Microsoft shitware
 into thinking that mail messages are newsgroup messages instead.  When we
 add NNTP support back into Citadel we'll have to add code to only output
 this field when appropriate.
				else if (i == 'P') {
					cprintf("Path: %s%s", mptr, nl);
				}
 ****/
				else if (i == 'U')
					cprintf("Subject: %s%s", mptr, nl);
				else if (i == 'I')
					strcpy(mid, mptr);
				else if (i == 'H')
					strcpy(lnode, mptr);
				else if (i == 'O')
					cprintf("X-Citadel-Room: %s%s",
						mptr, nl);
				else if (i == 'N')
					strcpy(snode, mptr);
				else if (i == 'R')
					cprintf("To: %s%s", mptr, nl);
				else if (i == 'T') {
					datestring(datestamp, atol(mptr),
						DATESTRING_RFC822 );
					cprintf("Date: %s%s", datestamp, nl);
				}
			}
		}
	}

	for (i=0; i<strlen(suser); ++i) {
		suser[i] = tolower(suser[i]);
		if (!isalnum(suser[i])) suser[i]='_';
	}

	if (mode == MT_RFC822) {
		if (!strcasecmp(snode, NODENAME)) {
			strcpy(snode, FQDN);
		}

		/* Construct a fun message id */
		cprintf("Message-ID: <%s", mid);
		if (strchr(mid, '@')==NULL) {
			cprintf("@%s", snode);
		}
		cprintf(">%s", nl);

		PerformUserHooks(luser, (-1L), EVT_OUTPUTMSG);

		if (strlen(fuser) > 0) {
			cprintf("From: %s (%s)%s", fuser, luser, nl);
		}
		else {
			cprintf("From: %s@%s (%s)%s", suser, snode, luser, nl);
		}

		cprintf("Organization: %s%s", lnode, nl);
	}

	/* end header processing loop ... at this point, we're in the text */

	mptr = TheMessage->cm_fields['M'];

	/* Tell the client about the MIME parts in this message */
	if (TheMessage->cm_format_type == FMT_RFC822) {
		if (mode == MT_CITADEL) {
			mime_parser(mptr, NULL,
				*list_this_part, NULL, NULL,
				NULL, 0);
		}
		else if (mode == MT_MIME) {	/* list parts only */
			mime_parser(mptr, NULL,
				*list_this_part, NULL, NULL,
				NULL, 0);
			if (do_proto) cprintf("000\n");
			return(om_ok);
		}
		else if (mode == MT_RFC822) {	/* unparsed RFC822 dump */
			/* FIXME ... we have to put some code in here to avoid
			 * printing duplicate header information when both
			 * Citadel and RFC822 headers exist.  Preference should
			 * probably be given to the RFC822 headers.
			 */
			while (ch=*(mptr++), ch!=0) {
				if (ch==13) ;
				else if (ch==10) cprintf("%s", nl);
				else cprintf("%c", ch);
			}
			if (do_proto) cprintf("000\n");
			return(om_ok);
		}
	}

	if (headers_only) {
		if (do_proto) cprintf("000\n");
		return(om_ok);
	}

	/* signify start of msg text */
	if (mode == MT_CITADEL)
		if (do_proto) cprintf("text\n");
	if (mode == MT_RFC822) {
		if (TheMessage->cm_fields['U'] == NULL) {
			cprintf("Subject: (no subject)%s", nl);
		}
		cprintf("%s", nl);
	}

	/* If the format type on disk is 1 (fixed-format), then we want
	 * everything to be output completely literally ... regardless of
	 * what message transfer format is in use.
	 */
	if (TheMessage->cm_format_type == FMT_FIXED) {
		strcpy(buf, "");
		while (ch = *mptr++, ch > 0) {
			if (ch == 13)
				ch = 10;
			if ((ch == 10) || (strlen(buf) > 250)) {
				cprintf("%s%s", buf, nl);
				strcpy(buf, "");
			} else {
				buf[strlen(buf) + 1] = 0;
				buf[strlen(buf)] = ch;
			}
		}
		if (strlen(buf) > 0)
			cprintf("%s%s", buf, nl);
	}

	/* If the message on disk is format 0 (Citadel vari-format), we
	 * output using the formatter at 80 columns.  This is the final output
	 * form if the transfer format is RFC822, but if the transfer format
	 * is Citadel proprietary, it'll still work, because the indentation
	 * for new paragraphs is correct and the client will reformat the
	 * message to the reader's screen width.
	 */
	if (TheMessage->cm_format_type == FMT_CITADEL) {
		memfmout(80, mptr, 0, nl);
	}

	/* If the message on disk is format 4 (MIME), we've gotta hand it
	 * off to the MIME parser.  The client has already been told that
	 * this message is format 1 (fixed format), so the callback function
	 * we use will display those parts as-is.
	 */
	if (TheMessage->cm_format_type == FMT_RFC822) {
		CtdlAllocUserData(SYM_MA_INFO, sizeof(struct ma_info));
		memset(ma, 0, sizeof(struct ma_info));
		mime_parser(mptr, NULL,
			*fixed_output, *fixed_output_pre, *fixed_output_post,
			NULL, 0);
	}

	/* now we're done */
	if (do_proto) cprintf("000\n");
	return(om_ok);
}



/*
 * display a message (mode 0 - Citadel proprietary)
 */
void cmd_msg0(char *cmdbuf)
{
	long msgid;
	int headers_only = 0;

	msgid = extract_long(cmdbuf, 0);
	headers_only = extract_int(cmdbuf, 1);

	CtdlOutputMsg(msgid, MT_CITADEL, headers_only, 1, 0);
	return;
}


/*
 * display a message (mode 2 - RFC822)
 */
void cmd_msg2(char *cmdbuf)
{
	long msgid;
	int headers_only = 0;

	msgid = extract_long(cmdbuf, 0);
	headers_only = extract_int(cmdbuf, 1);

	CtdlOutputMsg(msgid, MT_RFC822, headers_only, 1, 1);
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

	cprintf("%d %ld\n", BINARY_FOLLOWS, (long)smr.len);
	client_write(smr.ser, smr.len);
	phree(smr.ser);
}



/* 
 * display a message (mode 4 - MIME) (FIXME ... still evolving, not complete)
 */
void cmd_msg4(char *cmdbuf)
{
	long msgid;

	msgid = extract_long(cmdbuf, 0);
	CtdlOutputMsg(msgid, MT_MIME, 0, 1, 0);
}

/*
 * Open a component of a MIME message as a download file 
 */
void cmd_opna(char *cmdbuf)
{
	long msgid;

	CtdlAllocUserData(SYM_DESIRED_SECTION, SIZ);

	msgid = extract_long(cmdbuf, 0);
	extract(desired_section, cmdbuf, 1);

	CtdlOutputMsg(msgid, MT_DOWNLOAD, 0, 1, 1);
}			


/*
 * Save a message pointer into a specified room
 * (Returns 0 for success, nonzero for failure)
 * roomname may be NULL to use the current room
 */
int CtdlSaveMsgPointerInRoom(char *roomname, long msgid, int flags) {
	int i;
	char hold_rm[ROOMNAMELEN];
        struct cdbdata *cdbfr;
        int num_msgs;
        long *msglist;
        long highest_msg = 0L;
	struct CtdlMessage *msg = NULL;

	lprintf(9, "CtdlSaveMsgPointerInRoom(%s, %ld, %d)\n",
		roomname, msgid, flags);

	strcpy(hold_rm, CC->quickroom.QRname);

	/* We may need to check to see if this message is real */
	if (  (flags & SM_VERIFY_GOODNESS)
	   || (flags & SM_DO_REPL_CHECK)
	   ) {
		msg = CtdlFetchMessage(msgid);
		if (msg == NULL) return(ERROR + ILLEGAL_VALUE);
	}

	/* Perform replication checks if necessary */
	if ( (flags & SM_DO_REPL_CHECK) && (msg != NULL) ) {

		if (getroom(&CC->quickroom,
		   ((roomname != NULL) ? roomname : CC->quickroom.QRname) )
	   	   != 0) {
			lprintf(9, "No such room <%s>\n", roomname);
			if (msg != NULL) CtdlFreeMessage(msg);
			return(ERROR + ROOM_NOT_FOUND);
		}

		if (ReplicationChecks(msg) != 0) {
			getroom(&CC->quickroom, hold_rm);
			if (msg != NULL) CtdlFreeMessage(msg);
			lprintf(9, "Did replication, and newer exists\n");
			return(0);
		}
	}

	/* Now the regular stuff */
	if (lgetroom(&CC->quickroom,
	   ((roomname != NULL) ? roomname : CC->quickroom.QRname) )
	   != 0) {
		lprintf(9, "No such room <%s>\n", roomname);
		if (msg != NULL) CtdlFreeMessage(msg);
		return(ERROR + ROOM_NOT_FOUND);
	}

        cdbfr = cdb_fetch(CDB_MSGLISTS, &CC->quickroom.QRnumber, sizeof(long));
        if (cdbfr == NULL) {
                msglist = NULL;
                num_msgs = 0;
        } else {
                msglist = mallok(cdbfr->len);
                if (msglist == NULL)
                        lprintf(3, "ERROR malloc msglist!\n");
                num_msgs = cdbfr->len / sizeof(long);
                memcpy(msglist, cdbfr->ptr, cdbfr->len);
                cdb_free(cdbfr);
        }


	/* Make sure the message doesn't already exist in this room.  It
	 * is absolutely taboo to have more than one reference to the same
	 * message in a room.
	 */
        if (num_msgs > 0) for (i=0; i<num_msgs; ++i) {
		if (msglist[i] == msgid) {
			lputroom(&CC->quickroom);	/* unlock the room */
			getroom(&CC->quickroom, hold_rm);
			if (msg != NULL) CtdlFreeMessage(msg);
			return(ERROR + ALREADY_EXISTS);
		}
	}

        /* Now add the new message */
        ++num_msgs;
        msglist = reallok(msglist,
                          (num_msgs * sizeof(long)));

        if (msglist == NULL) {
                lprintf(3, "ERROR: can't realloc message list!\n");
        }
        msglist[num_msgs - 1] = msgid;

        /* Sort the message list, so all the msgid's are in order */
        num_msgs = sort_msglist(msglist, num_msgs);

        /* Determine the highest message number */
        highest_msg = msglist[num_msgs - 1];

        /* Write it back to disk. */
        cdb_store(CDB_MSGLISTS, &CC->quickroom.QRnumber, sizeof(long),
                  msglist, num_msgs * sizeof(long));

        /* Free up the memory we used. */
        phree(msglist);

	/* Update the highest-message pointer and unlock the room. */
	CC->quickroom.QRhighest = highest_msg;
	lputroom(&CC->quickroom);
	getroom(&CC->quickroom, hold_rm);

	/* Bump the reference count for this message. */
	if ((flags & SM_DONT_BUMP_REF)==0) {
		AdjRefCount(msgid, +1);
	}

	/* Return success. */
	if (msg != NULL) CtdlFreeMessage(msg);
        return (0);
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
		FILE *save_a_copy)		/* save a copy to disk? */
{
	long newmsgid;
	long retval;
	char msgidbuf[SIZ];
        struct ser_ret smr;

	/* Get a new message number */
	newmsgid = get_new_message_number();
	sprintf(msgidbuf, "%ld@%s", newmsgid, config.c_fqdn);

	/* Generate an ID if we don't have one already */
	if (msg->cm_fields['I']==NULL) {
		msg->cm_fields['I'] = strdoop(msgidbuf);
	}
	
        serialize_message(&smr, msg);

        if (smr.len == 0) {
                cprintf("%d Unable to serialize message\n",
                        ERROR+INTERNAL_ERROR);
                return (-1L);
        }

	/* Write our little bundle of joy into the message base */
	if (cdb_store(CDB_MSGMAIN, &newmsgid, sizeof(long),
		      smr.ser, smr.len) < 0) {
		lprintf(2, "Can't store message\n");
		retval = 0L;
	} else {
		retval = newmsgid;
	}

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

	if (is_valid_message(msg) == 0) return;		/* self check */

	ret->len = 3;
	for (i=0; i<26; ++i) if (msg->cm_fields[(int)forder[i]] != NULL)
		ret->len = ret->len +
			strlen(msg->cm_fields[(int)forder[i]]) + 2;

	lprintf(9, "serialize_message() calling malloc(%ld)\n", (long)ret->len);
	ret->ser = mallok(ret->len);
	if (ret->ser == NULL) {
		ret->len = 0;
		return;
	}

	ret->ser[0] = 0xFF;
	ret->ser[1] = msg->cm_anon_type;
	ret->ser[2] = msg->cm_format_type;
	wlen = 3;

	for (i=0; i<26; ++i) if (msg->cm_fields[(int)forder[i]] != NULL) {
		ret->ser[wlen++] = (char)forder[i];
		strcpy(&ret->ser[wlen], msg->cm_fields[(int)forder[i]]);
		wlen = wlen + strlen(msg->cm_fields[(int)forder[i]]) + 1;
	}
	if (ret->len != wlen) lprintf(3, "ERROR: len=%ld wlen=%ld\n",
		(long)ret->len, (long)wlen);

	return;
}



/*
 * Back end for the ReplicationChecks() function
 */
void check_repl(long msgnum, void *userdata) {
	struct CtdlMessage *msg;
	time_t timestamp = (-1L);

	lprintf(9, "check_repl() found message %ld\n", msgnum);
	msg = CtdlFetchMessage(msgnum);
	if (msg == NULL) return;
	if (msg->cm_fields['T'] != NULL) {
		timestamp = atol(msg->cm_fields['T']);
	}
	CtdlFreeMessage(msg);

	if (timestamp > msg_repl->highest) {
		msg_repl->highest = timestamp;	/* newer! */
		lprintf(9, "newer!\n");
		return;
	}
	lprintf(9, "older!\n");

	/* Existing isn't newer?  Then delete the old one(s). */
	CtdlDeleteMessages(CC->quickroom.QRname, msgnum, "");
}


/*
 * Check to see if any messages already exist which carry the same Extended ID
 * as this one.  
 *
 * If any are found:
 * -> With older timestamps: delete them and return 0.  Message will be saved.
 * -> With newer timestamps: return 1.  Message save will be aborted.
 */
int ReplicationChecks(struct CtdlMessage *msg) {
	struct CtdlMessage *template;
	int abort_this = 0;

	lprintf(9, "ReplicationChecks() started\n");
	/* No extended id?  Don't do anything. */
	if (msg->cm_fields['E'] == NULL) return 0;
	if (strlen(msg->cm_fields['E']) == 0) return 0;
	lprintf(9, "Extended ID: <%s>\n", msg->cm_fields['E']);

	CtdlAllocUserData(SYM_REPL, sizeof(struct repl));
	strcpy(msg_repl->extended_id, msg->cm_fields['E']);
	msg_repl->highest = atol(msg->cm_fields['T']);

	template = (struct CtdlMessage *) malloc(sizeof(struct CtdlMessage));
	memset(template, 0, sizeof(struct CtdlMessage));
	template->cm_fields['E'] = strdoop(msg->cm_fields['E']);

	CtdlForEachMessage(MSGS_ALL, 0L, (-127), NULL, template,
		check_repl, NULL);

	/* If a newer message exists with the same Extended ID, abort
	 * this save.
	 */
	if (msg_repl->highest > atol(msg->cm_fields['T']) ) {
		abort_this = 1;
		}

	CtdlFreeMessage(template);
	lprintf(9, "ReplicationChecks() returning %d\n", abort_this);
	return(abort_this);
}




/*
 * Save a message to disk
 */
long CtdlSaveMsg(struct CtdlMessage *msg,	/* message to save */
		char *rec,			/* Recipient (mail) */
		char *force,			/* force a particular room? */
		int supplied_mailtype)		/* local or remote type */
{
	char aaa[100];
	char hold_rm[ROOMNAMELEN];
	char actual_rm[ROOMNAMELEN];
	char force_room[ROOMNAMELEN];
	char content_type[SIZ];			/* We have to learn this */
	char recipient[SIZ];
	long newmsgid;
	char *mptr = NULL;
	struct usersupp userbuf;
	int a;
	struct MetaData smi;
	FILE *network_fp = NULL;
	static int seqnum = 1;
	struct CtdlMessage *imsg;
	char *instr;
	int mailtype;

	lprintf(9, "CtdlSaveMsg() called\n");
	if (is_valid_message(msg) == 0) return(-1);	/* self check */
	mailtype = supplied_mailtype;

	/* If this message has no timestamp, we take the liberty of
	 * giving it one, right now.
	 */
	if (msg->cm_fields['T'] == NULL) {
		lprintf(9, "Generating timestamp\n");
		sprintf(aaa, "%ld", (long)time(NULL));
		msg->cm_fields['T'] = strdoop(aaa);
	}

	/* If this message has no path, we generate one.
	 */
	if (msg->cm_fields['P'] == NULL) {
		lprintf(9, "Generating path\n");
		if (msg->cm_fields['A'] != NULL) {
			msg->cm_fields['P'] = strdoop(msg->cm_fields['A']);
			for (a=0; a<strlen(msg->cm_fields['P']); ++a) {
				if (isspace(msg->cm_fields['P'][a])) {
					msg->cm_fields['P'][a] = ' ';
				}
			}
		}
		else {
			msg->cm_fields['P'] = strdoop("unknown");
		}
	}

	strcpy(force_room, force);

	/* Strip non-printable characters out of the recipient name */
	lprintf(9, "Checking recipient (if present)\n");
	strcpy(recipient, rec);
	for (a = 0; a < strlen(recipient); ++a)
		if (!isprint(recipient[a]))
			strcpy(&recipient[a], &recipient[a + 1]);

	/* Change "user @ xxx" to "user" if xxx is an alias for this host */
	for (a=0; a<strlen(recipient); ++a) {
		if (recipient[a] == '@') {
			if (CtdlHostAlias(&recipient[a+1]) 
			   == hostalias_localhost) {
				recipient[a] = 0;
				lprintf(7, "Changed to <%s>\n", recipient);
				mailtype = MES_LOCAL;
			}
		}
	}

	lprintf(9, "Recipient is <%s>\n", recipient);

	/* Learn about what's inside, because it's what's inside that counts */
	lprintf(9, "Learning what's inside\n");
	if (msg->cm_fields['M'] == NULL) {
		lprintf(1, "ERROR: attempt to save message with NULL body\n");
	}

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
		while ((--a) > 0) {
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

	/* Goto the correct room */
	lprintf(9, "Switching rooms\n");
	strcpy(hold_rm, CC->quickroom.QRname);
	strcpy(actual_rm, CC->quickroom.QRname);

	/* If the user is a twit, move to the twit room for posting */
	lprintf(9, "Handling twit stuff\n");
	if (TWITDETECT) {
		if (CC->usersupp.axlevel == 2) {
			strcpy(hold_rm, actual_rm);
			strcpy(actual_rm, config.c_twitroom);
		}
	}

	/* ...or if this message is destined for Aide> then go there. */
	if (strlen(force_room) > 0) {
		strcpy(actual_rm, force_room);
	}

	lprintf(9, "Possibly relocating\n");
	if (strcasecmp(actual_rm, CC->quickroom.QRname)) {
		getroom(&CC->quickroom, actual_rm);
	}

	/*
	 * If this message has no O (room) field, generate one.
	 */
	if (msg->cm_fields['O'] == NULL) {
		msg->cm_fields['O'] = strdoop(CC->quickroom.QRname);
	}

	/* Perform "before save" hooks (aborting if any return nonzero) */
	lprintf(9, "Performing before-save hooks\n");
	if (PerformMessageHooks(msg, EVT_BEFORESAVE) > 0) return(-1);

	/* If this message has an Extended ID, perform replication checks */
	lprintf(9, "Performing replication checks\n");
	if (ReplicationChecks(msg) > 0) return(-1);

	/* Network mail - send a copy to the network program. */
	if ((strlen(recipient) > 0) && (mailtype == MES_BINARY)) {
		lprintf(9, "Sending network spool\n");
		sprintf(aaa, "./network/spoolin/netmail.%04lx.%04x.%04x",
			(long) getpid(), CC->cs_pid, ++seqnum);
		lprintf(9, "Saving a copy to %s\n", aaa);
		network_fp = fopen(aaa, "ab+");
		if (network_fp == NULL)
			lprintf(2, "ERROR: %s\n", strerror(errno));
	}

	/* Save it to disk */
	lprintf(9, "Saving to disk\n");
	newmsgid = send_message(msg, network_fp);
	if (network_fp != NULL) {
		fclose(network_fp);
		/* FIXME start a network run here */
	}

	if (newmsgid <= 0L) return(-1);

	/* Write a supplemental message info record.  This doesn't have to
	 * be a critical section because nobody else knows about this message
	 * yet.
	 */
	lprintf(9, "Creating MetaData record\n");
	memset(&smi, 0, sizeof(struct MetaData));
	smi.smi_msgnum = newmsgid;
	smi.smi_refcount = 0;
	safestrncpy(smi.smi_content_type, content_type, 64);
	PutMetaData(&smi);

	/* Now figure out where to store the pointers */
	lprintf(9, "Storing pointers\n");

	/* If this is being done by the networker delivering a private
	 * message, we want to BYPASS saving the sender's copy (because there
	 * is no local sender; it would otherwise go to the Trashcan).
	 */
	if ((!CC->internal_pgm) || (strlen(recipient) == 0)) {
		if (CtdlSaveMsgPointerInRoom(actual_rm, newmsgid, 0) != 0) {
			lprintf(3, "ERROR saving message pointer!\n");
			CtdlSaveMsgPointerInRoom(AIDEROOM, newmsgid, 0);
		}
	}

	/* For internet mail, drop a copy in the outbound queue room */
	if (mailtype == MES_INTERNET) {
		CtdlSaveMsgPointerInRoom(SMTP_SPOOLOUT_ROOM, newmsgid, 0);
	}

	/* Bump this user's messages posted counter. */
	lprintf(9, "Updating user\n");
	lgetuser(&CC->usersupp, CC->curr_user);
	CC->usersupp.posted = CC->usersupp.posted + 1;
	lputuser(&CC->usersupp);

	/* If this is private, local mail, make a copy in the
	 * recipient's mailbox and bump the reference count.
	 */
	if ((strlen(recipient) > 0) && (mailtype == MES_LOCAL)) {
		if (getuser(&userbuf, recipient) == 0) {
			lprintf(9, "Delivering private mail\n");
			MailboxName(actual_rm, &userbuf, MAILROOM);
			CtdlSaveMsgPointerInRoom(actual_rm, newmsgid, 0);
		}
		else {
			lprintf(9, "No user <%s>, saving in %s> instead\n",
				recipient, AIDEROOM);
			CtdlSaveMsgPointerInRoom(AIDEROOM, newmsgid, 0);
		}
	}

	/* Perform "after save" hooks */
	lprintf(9, "Performing after-save hooks\n");
	PerformMessageHooks(msg, EVT_AFTERSAVE);

	/* */
	lprintf(9, "Returning to original room\n");
	if (strcasecmp(hold_rm, CC->quickroom.QRname))
		getroom(&CC->quickroom, hold_rm);

	/* For internet mail, generate delivery instructions 
	 * (Yes, this is recursive!   Deal with it!)
	 */
	if (mailtype == MES_INTERNET) {
		lprintf(9, "Generating delivery instructions\n");
		instr = mallok(2048);
		sprintf(instr,
			"Content-type: %s\n\nmsgid|%ld\nsubmitted|%ld\n"
			"bounceto|%s@%s\n"
			"remote|%s|0||\n",
			SPOOLMIME, newmsgid, (long)time(NULL),
			msg->cm_fields['A'], msg->cm_fields['N'],
			recipient );

        	imsg = mallok(sizeof(struct CtdlMessage));
		memset(imsg, 0, sizeof(struct CtdlMessage));
		imsg->cm_magic = CTDLMESSAGE_MAGIC;
		imsg->cm_anon_type = MES_NORMAL;
		imsg->cm_format_type = FMT_RFC822;
		imsg->cm_fields['A'] = strdoop("Citadel");
		imsg->cm_fields['M'] = instr;
		CtdlSaveMsg(imsg, "", SMTP_SPOOLOUT_ROOM, MES_LOCAL);
		CtdlFreeMessage(imsg);
	}

	return(newmsgid);
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

	CtdlSaveMsg(msg, "", room, MES_LOCAL);
	CtdlFreeMessage(msg);
	syslog(LOG_NOTICE, text);
}



/*
 * Back end function used by make_message() and similar functions
 */
char *CtdlReadMessageBody(char *terminator,	/* token signalling EOT */
			size_t maxlen,		/* maximum message length */
			char *exist		/* if non-null, append to it;
						   exist is ALWAYS freed  */
			) {
	char buf[SIZ];
	int linelen;
	size_t message_len = 0;
	size_t buffer_len = 0;
	char *ptr;
	char *m;

	if (exist == NULL) {
		m = mallok(4096);
	}
	else {
		m = reallok(exist, strlen(exist) + 4096);
		if (m == NULL) phree(exist);
	}
	if (m == NULL) {
		while ( (client_gets(buf)>0) && strcmp(buf, terminator) ) ;;
		return(NULL);
	} else {
		buffer_len = 4096;
		m[0] = 0;
		message_len = 0;
	}
	/* read in the lines of message text one by one */
	while ( (client_gets(buf)>0) && strcmp(buf, terminator) ) {

		/* strip trailing newline type stuff */
		if (buf[strlen(buf)-1]==10) buf[strlen(buf)-1]=0;
		if (buf[strlen(buf)-1]==13) buf[strlen(buf)-1]=0;

		linelen = strlen(buf);

		/* augment the buffer if we have to */
		if ((message_len + linelen + 2) > buffer_len) {
			lprintf(9, "realloking\n");
			ptr = reallok(m, (buffer_len * 2) );
			if (ptr == NULL) {	/* flush if can't allocate */
				while ( (client_gets(buf)>0) &&
					strcmp(buf, terminator)) ;;
				return(m);
			} else {
				buffer_len = (buffer_len * 2);
				m = ptr;
				lprintf(9, "buffer_len is %ld\n", (long)buffer_len);
			}
		}

		/* Add the new line to the buffer.  We avoid using strcat()
		 * because that would involve traversing the entire message
		 * after each line, and this function needs to run fast.
		 */
		strcpy(&m[message_len], buf);
		m[message_len + linelen] = '\n';
		m[message_len + linelen + 1] = 0;
		message_len = message_len + linelen + 1;

		/* if we've hit the max msg length, flush the rest */
		if (message_len >= maxlen) {
			while ( (client_gets(buf)>0)
				&& strcmp(buf, terminator)) ;;
			return(m);
		}
	}
	return(m);
}




/*
 * Build a binary message to be saved on disk.
 */

static struct CtdlMessage *make_message(
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
	char buf[SIZ];
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

	sprintf(buf, "%ld", (long)time(NULL));			/* timestamp */
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


	msg->cm_fields['M'] = CtdlReadMessageBody("000",
						config.c_maxmsglen, NULL);


	return(msg);
}


/*
 * Check to see whether we have permission to post a message in the current
 * room.  Returns a *CITADEL ERROR CODE* and puts a message in errmsgbuf, or
 * returns 0 on success.
 */
int CtdlDoIHavePermissionToPostInThisRoom(char *errmsgbuf) {

	if (!(CC->logged_in)) {
		sprintf(errmsgbuf, "Not logged in.");
		return (ERROR + NOT_LOGGED_IN);
	}

	if ((CC->usersupp.axlevel < 2)
	    && ((CC->quickroom.QRflags & QR_MAILBOX) == 0)) {
		sprintf(errmsgbuf, "Need to be validated to enter "
				"(except in %s> to sysop)", MAILROOM);
		return (ERROR + HIGHER_ACCESS_REQUIRED);
	}

	if ((CC->usersupp.axlevel < 4)
	   && (CC->quickroom.QRflags & QR_NETWORK)) {
		sprintf(errmsgbuf, "Need net privileges to enter here.");
		return (ERROR + HIGHER_ACCESS_REQUIRED);
	}

	if ((CC->usersupp.axlevel < 6)
	   && (CC->quickroom.QRflags & QR_READONLY)) {
		sprintf(errmsgbuf, "Sorry, this is a read-only room.");
		return (ERROR + HIGHER_ACCESS_REQUIRED);
	}

	strcpy(errmsgbuf, "Ok");
	return(0);
}




/*
 * message entry  -  mode 0 (normal)
 */
void cmd_ent0(char *entargs)
{
	int post = 0;
	char recipient[SIZ];
	int anon_flag = 0;
	int format_type = 0;
	char newusername[SIZ];
	struct CtdlMessage *msg;
	int a, b;
	int e = 0;
	int mtsflag = 0;
	struct usersupp tempUS;
	char buf[SIZ];
	int err = 0;

	post = extract_int(entargs, 0);
	extract(recipient, entargs, 1);
	anon_flag = extract_int(entargs, 2);
	format_type = extract_int(entargs, 3);

	/* first check to make sure the request is valid. */

	err = CtdlDoIHavePermissionToPostInThisRoom(buf);
	if (err) {
		cprintf("%d %s\n", err, buf);
		return;
	}

	/* Check some other permission type things. */

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
		}
		else if (e == MES_LOCAL) {	/* don't search local file */
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
	}

	b = MES_NORMAL;
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
		CtdlSaveMsg(msg, buf, (mtsflag ? AIDEROOM : ""), e);
		CtdlFreeMessage(msg);
	CC->fake_postname[0] = '\0';
	return;
}



/* 
 * message entry - mode 3 (raw)
 */
void cmd_ent3(char *entargs)
{
	char recp[SIZ];
	int a;
	int e = 0;
	int valid_msg = 1;
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

	client_read((char*)&ch, 1);				/* 0xFF magic number */
	msg->cm_magic = CTDLMESSAGE_MAGIC;
	client_read((char*)&ch, 1);				/* anon type */
	msg->cm_anon_type = ch;
	client_read((char*)&ch, 1);				/* format type */
	msg->cm_format_type = ch;
	msglen = msglen - 3;

	while (msglen > 0) {
		client_read((char*)&which_field, 1);
		if (!isalpha(which_field)) valid_msg = 0;
		--msglen;
		tempbuf[0] = 0;
		do {
			client_read((char*)&ch, 1);
			--msglen;
			a = strlen(tempbuf);
			tempbuf[a+1] = 0;
			tempbuf[a] = ch;
		} while ( (ch != 0) && (msglen > 0) );
		if (valid_msg)
			msg->cm_fields[which_field] = strdoop(tempbuf);
	}

	msg->cm_flags = CM_SKIP_HOOKS;
	if (valid_msg) CtdlSaveMsg(msg, recp, "", e);
	CtdlFreeMessage(msg);
	phree(tempbuf);
}


/*
 * API function to delete messages which match a set of criteria
 * (returns the actual number of messages deleted)
 */
int CtdlDeleteMessages(char *room_name,		/* which room */
		       long dmsgnum,		/* or "0" for any */
		       char *content_type	/* or "" for any */
)
{

	struct quickroom qrbuf;
	struct cdbdata *cdbfr;
	long *msglist = NULL;
	int num_msgs = 0;
	int i;
	int num_deleted = 0;
	int delete_this;
	struct MetaData smi;

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
			if (strlen(content_type) == 0) {
				delete_this |= 0x02;
			} else {
				GetMetaData(&smi, msglist[i]);
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
 * Check whether the current user has permission to delete messages from
 * the current room (returns 1 for yes, 0 for no)
 */
int CtdlDoIHavePermissionToDeleteMessagesFromThisRoom(void) {
	getuser(&CC->usersupp, CC->curr_user);
	if ((CC->usersupp.axlevel < 6)
	    && (CC->usersupp.usernum != CC->quickroom.QRroomaide)
	    && ((CC->quickroom.QRflags & QR_MAILBOX) == 0)
	    && (!(CC->internal_pgm))) {
		return(0);
	}
	return(1);
}



/*
 * Delete message from current room
 */
void cmd_dele(char *delstr)
{
	long delnum;
	int num_deleted;

	if (CtdlDoIHavePermissionToDeleteMessagesFromThisRoom() == 0) {
		cprintf("%d Higher access required.\n",
			ERROR + HIGHER_ACCESS_REQUIRED);
		return;
	}
	delnum = extract_long(delstr, 0);

	num_deleted = CtdlDeleteMessages(CC->quickroom.QRname, delnum, "");

	if (num_deleted) {
		cprintf("%d %d message%s deleted.\n", OK,
			num_deleted, ((num_deleted != 1) ? "s" : ""));
	} else {
		cprintf("%d Message %ld not found.\n", ERROR, delnum);
	}
}


/*
 * Back end API function for moves and deletes
 */
int CtdlCopyMsgToRoom(long msgnum, char *dest) {
	int err;

	err = CtdlSaveMsgPointerInRoom(dest, msgnum,
		(SM_VERIFY_GOODNESS | SM_DO_REPL_CHECK) );
	if (err != 0) return(err);

	return(0);
}



/*
 * move or copy a message to another room
 */
void cmd_move(char *args)
{
	long num;
	char targ[SIZ];
	struct quickroom qtemp;
	int err;
	int is_copy = 0;

	num = extract_long(args, 0);
	extract(targ, args, 1);
	targ[ROOMNAMELEN - 1] = 0;
	is_copy = extract_int(args, 2);

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

	err = CtdlCopyMsgToRoom(num, targ);
	if (err != 0) {
		cprintf("%d Cannot store message in %s: error %d\n",
			err, targ, err);
		return;
	}

	/* Now delete the message from the source room,
	 * if this is a 'move' rather than a 'copy' operation.
	 */
	if (is_copy == 0) CtdlDeleteMessages(CC->quickroom.QRname, num, "");

	cprintf("%d Message %s.\n", OK, (is_copy ? "copied" : "moved") );
}



/*
 * GetMetaData()  -  Get the supplementary record for a message
 */
void GetMetaData(struct MetaData *smibuf, long msgnum)
{

	struct cdbdata *cdbsmi;
	long TheIndex;

	memset(smibuf, 0, sizeof(struct MetaData));
	smibuf->smi_msgnum = msgnum;
	smibuf->smi_refcount = 1;	/* Default reference count is 1 */

	/* Use the negative of the message number for its supp record index */
	TheIndex = (0L - msgnum);

	cdbsmi = cdb_fetch(CDB_MSGMAIN, &TheIndex, sizeof(long));
	if (cdbsmi == NULL) {
		return;		/* record not found; go with defaults */
	}
	memcpy(smibuf, cdbsmi->ptr,
	       ((cdbsmi->len > sizeof(struct MetaData)) ?
		sizeof(struct MetaData) : cdbsmi->len));
	cdb_free(cdbsmi);
	return;
}


/*
 * PutMetaData()  -  (re)write supplementary record for a message
 */
void PutMetaData(struct MetaData *smibuf)
{
	long TheIndex;

	/* Use the negative of the message number for its supp record index */
	TheIndex = (0L - smibuf->smi_msgnum);

	lprintf(9, "PuttMetaData(%ld) - ref count is %d\n",
		smibuf->smi_msgnum, smibuf->smi_refcount);

	cdb_store(CDB_MSGMAIN,
		  &TheIndex, sizeof(long),
		  smibuf, sizeof(struct MetaData));

}

/*
 * AdjRefCount  -  change the reference count for a message;
 *                 delete the message if it reaches zero
 */
void AdjRefCount(long msgnum, int incr)
{

	struct MetaData smi;
	long delnum;

	/* This is a *tight* critical section; please keep it that way, as
	 * it may get called while nested in other critical sections.  
	 * Complicating this any further will surely cause deadlock!
	 */
	begin_critical_section(S_SUPPMSGMAIN);
	GetMetaData(&smi, msgnum);
	lprintf(9, "Ref count for message <%ld> before write is <%d>\n",
		msgnum, smi.smi_refcount);
	smi.smi_refcount += incr;
	PutMetaData(&smi);
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
	char cmdbuf[SIZ];
	char ch;
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
		create_room(roomname, 
			( (is_mailbox != NULL) ? 5 : 3 ),
			"", 0, 1);
	}
	/* If the caller specified this object as unique, delete all
	 * other objects of this type that are currently in the room.
	 */
	if (is_unique) {
		lprintf(9, "Deleted %d other msgs of this type\n",
			CtdlDeleteMessages(roomname, 0L, content_type));
	}
	/* Now write the data */
	CtdlSaveMsg(msg, "", roomname, MES_LOCAL);
	CtdlFreeMessage(msg);
}






void CtdlGetSysConfigBackend(long msgnum, void *userdata) {
	config_msgnum = msgnum;
}


char *CtdlGetSysConfig(char *sysconfname) {
	char hold_rm[ROOMNAMELEN];
	long msgnum;
	char *conf;
	struct CtdlMessage *msg;
	char buf[SIZ];
	
	strcpy(hold_rm, CC->quickroom.QRname);
	if (getroom(&CC->quickroom, SYSCONFIGROOM) != 0) {
		getroom(&CC->quickroom, hold_rm);
		return NULL;
	}


	/* We want the last (and probably only) config in this room */
	begin_critical_section(S_CONFIG);
	config_msgnum = (-1L);
	CtdlForEachMessage(MSGS_LAST, 1, (-127), sysconfname, NULL,
		CtdlGetSysConfigBackend, NULL);
	msgnum = config_msgnum;
	end_critical_section(S_CONFIG);

	if (msgnum < 0L) {
		conf = NULL;
	}
	else {
        	msg = CtdlFetchMessage(msgnum);
        	if (msg != NULL) {
                	conf = strdoop(msg->cm_fields['M']);
                	CtdlFreeMessage(msg);
		}
		else {
			conf = NULL;
		}
	}

	getroom(&CC->quickroom, hold_rm);

	if (conf != NULL) do {
		extract_token(buf, conf, 0, '\n');
		strcpy(conf, &conf[strlen(buf)+1]);
	} while ( (strlen(conf)>0) && (strlen(buf)>0) );

	return(conf);
}

void CtdlPutSysConfig(char *sysconfname, char *sysconfdata) {
	char temp[PATH_MAX];
	FILE *fp;

	strcpy(temp, tmpnam(NULL));

	fp = fopen(temp, "w");
	if (fp == NULL) return;
	fprintf(fp, "%s", sysconfdata);
	fclose(fp);

	/* this handy API function does all the work for us */
	CtdlWriteObject(SYSCONFIGROOM, sysconfname, temp, NULL, 0, 1, 0);
	unlink(temp);
}
