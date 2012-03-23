/*
 * Client-side support functions.
 */

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

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_UTMP_H
#include <utmp.h>
#endif
#ifdef HAVE_UTMPX_H
#include <utmpx.h>
#endif

#include <libcitadel.h>
#include "citadel.h"
#include "citadel_ipc.h"
#include "screen.h"

#ifndef HAVE_GETUTLINE
struct utmp *getutline(struct utmp *ut);
#endif

#define ROUTINES_C

#include "citadel.h"
#include "routines.h"
#include "commands.h"
#include "citadel_decls.h"
#include "routines2.h"
#include "tuiconfig.h"

#define IFAIDE if(axlevel>=AxAideU)
#define IFNAIDE if (axlevel<AxAideU)

extern unsigned userflags;
//extern char *axdefs[8];
extern char sigcaught;
extern char rc_floor_mode;
extern int rc_ansi_color;
extern int rc_prompt_control;

/* Destructive backspace */
void back(int spaces) {
	int a;
	for (a=0; a<spaces; ++a) {
		scr_putc(8);
		scr_putc(32);
		scr_putc(8);
	}
}

/*
 * Edit or delete a user (cmd=25 to edit/create, 96 to delete)
 */
void edituser(CtdlIPC *ipc, int cmd)
{
	char buf[SIZ];
	char who[USERNAME_SIZE];
	char newname[USERNAME_SIZE];
	struct ctdluser *user = NULL;
	int newnow = 0;
	int r;				/* IPC response code */
	int change_name = 0;

	strcpy(newname, "");

	newprompt("User name: ", who, 29);
	while ((r = CtdlIPCAideGetUserParameters(ipc, who, &user, buf)) / 100 != 2) {
		scr_printf("%s\n", buf);
		if (cmd == 25) {
			scr_printf("Do you want to create this user? ");
			if (yesno()) {
				r = CtdlIPCCreateUser(ipc, who, 0, buf);
				if (r / 100 == 2) {
					newnow = 1;
					continue;
				}
				scr_printf("%s\n", buf);
			}
		}
		free(user);
		return;
	}

	if (cmd == 25) {
		val_user(ipc, user->fullname, 0); /* Display registration */

		if (!newnow) {
			change_name = 1;
			while (change_name == 1) {
				if (boolprompt("Change name", 0)) {
					strprompt("New name", newname, USERNAME_SIZE-1);
					r = CtdlIPCRenameUser(ipc, user->fullname, newname, buf);
					if (r / 100 != 2) {
						scr_printf("%s\n", buf);
					}
					else {
						strcpy(user->fullname, newname);
						change_name = 0;
					}
				}
				else {
					change_name = 0;
				}
			}
		}

		if (newnow || boolprompt("Change password", 0)) {
			strprompt("Password", user->password, -19);
		}
	
		user->axlevel = intprompt("Access level", user->axlevel, 0, 6);
		if (boolprompt("Permission to send Internet mail", (user->flags & US_INTERNET)))
			user->flags |= US_INTERNET;
		else
			user->flags &= ~US_INTERNET;
		if (boolprompt("Ask user to register again", !(user->flags & US_REGIS)))
			user->flags &= ~US_REGIS;
		else
			user->flags |= US_REGIS;
		user->timescalled = intprompt("Times called",
			      	user->timescalled, 0, INT_MAX);
		user->posted = intprompt("Messages posted",
				 	user->posted, 0, INT_MAX);
		user->lastcall = boolprompt("Set last call to now", 0) ?
					time(NULL) : user->lastcall;
		user->USuserpurge = intprompt("Purge time (in days, 0 for system default",
			      	user->USuserpurge, 0, INT_MAX);
	}

	if (cmd == 96) {
		scr_printf("Do you want to delete this user? ");
		if (!yesno()) {
			free(user);
			return;
		}
		user->axlevel = AxDeleted;
	}

	r = CtdlIPCAideSetUserParameters(ipc, user, buf);
	if (r / 100 != 2) {
		scr_printf("%s\n", buf);
	}
	free(user);
}


/* Display a prompt and flip a bit based on whether the user answers
 * yes or no.  Yes=1 and No=0, unless 'backwards' is set to a nonzero value
 * in which case No=1 and Yes=0.
 */
int set_attr(CtdlIPC *ipc, unsigned int sval, char *prompt, unsigned int sbit, int backwards)
{
	int a;
	int temp;

	temp = sval;
	color(DIM_WHITE);
	scr_printf("%50s ", prompt);
	color(DIM_MAGENTA);
	scr_printf("[");
	color(BRIGHT_MAGENTA);

	if (backwards) {
		scr_printf("%3s", ((temp&sbit) ? "No":"Yes"));
	}
	else {
		scr_printf("%3s", ((temp&sbit) ? "Yes":"No"));
	}

	color(DIM_MAGENTA);
	scr_printf("]? ");
	color(BRIGHT_CYAN);
	a = (temp & sbit);
	if (a != 0) a = 1;
	if (backwards) a = 1 - a;
	a = yesno_d(a);
	if (backwards) a = 1 - a;
	color(DIM_WHITE);
	temp = (temp|sbit);
	if (!a) temp = (temp^sbit);
	return(temp);
}

/*
 * modes are:  0 - .EC command, 1 - .EC for new user,
 *             2 - toggle Xpert mode  3 - toggle floor mode
 */
void enter_config(CtdlIPC *ipc, int mode)
{
	char buf[SIZ];
	struct ctdluser *user = NULL;
	int r;				/* IPC response code */

	r = CtdlIPCGetConfig(ipc, &user, buf);
	if (r / 100 != 2) {
		scr_printf("%s\n", buf);
		free(user);
		return;
	}

	if (mode == 0 || mode == 1) {

		user->flags = set_attr(ipc, user->flags,
				       "Are you an experienced Citadel user",
				       US_EXPERT, 0);
		if ((user->flags & US_EXPERT) == 0 && mode == 1) {
			free(user);
			return;
		}

		user->flags = set_attr(
			ipc,
			user->flags,
			"Print last old message on New message request",
			US_LASTOLD,
			0
		);

		user->flags = set_attr(
			ipc,
			user->flags,
			"Prompt after each message",
			US_NOPROMPT,
			1
		);

		if ((user->flags & US_NOPROMPT) == 0) {
			user->flags = set_attr(
				ipc,
				user->flags,
				"Use 'disappearing' prompts",
				US_DISAPPEAR,
				0
			);
		}

		user->flags = set_attr(
			ipc,
			user->flags,
			"Pause after each screenful of text",
			US_PAGINATOR,
			0
		);

		if (rc_prompt_control == 3 && (user->flags & US_PAGINATOR)) {
			user->flags = set_attr(
				ipc,
				user->flags,
				"<N>ext and <S>top work at paginator prompt",
				US_PROMPTCTL,
				0
			);
		}

		if (rc_floor_mode == RC_DEFAULT) {
			user->flags = set_attr(
				ipc,
				user->flags,
				"View rooms by floor",
				US_FLOORS,
				0
			);
		}

		if (rc_ansi_color == 3) {
			user->flags = set_attr(
				ipc,
				user->flags,
				"Enable color support",
				US_COLOR,
				0
			);
		}

	 	if ((user->flags & US_EXPERT) == 0) {
			formout(ipc, "unlisted");
		}

		user->flags = set_attr(
			ipc,
			user->flags,
			"Be unlisted in userlog",
			US_UNLISTED,
			0
		);

		if (!IsEmptyStr(editor_path)) {
			user->flags = set_attr(
				ipc,
				user->flags,
				"Always enter messages with the full-screen editor",
				US_EXTEDIT,
				0
			);
		}

	}

	if (mode == 2) {
		if (user->flags & US_EXPERT) {
			user->flags ^= US_EXPERT;
			scr_printf("Expert mode now OFF\n");
		} else {
			user->flags |= US_EXPERT;
			scr_printf("Expert mode now ON\n");
		}
	}

	if (mode == 3) {
		if (user->flags & US_FLOORS) {
			user->flags ^= US_FLOORS;
			scr_printf("Floor mode now OFF\n");
		} else {
			user->flags |= US_FLOORS;
			scr_printf("Floor mode now ON\n");
		}
	}

	r = CtdlIPCSetConfig(ipc, user, buf);
	if (r / 100 != 2) scr_printf("%s\n", buf);
	userflags = user->flags;
	free(user);
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


/* Searches for patn in search string */
int pattern(char *search, char *patn) {
	int a,b,len;
	
	len = strlen(patn);
	for (a=0; !IsEmptyStr(&search[a]); ++a) {
		b=strncasecmp(&search[a],patn,len);
		if (b==0) return(b);
	}
	return(-1);
}


void strproc(char *string)
{
	int a;

	if (IsEmptyStr(string)) return;

	/* Convert non-printable characters to blanks */
	for (a=0; !IsEmptyStr(&string[a]); ++a) {
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
	static char buf[128];

	snprintf(buf, sizeof buf, "errno = %d",e);
	return(buf);
}
#endif


void progress(CtdlIPC* ipc, unsigned long curr, unsigned long cmax)
{
	static char dots[] =
		"**************************************************";
	char dots_printed[51];
	char fmt[42];
	unsigned long a;

	if (curr >= cmax) {
		scr_printf("\r%79s\r","");
	} else {
		/* a will be range 0-50 rather than 0-100 */
		a=(curr * 50) / cmax;
		sprintf(fmt, "[%%s%%%lds] %%3ld%%%% %%10ld/%%10ld\r", 50 - a);
		strncpy(dots_printed, dots, a);
		dots_printed[a] = 0;
		scr_printf(fmt, dots_printed, "",
				curr * 100 / cmax, curr, cmax);
		scr_flush();
	}
}


/*
 * NOT the same locate_host() in locate_host.c.  This one just does a
 * 'who am i' to try to discover where the user is...
 */
void locate_host(CtdlIPC* ipc, char *hbuf)
{
#ifndef HAVE_UTMP_H
	char buf[SIZ];
	FILE *who;
	int a,b;

	who = (FILE *)popen("who am i","r");
	if (who==NULL) {
		strcpy(hbuf, ipc->ServInfo.fqdn);
		return;	
	}
	fgets(buf,sizeof buf,who);
	pclose(who);

	b = 0;
	for (a=0; !IsEmptyStr(&buf[a]); ++a) {
		if ((buf[a]=='(')||(buf[a]==')')) ++b;
	}
	if (b<2) {
		strcpy(hbuf, ipc->ServInfo.fqdn);
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

	if (IsEmptyStr(buf)) strcpy(hbuf, ipc->ServInfo.fqdn);
	else strncpy(hbuf,buf,24);
#else
	char *tty = ttyname(0);
#ifdef HAVE_GETUTXLINE
	struct utmpx ut, *put;
#else
	struct utmp ut, *put;
#endif

	if (tty == NULL) {
	    fail:
		safestrncpy(hbuf, ipc->ServInfo.fqdn, 24);
		return;
	}

	if (strncmp(tty, "/dev/", 5))
		goto fail;

	safestrncpy(ut.ut_line, &tty[5], sizeof ut.ut_line);

#ifdef HAVE_GETUTXLINE /* Solaris uses this */
	if ((put = getutxline(&ut)) == NULL)
#else
	if ((put = getutline(&ut)) == NULL)
#endif
		goto fail;

#if defined(HAVE_UT_TYPE) || defined(HAVE_GETUTXLINE)
	if (put->ut_type == USER_PROCESS) {
#endif
#if defined(HAVE_UT_HOST) || defined(HAVE_GETUTXLINE)
		if (*put->ut_host)
			safestrncpy(hbuf, put->ut_host, 24);
		else
#endif
			safestrncpy(hbuf, put->ut_line, 24);
#if defined(HAVE_UT_TYPE) || defined(HAVE_GETUTXLINE)
	}
	else goto fail;
#endif
#endif /* HAVE_UTMP_H */
}

/*
 * miscellaneous server commands (testing, etc.)
 */
void misc_server_cmd(CtdlIPC *ipc, char *cmd) {
	char buf[SIZ];

	CtdlIPC_chat_send(ipc, cmd);
	CtdlIPC_chat_recv(ipc, buf);
	scr_printf("%s\n",buf);
	if (buf[0]=='1') {
		set_keepalives(KA_HALF);
		while (CtdlIPC_chat_recv(ipc, buf), strcmp(buf,"000")) {
			scr_printf("%s\n",buf);
		}
		set_keepalives(KA_YES);
		return;
	}
	if (buf[0]=='4') {
		do {
			newprompt("> ",buf,255);
			CtdlIPC_chat_send(ipc, buf);
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
	char filename[SIZ];

	dp = opendir(dirname);
	if (dp == NULL) {
		return(errno);
	}

	while (d = readdir(dp), d != NULL) {
		snprintf(filename, sizeof filename, "%s/%s",
			dirname, d->d_name);
		unlink(filename);
	}

	closedir(dp);
	return(rmdir(dirname));
}
