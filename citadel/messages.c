/*
 * Citadel/UX message support routines
 * see copyright.txt for copyright information
 * $Id$
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdarg.h>
#include "citadel.h"
#include "messages.h"
#include "commands.h"
#include "rooms.h"
#include "tools.h"
#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

#define MAXWORDBUF 256
#define MAXMSGS 512

struct cittext {
	struct cittext *next;
	char text[MAXWORDBUF];
	};

void sttybbs(int cmd);
int struncmp(char *lstr, char *rstr, int len);
int fmout(int width, FILE *fp, char pagin, int height, int starting_lp, char subst);
int haschar(char *st, int ch);
int checkpagin(int lp, int pagin, int height);
void getline(char *string, int lim);
void formout(char *name);
int yesno(void);
void newprompt(char *prompt, char *str, int len);
int file_checksum(char *filename);
void do_edit(char *desc, char *read_cmd, char *check_cmd, char *write_cmd);

char reply_to[512];
long msg_arr[MAXMSGS];
int num_msgs;
extern char room_name[];
extern unsigned room_flags;
extern long highest_msg_read;
extern struct CtdlServInfo serv_info;
extern char temp[];
extern char temp2[];
extern int screenwidth;
extern int screenheight;
extern long maxmsgnum;
extern char is_mail;
extern char is_aide;
extern char is_room_aide;
extern char fullname[];
extern char axlevel;
extern unsigned userflags;
extern char sigcaught;
extern char editor_path[];
extern char printcmd[];
extern int rc_allow_attachments;
extern int rc_display_message_numbers;
extern int rc_force_mail_prompts;

extern int editor_pid;

int lines_printed;

void ka_sigcatch(int signum) {
	char buf[256];
	alarm(S_KEEPALIVE);
	signal(SIGALRM, ka_sigcatch);
	serv_puts("NOOP");
	serv_gets(buf);
	}


/*
 * server keep-alive version of wait() (needed for external editor)
 */
pid_t ka_wait(int *kstatus)
{
	pid_t p;

	alarm(S_KEEPALIVE);
	signal(SIGALRM, ka_sigcatch);
	do {
		errno = 0;
		p = wait(kstatus);
		} while (errno==EINTR);
	signal(SIGALRM,SIG_IGN);
	alarm(0);
	return(p);
	}


/*
 * version of system() that uses ka_wait()
 */
int ka_system(char *shc)
{
	pid_t childpid;
	pid_t waitpid;
	int retcode;

	childpid = fork();
	if (childpid < 0) {
		color(BRIGHT_RED);
		perror("Cannot fork");
		color(DIM_WHITE);
		return((pid_t)childpid);
		}

	if (childpid == 0) {
		execlp("/bin/sh","sh","-c",shc,NULL);
		exit(127);
		}

	if (childpid > 0) {
		do {
			waitpid = ka_wait(&retcode);
			} while (waitpid != childpid);
		return(retcode);
		}

	return(-1);
	}



/*
 * add a newline to the buffer...
 */
void add_newline(struct cittext *textlist)
{
	struct cittext *ptr;

	ptr=textlist;
	while (ptr->next != NULL) ptr = ptr->next;

	while (ptr->text[strlen(ptr->text)-1]==32)
		ptr->text[strlen(ptr->text)-1] = 0;
	/* strcat(ptr->text,"\n"); */

	ptr->next = (struct cittext *)
		malloc(sizeof(struct cittext));
	ptr = ptr->next;
	ptr->next = NULL;
	strcpy(ptr->text,"");
	}


/*
 * add a word to the buffer...
 */
void add_word(struct cittext *textlist, char *wordbuf)
{
	struct cittext *ptr;


	ptr=textlist;
	while (ptr->next != NULL) ptr = ptr->next;
	
	if (3+strlen(ptr->text)+strlen(wordbuf) > screenwidth) {
		ptr->next = (struct cittext *)
			malloc(sizeof(struct cittext));
		ptr = ptr->next;
		ptr->next = NULL;
		strcpy(ptr->text,"");
		}
	
	strcat(ptr->text,wordbuf);
	strcat(ptr->text," ");
	}


/*
 * begin editing of an opened file pointed to by fp, beginning at position pos.
 */
void citedit(FILE *fp, long int base_pos)
{
	int a,prev,finished,b,last_space;
	int appending = 0;
	struct cittext *textlist = NULL;
	struct cittext *ptr;
	char wordbuf[MAXWORDBUF];
	char buf[256];
	time_t last_server_msg,now;

	/*
	 * we're going to keep track of the last time we talked to
	 * the server, so we can periodically send keep-alive messages
	 * out so it doesn't time out.
	 */
	time(&last_server_msg);

	/* first, load the text into the buffer */
	fseek(fp,base_pos,0);
	textlist = (struct cittext *)malloc(sizeof(struct cittext));
	textlist->next = NULL;
	strcpy(textlist->text,"");

	strcpy(wordbuf,"");
	prev = (-1);
	while (a=getc(fp), a>=0) {
		appending = 1;
		if ((a==32)||(a==9)||(a==13)||(a==10)) {
			add_word(textlist,wordbuf);
			strcpy(wordbuf,"");
			if ((prev==13)||(prev==10)) {
				add_word(textlist,"\n");
				add_newline(textlist);
				add_word(textlist,"");
				}
			}
		else {
			wordbuf[strlen(wordbuf)+1] = 0;
			wordbuf[strlen(wordbuf)] = a;
			}
		if (strlen(wordbuf)+3 > screenwidth) {
			add_word(textlist,wordbuf);
			strcpy(wordbuf,"");
			}
		prev = a;
		}

	/* get text */
	finished = 0;
	prev = (appending ? 13 : (-1));
	strcpy(wordbuf,"");
	do {
		a=inkey();
		if (a==10) a=13;
		if (a==9) a=32;
		if (a==127) a=8;
		if ((a==32)&&(prev==13)) {
			add_word(textlist,"\n");
			add_newline(textlist);
			}
		if (a==8) {
			if (strlen(wordbuf)>0) {
				wordbuf[strlen(wordbuf)-1] = 0;
				putc(8,stdout);
				putc(32,stdout);
				putc(8,stdout);
				}
			}
		else if (a==13) {
			printf("\n");
			if (strlen(wordbuf)==0) finished = 1;
			else {
				for (b=0; b<strlen(wordbuf); ++b)
				   if (wordbuf[b]==32) {
					wordbuf[b]=0;
					add_word(textlist,wordbuf);
					strcpy(wordbuf,&wordbuf[b+1]);
					b=0;
					}
				add_word(textlist,wordbuf);
				strcpy(wordbuf,"");
				}
			}
		else {
			putc(a,stdout);
			wordbuf[strlen(wordbuf)+1] = 0;
			wordbuf[strlen(wordbuf)] = a;
			}
		if ((strlen(wordbuf)+3) > screenwidth) {
			last_space = (-1);
			for (b=0; b<strlen(wordbuf); ++b)
				if (wordbuf[b]==32) last_space = b;
			if (last_space>=0) {
				for (b=0; b<strlen(wordbuf); ++b)
				   if (wordbuf[b]==32) {
					wordbuf[b]=0;
					add_word(textlist,wordbuf);
					strcpy(wordbuf,&wordbuf[b+1]);
					b=0;
					}
				for (b=0; b<strlen(wordbuf); ++b) {
					putc(8,stdout);
					putc(32,stdout);
					putc(8,stdout);
					}
				printf("\n%s",wordbuf);
				}
			else {
				add_word(textlist,wordbuf);
				strcpy(wordbuf,"");
				printf("\n");
				}
			}
		prev = a;

		/* this check implements the server keep-alive */
		time(&now);
		if ( (now - last_server_msg) > S_KEEPALIVE ) {
			serv_puts("NOOP");
			serv_gets(buf);
			last_server_msg = now;
			}

		} while (finished==0);

	/* write the buffer back to disk */
	fseek(fp,base_pos,0);
	for (ptr=textlist; ptr!=NULL; ptr=ptr->next) {
		fprintf(fp,"%s",ptr->text);
		}
	putc(10,fp);
	putc(0,fp);

	/* and deallocate the memory we used */
	while (textlist!=NULL) {
		ptr=textlist->next;
		free(textlist);
		textlist=ptr;
		}
	}


int read_message(long int num, char pagin) /* Read a message from the server */
         				   /* message number */
           	/* 0 = normal read, 1 = read with pagination, 2 = header */
{
	char buf[256];
	char m_subject[256];
	char from[256];
	time_t now;
	struct tm *tm;
	int format_type = 0;
	int fr = 0;
	int nhdr = 0;

	sigcaught = 0;
	sttybbs(1);

	sprintf(buf,"MSG0 %ld|%d",num,(pagin==READ_HEADER ? 1 : 0));
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0]!='1') {
		printf("*** msg #%ld: %s\n",num,buf);
		++lines_printed;
		lines_printed = checkpagin(lines_printed,pagin,screenheight);
		sttybbs(0);
		return(0);
		}

	strcpy(m_subject,"");
	printf("\n");
	++lines_printed;
	lines_printed = checkpagin(lines_printed,pagin,screenheight);
	printf(" ");
	if (pagin == 1) color(BRIGHT_CYAN);

	if (pagin==2) {
		while(serv_gets(buf), strcmp(buf,"000")) {
			if (buf[4]=='=') {
				printf("%s\n",buf);
				++lines_printed;
				lines_printed = 
					checkpagin(lines_printed,
						pagin,screenheight);
				}
			}
		sttybbs(0);
		return(0);
		}

	strcpy(reply_to,"nobody...xxxxx");
	while(serv_gets(buf), struncmp(buf,"text",4)) {
		if (!struncmp(buf,"nhdr=yes",8)) nhdr=1;
		if (!struncmp(buf,"from=",5)) {
			strcpy(from,&buf[5]);
			}
		if (nhdr==1) buf[0]='_';
		if (!struncmp(buf,"type=",5))
			format_type=atoi(&buf[5]);
		if ((!struncmp(buf,"msgn=",5))&&(rc_display_message_numbers)) {
			color(DIM_WHITE);
			printf("[");
			color(BRIGHT_WHITE);
			printf("#%s",&buf[5]);
			color(DIM_WHITE);
			printf("] ");
			}
		if (!struncmp(buf,"from=",5)) {
			color(DIM_WHITE);
			printf("from ");
			color(BRIGHT_CYAN);
			printf("%s ",&buf[5]);
			}
		if (!struncmp(buf,"subj=",5))
			strcpy(m_subject,&buf[5]);
		if ((!struncmp(buf,"hnod=",5)) 
		   && (strucmp(&buf[5],serv_info.serv_humannode))) {
			color(DIM_WHITE);
			printf("(");
			color(BRIGHT_WHITE);
			printf("%s",&buf[5]);
			color(DIM_WHITE);
			printf(") ");
			}
		if ((!struncmp(buf,"room=",5))
		   && (strucmp(&buf[5],room_name))) {
			color(DIM_WHITE);
			printf("in ");
			color(BRIGHT_MAGENTA);
			printf("%s> ",&buf[5]);
			}

		if (!struncmp(buf,"node=",5)) {
			if ( (room_flags&QR_NETWORK)
			   || ((strucmp(&buf[5],serv_info.serv_nodename)
   			   &&(strucmp(&buf[5],serv_info.serv_fqdn)))))
				{
				color(DIM_WHITE);
				printf("@");
				color(BRIGHT_YELLOW);
				printf("%s ",&buf[5]);
				}
			if ((!strucmp(&buf[5],serv_info.serv_nodename))
   			   ||(!strucmp(&buf[5],serv_info.serv_fqdn)))
				{
				strcpy(reply_to,from);
				}
			else {
				sprintf(reply_to,"%s @ %s",from,&buf[5]);
				}
			}

		if (!struncmp(buf,"rcpt=",5)) {
			color(DIM_WHITE);
			printf("to ");
			color(BRIGHT_CYAN);
			printf("%s ",&buf[5]);
			}
		if (!struncmp(buf,"time=",5)) {
			now=atol(&buf[5]);
			tm=(struct tm *)localtime(&now);
			strcpy(buf,asctime(tm)); buf[strlen(buf)-1]=0;
			strcpy(&buf[16],&buf[19]);
			color(BRIGHT_MAGENTA);
			printf("%s ",&buf[4]);
			}
		}

	if (nhdr==1) {
		if (!is_room_aide) {
			printf(" ****");
			}
		else {
			printf(" %s",from);
			}
		}
	printf("\n");
	if (pagin == 1) color(BRIGHT_WHITE);
	++lines_printed;
	lines_printed = checkpagin(lines_printed,pagin,screenheight);

	if (strlen(m_subject)>0) {
		printf("Subject: %s\n",m_subject);
		++lines_printed;
		lines_printed = checkpagin(lines_printed,pagin,screenheight);
		}

	if (format_type == 0) {
		fr=fmout(screenwidth,NULL,
			((pagin==1) ? 1 : 0),
			screenheight,(-1),1);
		}
	else {
		while(serv_gets(buf), strcmp(buf,"000")) {
			if (sigcaught==0) {
				printf("%s\n",buf);
				lines_printed = lines_printed + 1 +
					(strlen(buf)/screenwidth);
				lines_printed =
					checkpagin(lines_printed,pagin,screenheight);
				}
			}
		fr = sigcaught;
		}
	printf("\n");
	++lines_printed;
	lines_printed = checkpagin(lines_printed,pagin,screenheight);

	if (pagin == 1) color(DIM_WHITE);
	sttybbs(0);
	return(fr);
	}

/*
 * replace string function for the built-in editor
 */
void replace_string(char *filename, long int startpos)
{
	char buf[512];
	char srch_str[128];
	char rplc_str[128];
	FILE *fp;
	int a;
	long rpos,wpos;
	char *ptr;
	int substitutions = 0;
	long msglen = 0L;

	printf("Enter text to be replaced:\n: ");
	getline(srch_str,128);
	if (strlen(srch_str)==0) return;
	
	printf("Enter text to replace it with:\n: ");
	getline(rplc_str,128);

	fp=fopen(filename,"r+");
	if (fp==NULL) return;

	wpos=startpos;
	fseek(fp,startpos,0);
	strcpy(buf,"");
	while (a=getc(fp), a>0) {
		++msglen;
		buf[strlen(buf)+1] = 0;
		buf[strlen(buf)] = a;
		if ( strlen(buf) >= strlen(srch_str) ) {
			ptr=&buf[strlen(buf)-strlen(srch_str)];
			if (!struncmp(ptr,srch_str,strlen(srch_str))) {
				strcpy(ptr,rplc_str);
				++substitutions;
				}
			}
		if (strlen(buf)>384) {
			rpos=ftell(fp);
			fseek(fp,wpos,0);
			fwrite((char *)buf,128,1,fp);
			strcpy(buf,&buf[128]);
			wpos = ftell(fp);
			fseek(fp,rpos,0);
			}
		}
	fseek(fp,wpos,0);
	if (strlen(buf)>0) fwrite((char *)buf,strlen(buf),1,fp);
	putc(0,fp);
	fclose(fp);
	printf("<R>eplace made %d substitution(s).\n\n",substitutions);
	}


int make_message(char *filename, char *recipient, int anon_type, int format_type, int mode)
                	/* temporary file name */
                 	/* NULL if it's not mail */
              		/* see MES_ types in header file */
                
         
{ 
	FILE *fp;
	int a,b,e_ex_code;
	time_t now;
	long beg;
	char datestr[64];
	int cksum = 0;

	if (mode==2) if (strlen(editor_path)==0) {
		printf("*** No editor available, using built-in editor\n");
		mode=0;
		}

	time(&now);
	strcpy(datestr,asctime(localtime(&now)));
	datestr[strlen(datestr)-1] = 0;

	if (room_flags & QR_ANONONLY) {
		printf(" ****");
		}
	else {
		printf(" %s from %s",datestr,fullname);
		if (strlen(recipient)>0) printf(" to %s",recipient);
		}
	printf("\n");

	beg = 0L;

	if (mode==1) printf("(Press ctrl-d when finished)\n");
	if (mode==0) {
		fp=fopen(filename,"r");
		if (fp!=NULL) {
			fmout(screenwidth,fp,0,screenheight,0,0);
			beg = ftell(fp);
			fclose(fp);
			}
		else {
			fp=fopen(filename,"w");
			fclose(fp);
			}
		}

ME1:	switch(mode) {

	   case 0:
		fp=fopen(filename,"r+");
		citedit(fp,beg);
		fclose(fp);
		goto MECR;

	   case 1:
		fp=fopen(filename,"w");
		do {
			a=inkey(); if (a==255) a=32;
			if (a==13) a=10;
			if (a!=4) {
				putc(a,fp);
				putc(a,stdout);
				}
			if (a==10) putc(13,stdout);
			} while(a!=4);
		fclose(fp);
		break;

	   case 2:
		e_ex_code = 1;	/* start with a failed exit code */
		editor_pid=fork();
		cksum = file_checksum(filename);
		if (editor_pid==0) {
			chmod(filename,0600);
			sttybbs(SB_RESTORE);
			execlp(editor_path,editor_path,filename,NULL);
			exit(1);
			}
		if (editor_pid>0) do {
			e_ex_code = 0;
			b=ka_wait(&e_ex_code);
			} while((b!=editor_pid)&&(b>=0));
		editor_pid = (-1);
		sttybbs(0);
		break;
		}

MECR:	if (mode==2) {
		if (file_checksum(filename) == cksum) {
			printf("*** Aborted message.\n");
			e_ex_code = 1;
			}
		if (e_ex_code==0) goto MEFIN;
		goto MEABT2;
		}
MECR1:	printf("Entry cmd (? for options) -> ");
MECR2:	b=inkey();
	if (b==NEXT_KEY) b='n';
	if (b==STOP_KEY) b='s';
	b=(b&127); b=tolower(b);
	if (b=='?') {
		printf("Help\n");
		formout("saveopt");
		goto MECR1;
		}
	if (b=='a') { printf("Abort\n");	goto MEABT;	}
	if (b=='c') { printf("Continue\n");	goto ME1;	}
	if (b=='s') { printf("Save message\n");	goto MEFIN;	} 
	if (b=='p') {
		printf("Print formatted\n");
		printf(" %s from %s",datestr,fullname);
		if (strlen(recipient)>0) printf(" to %s",recipient);
		printf("\n");
		fp=fopen(filename,"r");
		if (fp!=NULL) {
			fmout(screenwidth,fp,
				((userflags & US_PAGINATOR) ? 1 : 0),
				screenheight,0,0);
			beg = ftell(fp);
			fclose(fp);
			}
		goto MECR;
		}
	if (b=='r') {
		printf("Replace string\n");
		replace_string(filename,0L);
		goto MECR;
		}
	if (b=='h') {
		printf("Hold message\n");
		return(2);
		}
	goto MECR2;

MEFIN:	return(0);

MEABT:	printf("Are you sure? ");
	if (yesno()==0) goto ME1;
MEABT2:	unlink(filename);
	return(2);
	}

/*
 * transmit message text to the server
 */
void transmit_message(FILE *fp)
{
	char buf[256];
	int ch,a;
	
	strcpy(buf,"");
	while (ch=getc(fp), (ch>=0)) {
		if (ch==10) {
			if (!strcmp(buf,"000")) strcpy(buf,">000");
			serv_puts(buf);
			strcpy(buf,"");
			}
		else {
			a = strlen(buf);
			buf[a+1] = 0;
			buf[a] = ch;
			if ((ch==32)&&(strlen(buf)>200)) {
				buf[a]=0;
				if (!strcmp(buf,"000")) strcpy(buf,">000");
				serv_puts(buf);
				strcpy(buf,"");
				}
			if (strlen(buf)>250) {
				if (!strcmp(buf,"000")) strcpy(buf,">000");
				serv_puts(buf);
				strcpy(buf,"");
				}
			}
		}
	serv_puts(buf);
	}



/*
 * entmsg()  -  edit and create a message
 *              returns 0 if message was saved
 */
int entmsg(int is_reply, int c)
             		/* nonzero if this was a <R>eply command */
       {		/* */
	char buf[300];
	char cmd[256];
	int a,b;
	int need_recp = 0;
	int mode;
	long highmsg;
	FILE *fp;

	if (c>0) mode=1;
	else mode=0;

	sprintf(cmd,"ENT0 0||0|%d",mode);
	serv_puts(cmd);
	serv_gets(cmd);

	if ((strncmp(cmd,"570",3)) && (strncmp(cmd,"200",3))) {
		printf("%s\n",&cmd[4]);
		return(1);
		}
	need_recp = 0;
	if (!strncmp(cmd,"570",3)) need_recp = 1;

	if ((userflags & US_EXPERT) == 0) formout("entermsg");
		
	strcpy(buf,"");
	if (need_recp==1) {
		if (axlevel>=2) {
			if (is_reply) {
				strcpy(buf,reply_to);
				}
			else {
				printf("Enter recipient: ");
				getline(buf, 60);
				if (strlen(buf)==0) return(1);
				}
			}
		else strcpy(buf,"sysop");
		}

	b=0;
	if (room_flags&QR_ANONOPT) {
		printf("Anonymous (Y/N)? ");
		if (yesno()==1) b=1;
		}

/* if it's mail, we've got to check the validity of the recipient... */
	if (strlen(buf)>0) {
		sprintf(cmd,"ENT0 0|%s|%d|%d",buf,b,mode);
		serv_puts(cmd);
		serv_gets(cmd);
		if (cmd[0]!='2') {
			printf("%s\n",&cmd[4]);
			return(1);
			}
		}

/* learn the number of the newest message in in the room, so we can tell
 * upon saving whether someone else has posted too
 */
	num_msgs = 0;
	serv_puts("MSGS LAST|1");
	serv_gets(cmd);
	if (cmd[0]!='1') {
		printf("%s\n",&cmd[5]);
		}
	else {
		while (serv_gets(cmd), strcmp(cmd,"000")) {
			msg_arr[num_msgs++] = atol(cmd);
			}
		}

/* now put together the message */
	if ( make_message(temp,buf,b,0,c) != 0 ) return(2);

/* and send it to the server */
	sprintf(cmd,"ENT0 1|%s|%d|%d||",buf,b,mode);
	serv_puts(cmd);
	serv_gets(cmd);
	if (cmd[0]!='4') {
		printf("%s\n",&cmd[4]);
		return(1);
		}
	fp=fopen(temp,"r");
	if (fp!=NULL) {
		transmit_message(fp);
		fclose(fp);
		}
	serv_puts("000");
	unlink(temp);
	
	highmsg = msg_arr[num_msgs - 1];
	num_msgs = 0;
	serv_puts("MSGS NEW");
	serv_gets(cmd);
	if (cmd[0]!='1') {
		printf("%s\n",&cmd[5]);
		}
	else {
		while (serv_gets(cmd), strcmp(cmd,"000")) {
			msg_arr[num_msgs++] = atol(cmd);
			}
		}

	/* get new highest message number in room to set lrp for goto... */
	maxmsgnum = msg_arr[num_msgs - 1];

	/* now see if anyone else has posted in here */
	b=(-1);
	for (a=0; a<num_msgs; ++a) if (msg_arr[a]>highmsg) ++b;

	/* in the Mail> room, this algorithm always counts one message
	 * higher than in public rooms, so we decrement it by one */
	if (need_recp) --b;

	if (b==1) printf(
"*** 1 additional message has been entered in this room by another user.\n");
	if (b>1) printf(
"*** %d additional messages have been entered in this room by other users.\n",b);

	return(0);
	}

void process_quote(void) {	/* do editing on quoted file */
FILE *qfile,*tfile;
char buf[128];
int line,qstart,qend;

	/* Unlink the second temp file as soon as it's opened, so it'll get
	 * deleted even if the program dies
	 */
	qfile = fopen(temp2,"r");
	unlink(temp2);

	/* Display the quotable text with line numbers added */
	line = 0;
	fgets(buf,128,qfile);
	while (fgets(buf,128,qfile)!=NULL) {
		printf("%2d %s",++line,buf);
		}
	printf("Begin quoting at [ 1] : ");
	getline(buf,3);
	qstart = (buf[0]==0) ? (1) : atoi(buf);
	printf("  End quoting at [%d] : ",line);
	getline(buf,3);
	qend = (buf[0]==0) ? (line) : atoi(buf);
	rewind(qfile);
	line=0;
	fgets(buf,128,qfile);
	tfile=fopen(temp,"w");
	while(fgets(buf,128,qfile)!=NULL) {
		if ((++line>=qstart)&&(line<=qend)) fprintf(tfile," >%s",buf);
		}
	fprintf(tfile," \n");
	fclose(qfile);
	fclose(tfile);
	chmod(temp,0666);
	}


void readmsgs(int c, int rdir, int q)	/* read contents of a room */
      		/* 0=Read all  1=Read new  2=Read old 3=Read last q */
         	/* 1=Forward (-1)=Reverse */
      		/* Number of msgs to read (if c==3) */
	{
	int a,b,e,f,g,start;
	int hold_sw = 0;
	char arcflag = 0;
	char quotflag = 0;
	int hold_color = 0;
	char prtfile[PATH_MAX];
	char pagin;
	char cmd[256];
	char targ[ROOMNAMELEN];
	char filename[256];

	signal(SIGINT,SIG_IGN);
	signal(SIGQUIT,SIG_IGN);

	if (c<0) b=(MAXMSGS-1);
	else b=0;

	sprintf(prtfile, tmpnam(NULL));

	num_msgs = 0;
	strcpy(cmd,"MSGS ");
	switch(c) {
		case 0:	strcat(cmd,"ALL");
			break;
		case 1:	strcat(cmd,"NEW");
			break;
		case 2:	strcat(cmd,"OLD");
			break;
		case 3:	sprintf(&cmd[strlen(cmd)], "LAST|%d", q);
			break;
		}
	serv_puts(cmd);
	serv_gets(cmd);
	if (cmd[0]!='1') {
		printf("%s\n",&cmd[5]);
		}
	else {
		while (serv_gets(cmd), strcmp(cmd,"000")) {
			if (num_msgs == MAXMSGS) {
				memcpy(&msg_arr[0], &msg_arr[1],
					(sizeof(long) * (MAXMSGS - 1)) );
				--num_msgs;
				}
			msg_arr[num_msgs++] = atol(cmd);
			}
		}

	lines_printed = 0;

	/* this loop cycles through each message... */
	start = ( (rdir==1) ? 0 : (num_msgs-1) );
	for (a=start; ((a<num_msgs)&&(a>=0)); a=a+rdir) {
		while (msg_arr[a]==0L) {
			a=a+rdir; if ((a==MAXMSGS)||(a==(-1))) return;
			}

RAGAIN:		pagin=((arcflag==0)&&(quotflag==0)&&
			(userflags & US_PAGINATOR)) ? 1 : 0;

	/* If we're doing a quote, set the screenwidth to 72 temporarily */
		if (quotflag) {
			hold_sw = screenwidth;
			screenwidth = 72;
			}

	/* If printing or archiving, set the screenwidth to 80 temporarily */
		if (arcflag) {
			hold_sw = screenwidth;
			screenwidth = 80;
			}

	/* now read the message... */
		e=read_message(msg_arr[a],pagin);

	/* ...and set the screenwidth back if we have to */
		if ((quotflag)||(arcflag)) {
			screenwidth = hold_sw;
			}
RMSGREAD:	fflush(stdout);
		highest_msg_read = msg_arr[a];
		if (quotflag) {
			freopen("/dev/tty","r+",stdout);
			quotflag=0;
			enable_color = hold_color;
			process_quote();
			}
		if (arcflag) {
			freopen("/dev/tty","r+",stdout);
			arcflag=0;
			enable_color = hold_color;
			f=fork();
			if (f==0) {
				freopen(prtfile, "r", stdin);
				sttybbs(SB_RESTORE);
				ka_system(printcmd);
				sttybbs(SB_NO_INTR);
				unlink(prtfile);
				exit(0);
				}
			if (f>0) do {
				g=wait(NULL);
				} while((g!=f)&&(g>=0));
			printf("Message printed.\n");
			}
		if (e==3) return;
		if ( ((userflags&US_NOPROMPT)||(e==2)) 
		   && (((room_flags&QR_MAILBOX)==0)
		     ||(rc_force_mail_prompts==0))  ) {
			e='n';
			}
		else {
			color(DIM_WHITE);
			printf("(");
			color(BRIGHT_WHITE);
			printf("%d",num_msgs-a-1);
			color(DIM_WHITE);
			printf(") ");

			if (is_mail==1) keyopt("<R>eply ");
			keyopt("<B>ack <A>gain <Q>uote <N>ext <S>top <?>Help/others -> ");
			do {
				lines_printed = 2;
				e=(inkey()&127); e=tolower(e);
/* return key same as <N> */ 	if (e==13) e='n';
/* space key same as <N> */ 	if (e==32) e='n';
/* del/move for aides only */	if ((!is_room_aide)
				    &&((room_flags&QR_MAILBOX)==0)) {
					if ((e=='d')||(e=='m')) e=0;
					}
/* print only if available */	if ((e=='p')&&(strlen(printcmd)==0)) e=0;
/* can't move from Mail> */	if ((e=='m')&&(is_mail==1)) e=0;
/* can't reply in public rms */	if ((e=='r')&&(is_mail!=1)) e=0;
/* can't file if not allowed */	if ((e=='f')&&(rc_allow_attachments==0)) e=0;
				} while((e!='a')&&(e!='n')&&(e!='s')
					&&(e!='d')&&(e!='m')&&(e!='p')
					&&(e!='q')&&(e!='b')&&(e!='h')
					&&(e!='r')&&(e!='f')&&(e!='?'));
			switch(e) {
				case 's':	printf("Stop\r");	break;
				case 'a':	printf("Again\r");	break;
				case 'd':	printf("Delete\r");	break;
				case 'm':	printf("Move\r");	break;
				case 'n':	printf("Next\r");	break;
				case 'p':	printf("Print\r");	break;
				case 'q':	printf("Quote\r");	break;
				case 'b':	printf("Back\r");	break;
				case 'h':	printf("Header\r");	break;
				case 'r':	printf("Reply\r");	break;
				case 'f':	printf("File\r");	break;
				case '?':	printf("? <help>\r");	break;
				}
			if (userflags & US_DISAPPEAR)
				printf("\r%79s\r","");
			else
				printf("\n");
			fflush(stdout);
			}
		switch(e) {
		   case '?':	printf("Options available here:\n");
				printf(" ?  Help (prints this message)\n");
				printf(" S  Stop reading immediately\n");
				printf(" A  Again (repeats last message)\n");
				printf(" N  Next (continue with next message)\n");
				printf(" B  Back (go back to previous message)\n");
				if ((is_room_aide)
				    ||(room_flags&QR_MAILBOX)) {
					printf(" D  Delete this message\n");
					printf(" M  Move message to another room\n");
					}
				if (strlen(printcmd)>0)
					printf(" P  Print this message\n");
				printf(" Q  Quote portions of this message for your next post\n");
				printf(" H  Headers (display message headers only)\n");
				if (is_mail)
					printf(" R  Reply to this message\n");
				if (rc_allow_attachments)
					printf(" F  (save attachments to a file)\n");
				printf("\n");
				goto RMSGREAD;
		   case 'p':	fflush(stdout);
				freopen(prtfile,"w",stdout);
				arcflag = 1;
				hold_color = enable_color;
				enable_color = 0;
				goto RAGAIN;
		   case 'q':	fflush(stdout);
				freopen(temp2,"w",stdout);
				quotflag = 1;
				hold_color = enable_color;
				enable_color = 0;
				goto RAGAIN;
		   case 's':	return;
		   case 'a':	goto RAGAIN;
		   case 'b':	a=a-(rdir*2);
				break;
		   case 'm':	newprompt("Enter target room: ",
					targ,ROOMNAMELEN-1);
				if (strlen(targ)>0) {
					sprintf(cmd,"MOVE %ld|%s",
						msg_arr[a],targ);
					serv_puts(cmd);
					serv_gets(cmd);
					printf("%s\n",&cmd[4]);
					if (cmd[0]=='2') msg_arr[a]=0L;
					}
				else {
					goto RMSGREAD;
					}
				if (cmd[0]!='2') goto RMSGREAD;
				break;
		   case 'f':	newprompt("Which section? ", filename,
					((sizeof filename) -1));
				snprintf(cmd, sizeof cmd,
					"OPNA %ld|%s", msg_arr[a], filename);
				serv_puts(cmd);
				serv_gets(cmd);
				if (cmd[0]=='2') {
					extract(filename, &cmd[4], 2);
					download_to_local_disk(filename,
						extract_int(&cmd[4], 0));
					}
				else {
					printf("%s\n",&cmd[4]);
					}
				goto RMSGREAD;
		   case 'd':	printf("*** Delete this message? ");
				if (yesno()==1) {
					sprintf(cmd,"DELE %ld",msg_arr[a]);
					serv_puts(cmd);
					serv_gets(cmd);
					printf("%s\n",&cmd[4]);
					if (cmd[0]=='2') msg_arr[a]=0L;
					}
				else {
					goto RMSGREAD;
					}
				break;
		   case 'h':	read_message(msg_arr[a],READ_HEADER);
				goto RMSGREAD;
		   case 'r':	entmsg(1,(DEFAULT_ENTRY==46 ? 2 : 0));
				goto RMSGREAD;
			}
		} /* end for loop */
	} /* end read routine */




/*
 * View and edit a system message
 */
void edit_system_message(char *which_message)
{
	char desc[64];
	char read_cmd[64];
	char write_cmd[64];

	sprintf(desc, "system message '%s'", which_message);
	sprintf(read_cmd, "MESG %s", which_message);
	sprintf(write_cmd, "EMSG %s", which_message);
	do_edit(desc, read_cmd, "NOOP", write_cmd);
	}
