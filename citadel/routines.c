/* Citadel/UX support routines */
/* $Id$ */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <pwd.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <limits.h>

#define ROUTINES_C

#include "citadel.h"
#include "routines.h"
#include "commands.h"
#include "tools.h"

void sttybbs(int cmd);
void newprompt(char *prompt, char *str, int len);
void val_user(char *, int);
void formout(char *name);
void logoff(int code);
void set_keepalives(int s);
void strprompt(char *prompt, char *str, int len);
void newprompt(char *prompt, char *str, int len);
void color(int colornum);

#define IFAIDE if(axlevel>=6)
#define IFNAIDE if (axlevel<6)

extern unsigned userflags;
extern char *axdefs[7];
extern char sigcaught;
extern struct CtdlServInfo serv_info;
extern char rc_floor_mode;
extern int rc_ansi_color;

int struncmp(char *lstr, char *rstr, int len)
{
	int pos = 0;
	char lc,rc;
	while (pos<len) {
		lc=tolower(lstr[pos]);
		rc=tolower(rstr[pos]);
		if ((lc==0)&&(rc==0)) return(0);
		if (lc<rc) return(-1);
		if (lc>rc) return(1);
		pos=pos+1;
		}
	return(0);
	}


/* 
 * check for the presence of a character within a string (returns count)
 */
int haschar(char *st, int ch)
{
	int a,b;
	b=0;
	for (a=0; a<strlen(st); ++a) if (st[a]==ch) ++b;
	return(b);
	}


void back(int spaces) /* Destructive backspace */
            {
int a;
	for (a=1; a<=spaces; ++a) {
		putc(8,stdout); putc(32,stdout); putc(8,stdout);
		}
	}

void hit_any_key(void) {		/* hit any key to continue */
	int a,b;

	printf("%s\r",serv_info.serv_moreprompt);
	sttybbs(0);
	b=inkey();
	for (a=0; a<strlen(serv_info.serv_moreprompt); ++a)
		putc(' ',stdout);
	putc(13,stdout);
	sttybbs(1);
	if (b==NEXT_KEY) sigcaught = SIGINT;
	if (b==STOP_KEY) sigcaught = SIGQUIT;
	}

/*
 * change a user's access level
 */
void edituser(void)
{
	char buf[256];
	char who[256];
	char pass[256];
	int flags;
	int timescalled;
	int posted;
	int axlevel;
	long usernum;
	time_t lastcall;
	int userpurge;

	newprompt("User name: ",who,25);
	sprintf(buf,"AGUP %s",who);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0]!='2') {
		printf("%s\n",&buf[4]);
		return;
		}
	extract(who, &buf[4], 0);
	extract(pass, &buf[4], 1);
	flags = extract_int(&buf[4], 2);
	timescalled = extract_int(&buf[4], 3);
	posted = extract_int(&buf[4], 4);
	axlevel = extract_int(&buf[4], 5);
	usernum = extract_long(&buf[4], 6);
	lastcall = extract_long(&buf[4], 7);
	userpurge = extract_int(&buf[4], 8);

	val_user(who, 0); /* Display registration */
	strprompt("Password", pass, 19);
	axlevel = intprompt("Access level", axlevel, 0, 6);
	timescalled = intprompt("Times called", timescalled, 0, INT_MAX);
	posted = intprompt("Messages posted", posted, 0, INT_MAX);
	lastcall = (boolprompt("Set last call to now", 0)?time(NULL):lastcall);
	userpurge = intprompt("Purge time (in days, 0 for system default",
				userpurge, 0, INT_MAX);

	sprintf(buf, "ASUP %s|%s|%d|%d|%d|%d|%ld|%ld|%d",
		who, pass, flags, timescalled, posted, axlevel, usernum,
		lastcall, userpurge);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0]!='2') {
		printf("%s\n",&buf[4]);
		}
	}


int set_attr(int sval, char *prompt, unsigned int sbit)
{
	int a;
	int temp;

	temp = sval;
	color(3);
	printf("%45s [", prompt);
	color(1);
	printf("%3s", ((temp&sbit) ? "Yes":"No"));
	color(3);
	printf("]? ");
	color(2);
	a=yesno_d(temp&sbit);
	color(7);
	temp=(temp|sbit);
	if (!a) temp=(temp^sbit);
	return(temp);
	}

/*
 * modes are:  0 - .EC command, 1 - .EC for new user,
 *             2 - toggle Xpert mode  3 - toggle floor mode
 */
void enter_config(int mode)
{
 	int width,height,flags;
	char buf[128];

	sprintf(buf,"GETU");
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0]!='2') {
		printf("%s\n",&buf[4]);
		return;
		}

	width = extract_int(&buf[4],0);
	height = extract_int(&buf[4],1);
	flags = extract_int(&buf[4],2);

	if ((mode==0)||(mode==1)) {

	 width = intprompt("Enter your screen width",width,20,255);
	 height = intprompt("Enter your screen height",height,3,255);
 
	 flags = set_attr(flags,
		"Are you an experienced Citadel user",US_EXPERT);
	 if ( ((flags&US_EXPERT)==0) && (mode==1))
		return;
	 flags = set_attr(flags,
		"Print last old message on New message request",US_LASTOLD);
	 if ((flags&US_EXPERT)==0) formout("unlisted");
	 flags = set_attr(flags,"Be unlisted in userlog",US_UNLISTED);
	 flags = set_attr(flags,"Suppress message prompts",US_NOPROMPT);
	 if ((flags & US_NOPROMPT)==0)
	    flags = set_attr(flags,"Use 'disappearing' prompts",US_DISAPPEAR);
	 flags = set_attr(flags,
		"Pause after each screenful of text",US_PAGINATOR);
	 if (rc_floor_mode == RC_DEFAULT) {
	  flags = set_attr(flags,
		"View rooms by floor",US_FLOORS);
	  }
	 if (rc_ansi_color == 3) {
	  flags = set_attr(flags,
		"Enable color support",US_COLOR);
	  }
	 }

	if (mode==2) {
	 if (flags & US_EXPERT) {
		flags = (flags ^ US_EXPERT);
		printf("Expert mode now OFF\n");
		}
	 else {
		flags = (flags | US_EXPERT);
		printf("Expert mode now ON\n");
		}
	 }

	if (mode==3) {
	 if (flags & US_FLOORS) {
		flags = (flags ^ US_FLOORS);
		printf("Floor mode now OFF\n");
		}
	 else {
		flags = (flags | US_FLOORS);
		printf("Floor mode now ON\n");
		}
	 }

	sprintf(buf,"SETU %d|%d|%d",width,height,flags);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0]!='2') printf("%s\n",&buf[4]);
	userflags = flags;
}

/*
 * getstring()  -  get a line of text from a file
 *		   ignores lines beginning with "#"
 */
int getstring(FILE *fp, char *string)
{
	int a,c;
	do {
		strcpy(string,"");
		a=0;
		do {
			c=getc(fp);
			if (c<0) {
				string[a]=0;
				return(-1);
				}
			string[a++]=c;
			} while(c!=10);
			string[a-1]=0;
		} while(string[0]=='#');
	return(strlen(string));
	}

int pattern(char *search, char *patn)	/* Searches for patn in search string */
              
            
{
	int a,b;
	for (a=0; a<strlen(search); ++a)
	{	b=struncmp(&search[a],patn,strlen(patn));
		if (b==0) return(b);
		}
	return(-1);
}

void interr(int errnum)	/* display internal error as defined in errmsgs */
            {
	printf("*** INTERNAL ERROR %d\n",errnum);
	printf("(Press any key to continue)\n");
	inkey();
	logoff(errnum);
}



/*
 * Check to see if we need to pause at the end of a screen.
 * If we do, we have to disable server keepalives during the pause because
 * we are probably in the middle of a server operation and the NOOP command
 * would confuse everything.
 */
int checkpagin(int lp, int pagin, int height)
{
	if (pagin!=1) return(0);
	if (lp>=(height-1)) {
		set_keepalives(KA_NO);
		hit_any_key();
		set_keepalives(KA_YES);
		return(0);
		}
	return(lp);
	}


void strproc(char *string)
{
	int a;

	if (strlen(string)==0) return;

	/* Convert non-printable characters to blanks */
	for (a=0; a<strlen(string); ++a) {
		if (string[a]<32) string[a]=32;
		if (string[a]>126) string[a]=32;
		}

	/* Remove leading and trailing blanks */
	while(string[0]<33) strcpy(string,&string[1]);
	while(string[strlen(string)-1]<33) string[strlen(string)-1]=0;

	/* Remove double blanks */
	for (a=0; a<strlen(string); ++a) {
		if ((string[a]==32)&&(string[a+1]==32)) {
			strcpy(&string[a],&string[a+1]);
			a=0;
			}
		}

	/* remove characters which would interfere with the network */
	for (a=0; a<strlen(string); ++a) {
		if (string[a]=='!') strcpy(&string[a],&string[a+1]);
		if (string[a]=='@') strcpy(&string[a],&string[a+1]);
		if (string[a]=='_') strcpy(&string[a],&string[a+1]);
		if (string[a]==',') strcpy(&string[a],&string[a+1]);
		if (string[a]=='%') strcpy(&string[a],&string[a+1]);
		if (string[a]=='|') strcpy(&string[a],&string[a+1]);
		}

	}


#ifndef HAVE_STRERROR
/*
 * replacement strerror() for systems that don't have it
 */
char *strerror(int e)
{
	static char buf[32];

	sprintf(buf,"errno = %d",e);
	return(buf);
	}
#endif


void progress(long int curr, long int cmax)
{
	static long dots_printed;
	long a;

	if (curr==0) {
		printf(".......................................");
		printf(".......................................\r");
		fflush(stdout);
		dots_printed = 0;
		}
	else if (curr==cmax) {
		printf("\r%79s\n","");
		}
	else {
		a=(curr * 100) / cmax;
		a=a*78; a=a/100;
		while (dots_printed < a) {
			printf("*");
			++dots_printed;
			fflush(stdout);
			}
		}
	}


/*
 * NOT the same locate_host() in locate_host.c.  This one just does a
 * 'who am i' to try to discover where the user is...
 */
void locate_host(char *hbuf)
{
	char buf[256];
	FILE *who;
	int a,b;

	who = (FILE *)popen("who am i","r");
	if (who==NULL) {
		strcpy(hbuf,serv_info.serv_fqdn);
		return;	
		}
	fgets(buf,256,who);
	pclose(who);

	b = 0;
	for (a=0; a<strlen(buf); ++a) {
		if ((buf[a]=='(')||(buf[a]==')')) ++b;
		}
	if (b<2) {
		strcpy(hbuf,serv_info.serv_fqdn);
		return;
		}

	for (a=0; a<strlen(buf); ++a) {
		if (buf[a]=='(') {
			strcpy(buf,&buf[a+1]);
			}
		}
	for (a=0; a<strlen(buf); ++a) {
		if (buf[a]==')') buf[a] = 0;
		}

	if (strlen(buf)==0) strcpy(hbuf,serv_info.serv_fqdn);
	else strncpy(hbuf,buf,24);
	}

/*
 * miscellaneous server commands (testing, etc.)
 */
void misc_server_cmd(char *cmd) {
	char buf[256];

	serv_puts(cmd);
	serv_gets(buf);
	printf("%s\n",buf);
	if (buf[0]=='1') {
		set_keepalives(KA_NO);
		while (serv_gets(buf), strcmp(buf,"000")) {
			printf("%s\n",buf);
			}
		set_keepalives(KA_YES);
		return;
		}
	if (buf[0]=='4') {
		do {
			newprompt("> ",buf,255);
			serv_puts(buf);
			} while(strcmp(buf,"000"));
		return;
		}
	}


/*
 * compute the checksum of a file
 */
int file_checksum(char *filename)
{
	int cksum = 0;
	int ch;
	FILE *fp;

	fp = fopen(filename,"r");
	if (fp == NULL) return(0);

	/* yes, this algorithm may allow cksum to overflow, but that's ok
	 * as long as it overflows consistently, which it will.
	 */
	while (ch=getc(fp), ch>=0) {
		cksum = (cksum + ch);
		}

	fclose(fp);
	return(cksum);
	}

/*
 * nuke a directory and its contents
 */
int nukedir(char *dirname)
{
	DIR *dp;
	struct dirent *d;
	char filename[256];

	dp = opendir(dirname);
	if (dp == NULL) {
		return(errno);
		}

	while (d = readdir(dp), d != NULL) {
		sprintf(filename, "%s/%s", dirname, d->d_name);
		unlink(filename);
		}

	closedir(dp);
	return(rmdir(dirname));
	}
