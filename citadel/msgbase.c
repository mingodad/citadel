/* $Id$ */
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <syslog.h>
#include <pthread.h>
#include "citadel.h"
#include "server.h"
#include <errno.h>
#include <sys/stat.h>
#include "database.h"
#include "msgbase.h"
#include "support.h"
#include "sysdep_decls.h"
#include "room_ops.h"
#include "user_ops.h"
#include "control.h"
#include "dynloader.h"

#define MSGS_ALL	0
#define MSGS_OLD	1
#define MSGS_NEW	2
#define MSGS_FIRST	3
#define MSGS_LAST	4
#define MSGS_GT		5

extern struct config config;


/*
 * Aliasing for network mail.
 * (Error messages have been commented out, because this is a server.)
 */
int alias(char *name)		/* process alias and routing info for mail */
             {
	FILE *fp;
	int a,b;
	char aaa[300],bbb[300];

	lprintf(9, "alias() called for <%s>\n", name);
	
	fp=fopen("network/mail.aliases","r");
	if (fp==NULL) fp=fopen("/dev/null","r");
	if (fp==NULL) return(M_ERROR);
GNA:	strcpy(aaa,""); strcpy(bbb,"");
	do {
		a=getc(fp);
		if (a==',') a=0;
		if (a>0) {
			b=strlen(aaa);
			aaa[b]=a;
			aaa[b+1]=0;
			}
		} while(a>0);
	do {
		a=getc(fp);
		if (a==10) a=0;
		if (a>0) {
			b=strlen(bbb);
			bbb[b]=a;
			bbb[b+1]=0;
			}
		} while(a>0);
	if (a<0) {
		fclose(fp);
		goto DETYPE;
		}
	if (strcasecmp(name,aaa)) goto GNA;
	fclose(fp);
	strcpy(name,bbb);
	lprintf(7, "Mail is being forwarded to %s\n", name);

DETYPE:	/* determine local or remote type, see citadel.h */
	for (a=0; a<strlen(name); ++a) if (name[a]=='!') return(M_INTERNET);
	for (a=0; a<strlen(name); ++a)
		if (name[a]=='@')
			for (b=a; b<strlen(name); ++b)
				if (name[b]=='.') return(M_INTERNET);
	b=0; for (a=0; a<strlen(name); ++a) if (name[a]=='@') ++b;
	if (b>1) {
		lprintf(7, "Too many @'s in address\n");
		return(M_ERROR);
		}
	if (b==1) {
		for (a=0; a<strlen(name); ++a)
			if (name[a]=='@') strcpy(bbb,&name[a+1]);
		while (bbb[0]==32) strcpy(bbb,&bbb[1]);
		fp = fopen("network/mail.sysinfo","r");
		if (fp==NULL) return(M_ERROR);
GETSN:		do {
			a=getstring(fp,aaa);
			} while ((a>=0)&&(strcasecmp(aaa,bbb)));
		a=getstring(fp,aaa);
		if (!strncmp(aaa,"use ",4)) {
			strcpy(bbb,&aaa[4]);
			fseek(fp,0L,0);
			goto GETSN;
			}
		fclose(fp);
		if (!strncmp(aaa,"uum",3)) {
			strcpy(bbb,name);
			for (a=0; a<strlen(bbb); ++a) {
				if (bbb[a]=='@') bbb[a]=0;
				if (bbb[a]==' ') bbb[a]='_';
				}
			while(bbb[strlen(bbb)-1]=='_') bbb[strlen(bbb)-1]=0;
			sprintf(name,&aaa[4],bbb);
			return(M_INTERNET);
			}
		if (!strncmp(aaa,"bin",3)) {
			strcpy(aaa,name); strcpy(bbb,name);
			while (aaa[strlen(aaa)-1]!='@') aaa[strlen(aaa)-1]=0;
			aaa[strlen(aaa)-1]=0;
			while (aaa[strlen(aaa)-1]==' ') aaa[strlen(aaa)-1]=0;
			while (bbb[0]!='@') strcpy(bbb,&bbb[1]);
			strcpy(bbb,&bbb[1]);
			while (bbb[0]==' ') strcpy(bbb,&bbb[1]);
			sprintf(name,"%s @%s",aaa,bbb);
			return(M_BINARY);
			}
		return(M_ERROR);
		}
	return(M_LOCAL);
	}


void get_mm(void) {
	FILE *fp;

	fp=fopen("citadel.control","r");
	fread((char *)&CitControl,sizeof(struct CitControl),1,fp);
	fclose(fp);
	}

/*
 * cmd_msgs()  -  get list of message #'s in this room
 */
void cmd_msgs(char *cmdbuf)
{
	int a = 0;
	int mode = 0;
	char which[256];
	int cm_howmany = 0;
	long cm_gt = 0L;
	struct visit vbuf;

	extract(which,cmdbuf,0);

	mode = MSGS_ALL;
	strcat(which,"   ");
	if (!strncasecmp(which,"OLD",3))	mode = MSGS_OLD;
	if (!strncasecmp(which,"NEW",3))	mode = MSGS_NEW;
	if (!strncasecmp(which,"FIRST",5))	{
		mode = MSGS_FIRST;
		cm_howmany = extract_int(cmdbuf,1);
		}
	if (!strncasecmp(which,"LAST",4))	{
		mode = MSGS_LAST;
		cm_howmany = extract_int(cmdbuf,1);
		}
	if (!strncasecmp(which,"GT",2))	{
		mode = MSGS_GT;
		cm_gt = extract_long(cmdbuf,1);
		}

	if ((!(CC->logged_in))&&(!(CC->internal_pgm))) {
		cprintf("%d not logged in\n",ERROR+NOT_LOGGED_IN);
		return;
		}
	get_mm();
	get_msglist(&CC->quickroom);
	getuser(&CC->usersupp,CC->curr_user);
	CtdlGetRelationship(&vbuf, &CC->usersupp, &CC->quickroom);

	cprintf("%d Message list...\n",LISTING_FOLLOWS);
	if (CC->num_msgs != 0) {
	   for (a=0; a<(CC->num_msgs); ++a) 
	       if ((MessageFromList(a) >=0)
	       && ( 

(mode==MSGS_ALL)
|| ((mode==MSGS_OLD) && (MessageFromList(a) <= vbuf.v_lastseen))
|| ((mode==MSGS_NEW) && (MessageFromList(a) > vbuf.v_lastseen))
|| ((mode==MSGS_NEW) && (MessageFromList(a) >= vbuf.v_lastseen)
		     && (CC->usersupp.flags & US_LASTOLD))
|| ((mode==MSGS_LAST)&& (a>=(CC->num_msgs-cm_howmany)))
|| ((mode==MSGS_FIRST)&&(a<cm_howmany))
|| ((mode==MSGS_GT) && (MessageFromList(a) > cm_gt))

			)
		) {
			cprintf("%ld\n", MessageFromList(a));
			}
	   }
	cprintf("000\n");
	}



/* 
 * help_subst()  -  support routine for help file viewer
 */
void help_subst(char *strbuf, char *source, char *dest)
{
	char workbuf[256];
	int p;

	while (p=pattern2(strbuf,source), (p>=0)) {
		strcpy(workbuf,&strbuf[p+strlen(source)]);
		strcpy(&strbuf[p],dest);
		strcat(strbuf,workbuf);
		}
	}


void do_help_subst(char *buffer)
{
	char buf2[16];

	help_subst(buffer,"^nodename",config.c_nodename);
	help_subst(buffer,"^humannode",config.c_humannode);
	help_subst(buffer,"^fqdn",config.c_fqdn);
	help_subst(buffer,"^username",CC->usersupp.fullname);
	sprintf(buf2,"%ld",CC->usersupp.usernum);
	help_subst(buffer,"^usernum",buf2);
	help_subst(buffer,"^sysadm",config.c_sysadm);
	help_subst(buffer,"^variantname",CITADEL);
	sprintf(buf2,"%d",config.c_maxsessions);
	help_subst(buffer,"^maxsessions",buf2);
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
	int a,b,c;
	int real = 0;
	int old = 0;
	CIT_UBYTE ch;
	char aaa[140];
	char buffer[256];
	
	strcpy(aaa,""); old=255;
	strcpy(buffer,"");
	c=1; /* c is the current pos */

FMTA:	if (subst) {
		while (ch=*mptr, ((ch!=0) && (strlen(buffer)<126) )) {
			ch=*mptr++;
			buffer[strlen(buffer)+1] = 0;
			buffer[strlen(buffer)] = ch;
			}

		if (buffer[0]=='^') do_help_subst(buffer);

		buffer[strlen(buffer)+1] = 0;
		a=buffer[0];
		strcpy(buffer,&buffer[1]);
		}
	
	else ch=*mptr++;

	old=real;
	real=ch;
	if (ch<=0) goto FMTEND;
	
	if ( ((ch==13)||(ch==10)) && (old!=13) && (old!=10) ) ch=32;
	if ( ((old==13)||(old==10)) && (isspace(real)) ) {
		cprintf("\n");
		c=1;
		}
	if (ch>126) goto FMTA;

	if (ch>32) {
	if ( ((strlen(aaa)+c)>(width-5)) && (strlen(aaa)>(width-5)) )
		{ cprintf("\n%s",aaa); c=strlen(aaa); aaa[0]=0;
		}
	 b=strlen(aaa); aaa[b]=ch; aaa[b+1]=0; }
	if (ch==32) {
		if ((strlen(aaa)+c)>(width-5)) { 
			cprintf("\n");
			c=1;
			}
		cprintf("%s ",aaa); ++c; c=c+strlen(aaa);
		strcpy(aaa,"");
		goto FMTA;
		}
	if ((ch==13)||(ch==10)) {
		cprintf("%s\n",aaa);
		c=1;
		strcpy(aaa,"");
		goto FMTA;
		}
	goto FMTA;

FMTEND:	cprintf("\n");
	}


/*
 * Get a message off disk.  (return value is the message's timestamp)
 * 
 */
time_t output_message(char *msgid, int mode,
			int headers_only, int desired_section) {
	long msg_num;
	int a;
	CIT_UBYTE ch, rch;
	CIT_UBYTE format_type,anon_flag;
	char buf[1024];
	long msg_len;
	int msg_ok = 0;
	char boundary[256];		/* attachment boundary */
	char current_section = 0;	/* section currently being parsed */
	int has_attachments = 0;

	struct cdbdata *dmsgtext;
	char *mptr;

	/* buffers needed for RFC822 translation */
	char suser[256];
	char luser[256];
	char snode[256];
	char lnode[256];
	char mid[256];
	time_t xtime = 0L;
	/* */

	strcpy(boundary, "");
	msg_num = atol(msgid);


	if ((!(CC->logged_in))&&(!(CC->internal_pgm))&&(mode!=MT_DATE)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return(xtime);
		}

	/* We used to need to check in the current room's message list
	 * to determine where the message's disk position.  We no longer need
	 * to do this, but we do it anyway as a security measure, in order to
	 * prevent rogue clients from reading messages not in the current room.
	 */

	msg_ok = 0;
	if (CC->num_msgs > 0) {
		for (a=0; a<CC->num_msgs; ++a) {
			if (MessageFromList(a) == msg_num) {
				msg_ok = 1;
				}
			}
		}

	if (!msg_ok) {
		if (mode != MT_DATE)
			cprintf("%d Message %ld is not in this room.\n",
				ERROR, msg_num);
		return(xtime);
		}
	

	dmsgtext = cdb_fetch(CDB_MSGMAIN, &msg_num, sizeof(long));
	
	if (dmsgtext == NULL) {
		if (mode != MT_DATE)
			cprintf("%d Can't find message %ld\n",
				ERROR+INTERNAL_ERROR);
		return(xtime);
		}

	msg_len = (long) dmsgtext->len;
	mptr = dmsgtext->ptr;

	/* this loop spews out the whole message if we're doing raw format */
	if (mode == MT_RAW) {
		cprintf("%d %ld\n", BINARY_FOLLOWS, msg_len);
		client_write(dmsgtext->ptr, (int) msg_len);
		cdb_free(dmsgtext);
		return(xtime);
		}

	/* Otherwise, we'll start parsing it field by field... */
	ch = *mptr++;
	if (ch != 255) {
		cprintf("%d Illegal message format on disk\n",
			ERROR+INTERNAL_ERROR);
		cdb_free(dmsgtext);
		return(xtime);
		}

	anon_flag = *mptr++;
	format_type = *mptr++;

	/* Are we just looking for the message date? */
	if (mode == MT_DATE) while(ch = *mptr++, (ch!='M' && ch!=0)) {
		buf[0] = 0;
		do {
			buf[strlen(buf)+1] = 0;
			rch = *mptr++;
			buf[strlen(buf)] = rch;
			} while (rch > 0);

		if (ch=='T') {
			xtime = atol(buf);
			cdb_free(dmsgtext);
			return(xtime);
			}
		}


	/* now for the user-mode message reading loops */
	cprintf("%d Message %ld:\n",LISTING_FOLLOWS,msg_num);

	if (mode == MT_CITADEL) cprintf("type=%d\n",format_type);

	if ( (anon_flag == MES_ANON) && (mode == MT_CITADEL) ) {
		cprintf("nhdr=yes\n");
		}

	/* begin header processing loop for Citadel message format */

	if (mode == MT_CITADEL) while(ch = *mptr++, (ch!='M' && ch!=0)) {
		buf[0] = 0;
		do {
			buf[strlen(buf)+1] = 0;
			rch = *mptr++;
			buf[strlen(buf)] = rch;
			} while (rch > 0);

		if (ch=='A') {
			PerformUserHooks(buf, (-1L), EVT_OUTPUTMSG);
			if (anon_flag==MES_ANON) cprintf("from=****");
			else if (anon_flag==MES_AN2) cprintf("from=anonymous");
			else cprintf("from=%s",buf);
			if ((is_room_aide()) && ((anon_flag == MES_ANON)
			   || (anon_flag == MES_AN2)))
				cprintf(" [%s]",buf);
			cprintf("\n");
			}
		else if (ch=='Z') {
			has_attachments = 1;
			sprintf(boundary, "--%s", buf);
			}
		else if (ch=='P') cprintf("path=%s\n",buf);
		else if (ch=='U') cprintf("subj=%s\n",buf);
		else if (ch=='I') cprintf("msgn=%s\n",buf);
		else if (ch=='H') cprintf("hnod=%s\n",buf);
		else if (ch=='O') cprintf("room=%s\n",buf);
		else if (ch=='N') cprintf("node=%s\n",buf);
		else if (ch=='R') cprintf("rcpt=%s\n",buf);
		else if (ch=='T') cprintf("time=%s\n",buf);
		/* else cprintf("fld%c=%s\n",ch,buf); */
		}

	/* begin header processing loop for RFC822 transfer format */

	strcpy(suser, "");
	strcpy(luser, "");
	strcpy(snode, NODENAME);
	strcpy(lnode, HUMANNODE);
	if (mode == MT_RFC822) while(ch = *mptr++, (ch!='M' && ch!=0)) {
		buf[0] = 0;
		do {
			buf[strlen(buf)+1] = 0;
			rch = *mptr++;
			buf[strlen(buf)] = rch;
			} while (rch > 0);

		if (ch=='A') strcpy(luser, buf);
		else if (ch=='P') {
			cprintf("Path: %s\n",buf);
			for (a=0; a<strlen(buf); ++a) {
				if (buf[a] == '!') {
					strcpy(buf,&buf[a+1]);
					a=0;
					}
				}
			strcpy(suser, buf);
			}
		else if (ch=='U') cprintf("Subject: %s\n",buf);
		else if (ch=='I') strcpy(mid, buf);
		else if (ch=='H') strcpy(lnode, buf);
		else if (ch=='O') cprintf("X-Citadel-Room: %s\n",buf);
		else if (ch=='N') strcpy(snode, buf);
		else if (ch=='R') cprintf("To: %s\n",buf);
		else if (ch=='T')  {
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

	if (ch==0) {
		cprintf("text\n*** ?Message truncated\n000\n");
		cdb_free(dmsgtext);
		return(xtime);
		}

	if (headers_only) {
		/* give 'em a length */
		msg_len = 0L;
		while(ch = *mptr++, ch>0) {
			++msg_len;
			}
		cprintf("mlen=%ld\n", msg_len);
		cprintf("000\n");
		cdb_free(dmsgtext);
		return(xtime);
		}

	/* signify start of msg text */
	if (mode == MT_CITADEL)	cprintf("text\n");
	if (mode == MT_RFC822) cprintf("\n");

	/* If the format type on disk is 1 (fixed-format), then we want
	 * everything to be output completely literally ... regardless of
	 * what message transfer format is in use.
	 */
	if (format_type == 1) {
		strcpy(buf, "");
		while(ch = *mptr++, ch>0) {
			if (ch == 13) ch = 10;
			if ( (ch == 10) || (strlen(buf)>250) ) {
				if (has_attachments) if (!strncmp(buf, boundary, strlen(boundary))) {
					++current_section;
					}
				if (current_section == desired_section) {
					if ( (has_attachments == 0) || (strncmp(buf, boundary, strlen(boundary)))) {
						cprintf("%s\n", buf);
						}
					}
				strcpy(buf, "");
				}
			else {
				buf[strlen(buf)+1] = 0;
				buf[strlen(buf)] = ch;
				}
			}
		if (strlen(buf)>0) cprintf("%s\n", buf);
		}
	/* If the message on disk is format 0 (Citadel vari-format), we
	 * output using the formatter at 80 columns.  This is the final output
	 * form if the transfer format is RFC822, but if the transfer format
	 * is Citadel proprietary, it'll still work, because the indentation
	 * for new paragraphs is correct and the client will reformat the
	 * message to the reader's screen width.
	 */
	if (format_type == 0) {
		memfmout(80,mptr,0);
		}


	/* now we're done */
	cprintf("000\n");
	cdb_free(dmsgtext);
	return(xtime);
	}


/*
 * display a message (mode 0 - Citadel proprietary)
 */
void cmd_msg0(char *cmdbuf)
{
	char msgid[256];
	int headers_only = 0;
	int desired_section = 0;

	extract(msgid,cmdbuf,0);
	headers_only = extract_int(cmdbuf, 1);
	desired_section = extract_int(cmdbuf, 2);

	output_message(msgid,MT_CITADEL, headers_only, desired_section);
	return;
	}


/*
 * display a message (mode 2 - RFC822)
 */
void cmd_msg2(char *cmdbuf)
{
	char msgid[256];
	int headers_only = 0;

	extract(msgid,cmdbuf,0);
	headers_only = extract_int(cmdbuf,1);

	output_message(msgid,MT_RFC822,headers_only,0);
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

	extract(msgid,cmdbuf,0);
	headers_only = extract_int(cmdbuf,1);

	output_message(msgid,MT_RAW,headers_only,0);
	}



/*
 * Message base operation to send a message to the master file
 * (returns new message number)
 */
long send_message(char *message_in_memory,	/* pointer to buffer */
		size_t message_length,		/* length of buffer */
		int generate_id) {		/* 1 to generate an I field */

	long newmsgid;

	/* Get a new message number */
	newmsgid = get_new_message_number();

	/* Write our little bundle of joy into the message base */

	begin_critical_section(S_MSGMAIN);
	if ( cdb_store(CDB_MSGMAIN, &newmsgid, sizeof(long),
			message_in_memory, message_length) < 0 ) {
		lprintf(2, "Can't store message\n");
		end_critical_section(S_MSGMAIN);
		return 0L;
		}
	end_critical_section(S_MSGMAIN);

	/* Finally, return the pointers */
	return(newmsgid);
	}



/*
 * this is a simple file copy routine.
 */
void copy_file(char *from, char *to)
{
	FILE *ffp,*tfp;
	int a;

	ffp=fopen(from,"r");
	if (ffp==NULL) return;
	tfp=fopen(to,"w");
	if (tfp==NULL) {
		fclose(ffp);
		return;
		}
	while (a=getc(ffp), a>=0) {
		putc(a,tfp);
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
		char mtsflag,	/* 0 for normal, 1 to force Aide> room */
		int mailtype,	/* local or remote type, see citadel.h */
		int generate_id) /* set to 1 to generate an 'I' field */
{
	char aaa[100];
	char hold_rm[ROOMNAMELEN];
	char actual_rm[ROOMNAMELEN];
	char recipient[256];
	long newmsgid;
	char *message_in_memory;
	struct stat statbuf;
	size_t templen;
	FILE *fp;
	struct usersupp userbuf;
	int a;

	lprintf(9, "save_message(%s,%s,%d,%d,%d)\n",
		mtmp, rec, mtsflag, mailtype, generate_id);

	/* Strip non-printable characters out of the recipient name */
	strcpy(recipient, rec);
	for (a=0; a<strlen(recipient); ++a)
		if (!isprint(recipient[a]))
			strcpy(&recipient[a], &recipient[a+1]);

	/* Measure the message */
	stat(mtmp, &statbuf);
	templen = statbuf.st_size;

	/* Now read it into memory */
	message_in_memory = (char *) malloc(templen);
	if (message_in_memory == NULL) {
		lprintf(2, "Can't allocate memory to save message!\n");
		return;
		}

	fp = fopen(mtmp, "rb");
	fread(message_in_memory, templen, 1, fp);
	fclose(fp);

	newmsgid = send_message(message_in_memory, templen, generate_id);
	free(message_in_memory);
	if (newmsgid <= 0L) return;

	strcpy(actual_rm, CC->quickroom.QRname);
	strcpy(hold_rm, "");

	/* If the user is a twit, move to the twit room for posting... */
	if (TWITDETECT) if (CC->usersupp.axlevel==2) {
		strcpy(hold_rm, actual_rm);
		strcpy(actual_rm, config.c_twitroom);
		}

	/* ...or if this is a private message, go to the target mailbox. */
	if (strlen(recipient) > 0) {
		mailtype = alias(recipient);
		if (mailtype == M_LOCAL) {
			if (getuser(&userbuf, recipient)!=0) {
				mtsflag = 1; /* User not found, goto Aide */
				}
			else {
				strcpy(hold_rm, actual_rm);
				MailboxName(actual_rm, &userbuf, MAILROOM);
				}
			}
		}

	/* ...or if this message is destined for Aide> then go there. */
	if (mtsflag) {
		strcpy(hold_rm, actual_rm);
		strcpy(actual_rm, AIDEROOM);
		}

	/* This call to usergoto() changes rooms if necessary.  It also
	 * causes the latest message list to be read into memory.
	 */
	usergoto(actual_rm, 0);

	/* read in the quickroom record, obtaining a lock... */
	lgetroom(&CC->quickroom, actual_rm);

	/* Fix an obscure bug */
	if (!strcasecmp(CC->quickroom.QRname, AIDEROOM)) {
		CC->quickroom.QRflags = CC->quickroom.QRflags & ~QR_MAILBOX;
		}

	/* Add the message pointer to the room */
	AddMessageToRoom(&CC->quickroom, newmsgid);

	/* update quickroom */
	CC->quickroom.QRhighest = newmsgid;
	lputroom(&CC->quickroom, actual_rm);

	/* Network mail - send a copy to the network program. */
	if ( (strlen(recipient)>0) && (mailtype != M_LOCAL) ) {
		sprintf(aaa,"./network/spoolin/nm.%d",getpid());
		copy_file(mtmp,aaa);
		system("exec nohup ./netproc >/dev/null 2>&1 &");
		}

	/* Bump this user's messages posted counter. */
	lgetuser(&CC->usersupp, CC->curr_user);
	CC->usersupp.posted = CC->usersupp.posted + 1;
	lputuser(&CC->usersupp, CC->curr_user);

	/* If we've posted in a room other than the current room, then we
	 * have to now go back to the current room...
	 */
	if (strlen(hold_rm) > 0) {
		usergoto(hold_rm, 0);
		}
	unlink(mtmp);		/* delete the temporary file */
	}


/*
 * Generate an administrative message and post it in the Aide> room.
 */
void aide_message(char *text)
{
	time_t now;
	FILE *fp;

	time(&now);
	fp=fopen(CC->temp,"wb");
	fprintf(fp,"%c%c%c",255,MES_NORMAL,0);
	fprintf(fp,"Psysop%c",0);
	fprintf(fp,"T%ld%c",now,0);
	fprintf(fp,"ACitadel%c",0);
	fprintf(fp,"OAide%c",0);
	fprintf(fp,"N%s%c",NODENAME,0);
	fprintf(fp,"M%s\n%c",text,0);
	fclose(fp);
	save_message(CC->temp,"",1,M_LOCAL,1);
	syslog(LOG_NOTICE,text);
	}



/*
 * Build a binary message to be saved on disk.
 */
void make_message(
	char *filename,			/* temporary file name */
	struct usersupp *author,	/* author's usersupp structure */
	char *recipient,		/* NULL if it's not mail */
	char *room,			/* room where it's going */
	int type,			/* see MES_ types in header file */
	int net_type,			/* see MES_ types in header file */
	int format_type,		/* local or remote (see citadel.h) */
	char *fake_name,		/* who we're masquerading as */
	char *boundary) {		/* boundary (if exist attachments) */

	FILE *fp;
	int a;
	time_t now;
	char dest_node[32];
	char buf[256];

	/* Don't confuse the poor folks if it's not routed mail. */
	strcpy(dest_node, "");


	/* If net_type is M_BINARY, split out the destination node. */
	if (net_type == M_BINARY) {
		strcpy(dest_node,NODENAME);
		for (a=0; a<strlen(recipient); ++a) {
			if (recipient[a]=='@') {
				recipient[a]=0;
				strcpy(dest_node,&recipient[a+1]);
				}
			}
		}

	/* if net_type is M_INTERNET, set the dest node to 'internet' */
	if (net_type == M_INTERNET) {
		strcpy(dest_node,"internet");
		}

	while (isspace(recipient[strlen(recipient)-1]))
		recipient[strlen(recipient)-1] = 0;

	time(&now);
	fp=fopen(filename,"w");
	putc(255,fp);
	putc(type,fp);	/* Normal or anonymous, see MES_ flags */
	putc(format_type,fp);	/* Formatted or unformatted */
	fprintf(fp,"Pcit%ld%c",author->usernum,0);	/* path */
	fprintf(fp,"T%ld%c",now,0);			/* date/time */
	if (fake_name[0])
	   fprintf(fp,"A%s%c",fake_name,0);
	else
 	   fprintf(fp,"A%s%c",author->fullname,0); 	/* author */

	if (CC->quickroom.QRflags & QR_MAILBOX) {	/* room */
		fprintf(fp,"O%s%c", &CC->quickroom.QRname[11], 0);
		}
	else {
		fprintf(fp,"O%s%c",CC->quickroom.QRname,0);
		}

	fprintf(fp,"N%s%c",NODENAME,0); 		/* nodename */
	fprintf(fp,"H%s%c",HUMANNODE,0); 		/* human nodename */

	if (recipient[0]!=0) fprintf(fp, "R%s%c", recipient, 0);
	if (dest_node[0]!=0) fprintf(fp, "D%s%c", dest_node, 0);
	if (boundary[0]!=0) fprintf(fp, "Z%s%c", boundary, 0);

	putc('M',fp);

	while (client_gets(buf), strcmp(buf,"000"))
	{
	   fprintf(fp,"%s\n",buf);
        }
        syslog(LOG_INFO, "Closing message");
	putc(0,fp);
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
	char newusername[256];		/* <bc> */
	char boundary[256];

	int a,b;
	int e = 0;
	int mtsflag = 0;
	struct usersupp tempUS;
	char buf[256];

	post = extract_int(entargs,0);
	extract(recipient,entargs,1);
	anon_flag = extract_int(entargs,2);
	format_type = extract_int(entargs,3);
	extract(boundary, entargs, 5);

	/* first check to make sure the request is valid. */

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}
	if ((CC->usersupp.axlevel<2)&&((CC->quickroom.QRflags&QR_MAILBOX)==0)) {
		cprintf("%d Need to be validated to enter ",
			ERROR+HIGHER_ACCESS_REQUIRED);
		cprintf("(except in %s> to sysop)\n", MAILROOM);
		return;
		}
	if ((CC->usersupp.axlevel<4)&&(CC->quickroom.QRflags&QR_NETWORK)) {
		cprintf("%d Need net privileges to enter here.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}
	if ((CC->usersupp.axlevel<6)&&(CC->quickroom.QRflags&QR_READONLY)) {
		cprintf("%d Sorry, this is a read-only room.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	mtsflag=0;
	
		
        if (post==2) {			/* <bc> */
           if (CC->usersupp.axlevel<6)
           {
              cprintf("%d You don't have permission to do an aide post.\n",
		ERROR+HIGHER_ACCESS_REQUIRED);
              return;
           }
    	   extract(newusername,entargs,4);
    	   bzero(CC->fake_postname, 32);
           strcpy(CC->fake_postname, newusername);
           cprintf("%d Ok\n",OK);
           return;
        }
        
	CC->cs_flags |= CS_POSTING;
	
	buf[0]=0;
	if (CC->quickroom.QRflags & QR_MAILBOX) {
		if (CC->usersupp.axlevel>=2) {
			strcpy(buf,recipient);
			}
		else strcpy(buf,"sysop");
		e=alias(buf);			/* alias and mail type */
		if ((buf[0]==0) || (e==M_ERROR)) {
			cprintf("%d Unknown address - cannot send message.\n",
				ERROR+NO_SUCH_USER);
			return;
			}
		if ((e!=M_LOCAL)&&(CC->usersupp.axlevel<4)) {
			cprintf("%d Net privileges required for network mail.\n",
				ERROR+HIGHER_ACCESS_REQUIRED);
			return;
			}
		if ((RESTRICT_INTERNET==1)&&(e==M_INTERNET)
		   &&((CC->usersupp.flags&US_INTERNET)==0)
		   &&(!CC->internal_pgm) ) {
			cprintf("%d You don't have access to Internet mail.\n",
				ERROR+HIGHER_ACCESS_REQUIRED);
			return;
			}
		if (!strcasecmp(buf,"sysop")) {
			mtsflag=1;
			goto SKFALL;
			}
		if (e!=M_LOCAL) goto SKFALL;	/* don't search local file  */
		if (!strcasecmp(buf,CC->usersupp.fullname)) {
			cprintf("%d Can't send mail to yourself!\n",
				ERROR+NO_SUCH_USER);
			return;
			}

		/* Check to make sure the user exists; also get the correct
	 	* upper/lower casing of the name. 
	 	*/
		a = getuser(&tempUS,buf);
		if (a != 0) {
			cprintf("%d No such user.\n",ERROR+NO_SUCH_USER);
			return;
			}
		strcpy(buf,tempUS.fullname);
		}
	
SKFALL: b=MES_NORMAL;
	if (CC->quickroom.QRflags&QR_ANONONLY) b=MES_ANON;
	if (CC->quickroom.QRflags&QR_ANONOPT) {
		if (anon_flag==1) b=MES_AN2;
		}
	if ((CC->quickroom.QRflags & QR_MAILBOX) == 0) buf[0]=0;

	/* If we're only checking the validity of the request, return
	 * success without creating the message.
	 */
	if (post==0) {
		cprintf("%d %s\n",OK,buf);
		return;
		}
	
	cprintf("%d send message\n",SEND_LISTING);
	if (CC->fake_postname[0])
  	   make_message(CC->temp,&CC->usersupp,buf,CC->quickroom.QRname,b,e,format_type, CC->fake_postname, boundary);
  	else
  	   if (CC->fake_username[0])
  	      make_message(CC->temp,&CC->usersupp,buf,CC->quickroom.QRname,b,e,format_type, CC->fake_username, boundary);
  	   else
  	      make_message(CC->temp,&CC->usersupp,buf,CC->quickroom.QRname,b,e,format_type, "", boundary);
	save_message(CC->temp,buf,mtsflag,e,1);
        CC->fake_postname[0]='\0';
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

	/* If we're in Mail, check the recipient */
	if (CC->quickroom.QRflags & QR_MAILBOX) {
		extract(recp, entargs, 1);
		e=alias(recp);			/* alias and mail type */
		if ((buf[0]==0) || (e==M_ERROR)) {
			cprintf("%d Unknown address - cannot send message.\n",
				ERROR+NO_SUCH_USER);
			return;
			}
		if (e == M_LOCAL) {
			a = getuser(&tempUS,recp);
			if (a!=0) {
				cprintf("%d No such user.\n", ERROR+NO_SUCH_USER);
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
			CC->temp, strerror(errno) );
		return;
		}

	msglen = extract_long(entargs, 2);
	cprintf("%d %ld\n", SEND_BINARY, msglen);
	while(msglen > 0L) {
		bloklen = ((msglen >= 255L) ? 255 : msglen);
		client_read(buf, (int)bloklen );
		fwrite(buf, (int)bloklen, 1, fp);
		msglen = msglen - bloklen;
		}
	fclose(fp);

	save_message(CC->temp, recp, 0, e, 0);
	}


/*
 * Delete message from current room
 */
void cmd_dele(char *delstr)
{
	long delnum;
	int a,ok;

	getuser(&CC->usersupp,CC->curr_user);
	if ((CC->usersupp.axlevel < 6)
	   && (CC->usersupp.usernum != CC->quickroom.QRroomaide)) {
		cprintf("%d Higher access required.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	delnum = atol(delstr);
	if (CC->quickroom.QRflags & QR_MAILBOX) {
		cprintf("%d Can't delete mail.\n",ERROR);
		return;
		}
	
	/* get room records, obtaining a lock... */
	lgetroom(&CC->quickroom,CC->quickroom.QRname);
	get_msglist(&CC->quickroom);

	ok = 0;
	if (CC->num_msgs > 0) for (a=0; a<(CC->num_msgs); ++a) {
		if (MessageFromList(a) == delnum) {
			SetMessageInList(a, 0L);
			ok = 1;
			}
		}

	CC->num_msgs = sort_msglist(CC->msglist, CC->num_msgs);
	CC->quickroom.QRhighest = MessageFromList(CC->num_msgs - 1);

	put_msglist(&CC->quickroom);
	lputroom(&CC->quickroom,CC->quickroom.QRname);
	if (ok==1) {
		cdb_delete(CDB_MSGMAIN, &delnum, sizeof(long));
		cprintf("%d Message deleted.\n",OK);
		}
	else cprintf("%d No message %ld.\n",ERROR,delnum);
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

	num = extract_long(args,0);
	extract(targ,args,1);
	
	getuser(&CC->usersupp,CC->curr_user);
	if ((CC->usersupp.axlevel < 6)
	   && (CC->usersupp.usernum != CC->quickroom.QRroomaide)) {
		cprintf("%d Higher access required.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	if (getroom(&qtemp, targ) != 0) {
		cprintf("%d '%s' does not exist.\n",ERROR,targ);
		return;
		}

	/* yank the message out of the current room... */
	lgetroom(&CC->quickroom, CC->quickroom.QRname);
	get_msglist(&CC->quickroom);

	foundit = 0;
	for (a=0; a<(CC->num_msgs); ++a) {
		if (MessageFromList(a) == num) {
			foundit = 1;
			SetMessageInList(a, 0L);
			}
		}
	if (foundit) {
		CC->num_msgs = sort_msglist(CC->msglist, CC->num_msgs);
		put_msglist(&CC->quickroom);
		CC->quickroom.QRhighest = MessageFromList((CC->num_msgs)-1);
		}
	lputroom(&CC->quickroom,CC->quickroom.QRname);
	if (!foundit) {
		cprintf("%d msg %ld does not exist.\n",ERROR,num);
		return;
		}

	/* put the message into the target room */
	lgetroom(&qtemp, targ);
	AddMessageToRoom(&qtemp, num);
	lputroom(&qtemp, targ);

	cprintf("%d Message moved.\n", OK);
	}
