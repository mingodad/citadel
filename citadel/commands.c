/*
 * $Id$
 *
 * This file contains functions which implement parts of the
 * text-mode user interface.
 *
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#else
#include <sgtty.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef THREADED_CLIENT
#include <pthread.h>
#endif

#include <signal.h>
#include <errno.h>
#include <stdarg.h>
#include "citadel.h"
#include "commands.h"
#include "messages.h"
#include "citadel_decls.h"
#include "routines.h"
#include "routines2.h"
#include "tools.h"
#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

struct citcmd {
	struct citcmd *next;
	int c_cmdnum;
	int c_axlevel;
	char c_keys[5][64];
};

#define IFNEXPERT if ((userflags&US_EXPERT)==0)


int rc_exp_beep;
char rc_exp_cmd[256];
int rc_allow_attachments;
int rc_display_message_numbers;
int rc_force_mail_prompts;
int rc_ansi_color;
int num_urls = 0;
char urls[MAXURLS][256];
char rc_url_cmd[256];

char *gl_string;
int next_lazy_cmd = 5;

struct citcmd *cmdlist = NULL;


/* these variables are local to this module */
char keepalives_enabled = KA_YES;	/* send NOOPs to server when idle */
int ok_to_interrupt = 0;		/* print express msgs asynchronously */
time_t AnsiDetect;			/* when did we send the detect code? */
int enable_color = 0;			/* nonzero for ANSI color */


/*
 * print_express()  -  print express messages if there are any
 */
void print_express(void)
{
	char buf[256];
	FILE *outpipe;

	if (express_msgs == 0)
		return;
	express_msgs = 0;
	serv_puts("PEXP");
	serv_gets(buf);
	if (buf[0] != '1')
		return;

	if (strlen(rc_exp_cmd) > 0) {
		outpipe = popen(rc_exp_cmd, "w");
		if (outpipe != NULL) {
			while (serv_gets(buf), strcmp(buf, "000")) {
				fprintf(outpipe, "%s\n", buf);
			}
			pclose(outpipe);
			return;
		}
	}
	/* fall back to built-in express message display */
	if (rc_exp_beep) {
		putc(7, stdout);
	}
	color(BRIGHT_RED);
	printf("\r---\n");
	while (serv_gets(buf), strcmp(buf, "000")) {
		printf("%s\n", buf);
	}
	printf("---\n");
	color(BRIGHT_WHITE);
}


void set_keepalives(int s)
{
	keepalives_enabled = (char) s;
}

/* 
 * This loop handles the "keepalive" messages sent to the server when idling.
 */

static time_t idlet = 0;
static void really_do_keepalive(void) {
	char buf[256];

	time(&idlet);
	if (keepalives_enabled != KA_NO) {
		serv_puts("NOOP");
		if (keepalives_enabled == KA_YES) {
			serv_gets(buf);
			if (buf[3] == '*') {
				express_msgs = 1;
				if (ok_to_interrupt == 1) {
					printf("\r%64s\r", "");
					print_express();
					printf("%s%c ", room_name,
					       room_prompt(room_flags));
					fflush(stdout);
				}
			}
		}
	}
}

/* threaded nonblocking keepalive stuff starts here. I'm going for a simple
   encapsulated interface; in theory there should be no need to touch these
   globals outside of the async_ka_* functions. */

#ifdef THREADED_CLIENT
static pthread_t ka_thr_handle;
static int ka_thr_active = 0;
static int async_ka_enabled = 0;

static void *ka_thread(void *arg)
{
	really_do_keepalive();
	pthread_detach(ka_thr_handle);
	ka_thr_active = 0;
	return NULL;
}

/* start up a thread to handle a keepalive in the background */
static void async_ka_exec(void)
{
	if (!ka_thr_active) {
		ka_thr_active = 1;
		if (pthread_create(&ka_thr_handle, NULL, ka_thread, NULL)) {
			perror("pthread_create");
			exit(1);
		}
	}
}
#endif /* THREADED_CLIENT */

static void do_keepalive(void)
{
	time_t now;

	time(&now);
	if ((now - idlet) < ((long) S_KEEPALIVE))
		return;

	/* Do a space-backspace to keep telnet sessions from idling out */
	printf(" %c", 8);
	fflush(stdout);

#ifdef THREADED_CLIENT
	if (async_ka_enabled)
		async_ka_exec();
	else
#endif
		really_do_keepalive();
}


/* Now the actual async-keepalve API that we expose to higher levels:
   async_ka_start() and async_ka_end(). These do nothing when we don't have
   threading enabled, so we avoid sprinkling ifdef's throughout the code. */

/* wait for a background keepalive to complete. this must be done before
   attempting any further server requests! */
void async_ka_end(void)
{
#ifdef THREADED_CLIENT
	if (ka_thr_active)
		pthread_join(ka_thr_handle, NULL);

	async_ka_enabled--;
#endif
}

/* tell do_keepalive() that keepalives are asynchronous. */
void async_ka_start(void)
{
#ifdef THREADED_CLIENT
	async_ka_enabled++;
#endif
}


int inkey(void)
{				/* get a character from the keyboard, with   */
	int a;			/* the watchdog timer in effect if necessary */
	fd_set rfds;
	struct timeval tv;
	time_t start_time, now;
	char inbuf[2];

	fflush(stdout);
	time(&start_time);

	do {

		/* This loop waits for keyboard input.  If the keepalive
		 * timer expires, it sends a keepalive to the server if
		 * necessary and then waits again.
		 */
		do {
			do_keepalive();

			FD_ZERO(&rfds);
			FD_SET(0, &rfds);
			tv.tv_sec = S_KEEPALIVE;
			tv.tv_usec = 0;

			time(&now);
			if (((now - start_time) > SLEEPING)
			    && (SLEEPING != 0) && (getppid() == 1)) {
				printf("Sleeping? Call again.\n");
				logoff(SIGALRM);
			}
			select(1, &rfds, NULL, NULL, &tv);
		} while (!FD_ISSET(0, &rfds));




		/* At this point, there's input, so fetch it.
		 * (There's a hole in the bucket...)
		 */
		read(0, inbuf, 1);
		a = inbuf[0];
		if (a == 127)
			a = 8;
		if (a > 126)
			a = 0;
		if (a == 10)
			a = 13;
		if (((a != 4) && (a != 13) && (a != 8) && (a != NEXT_KEY) && (a != STOP_KEY))
		    && ((a < 32) || (a > 126)))
			a = 0;
	} while (a == 0);
	return (a);
}


int yesno(void)
{				/* Returns 1 for yes, 0 for no */
	int a;
	while (1) {
		a = inkey();
		a = tolower(a);
		if (a == 'y') {
			printf("Yes\n");
			return (1);
		}
		if (a == 'n') {
			printf("No\n");
			return (0);
		}
	}
}

/* Returns 1 for yes, 0 for no, arg is default value */
int yesno_d(int d)
{
	int a;
	while (1) {
		a = inkey();
		a = tolower(a);
		if (a == 13)
			a = (d ? 'y' : 'n');
		if (a == 'y') {
			printf("Yes\n");
			return (1);
		}
		if (a == 'n') {
			printf("No\n");
			return (0);
		}
	}
}




/* Gets a line from the terminal */
/* string == Pointer to string buffer */
/* lim == Maximum length - if negative, no-show */
void getline(char *string, int lim) 
{
	int a, b;
	char flag = 0;

	if (lim < 0) {
		lim = (0 - lim);
		flag = 1;
	}
	strcpy(string, "");
	gl_string = string;
	async_ka_start();
      GLA:a = inkey();
	a = (a & 127);
	if ((a == 8) && (strlen(string) == 0))
		goto GLA;
	if ((a != 13) && (a != 8) && (strlen(string) == lim))
		goto GLA;
	if ((a == 8) && (string[0] != 0)) {
		string[strlen(string) - 1] = 0;
		putc(8, stdout);
		putc(32, stdout);
		putc(8, stdout);
		goto GLA;
	}
	if ((a == 13) || (a == 10)) {
		putc(13, stdout);
		putc(10, stdout);
		async_ka_end();
		return;
	}
	if (a < 32)
		a = '.';
	b = strlen(string);
	string[b] = a;
	string[b + 1] = 0;
	if (flag == 0)
		putc(a, stdout);
	if (flag == 1)
		putc('*', stdout);
	goto GLA;
}


/*
 * strprompt()  -  prompt for a string, print the existing value and
 *                 allow the user to press return to keep it...
 */
void strprompt(char *prompt, char *str, int len)
{
	char buf[128];
	print_express();
	color(DIM_WHITE);
	printf("%s ", prompt);
	color(DIM_MAGENTA);
	printf("[");
	color(BRIGHT_MAGENTA);
	printf("%s", str);
	color(DIM_MAGENTA);
	printf("]");
	color(DIM_WHITE);
	printf(": ");
	color(BRIGHT_CYAN);
	getline(buf, len);
	if (buf[0] != 0)
		strcpy(str, buf);
	color(DIM_WHITE);
}

/*
 * boolprompt()  -  prompt for a yes/no, print the existing value and
 *                  allow the user to press return to keep it...
 */
int boolprompt(char *prompt, int prev_val)
{
	int r;

	color(DIM_WHITE);
	printf("%s ", prompt);
	color(DIM_MAGENTA);
	printf(" [");
	color(BRIGHT_MAGENTA);
	printf("%s", (prev_val ? "Yes" : "No"));
	color(DIM_MAGENTA);
	printf("]: ");
	color(BRIGHT_CYAN);
	r = (yesno_d(prev_val));
	color(DIM_WHITE);
	return r;
}

/* 
 * intprompt()  -  like strprompt(), except for an integer
 *                 (note that it RETURNS the new value!)
 */
int intprompt(char *prompt, int ival, int imin, int imax)
{
	char buf[16];
	int i;
	int p;

	do {
		i = ival;
		snprintf(buf, sizeof buf, "%d", i);
		strprompt(prompt, buf, 15);
		i = atoi(buf);
		for (p=0; p<strlen(buf); ++p) {
			if (!isdigit(buf[p])) i = imin - 1;
		}
		if (i < imin)
			printf("*** Must be no less than %d.\n", imin);
		if (i > imax)
			printf("*** Must be no more than %d.\n", imax);
	} while ((i < imin) || (i > imax));
	return (i);
}

/* 
 * newprompt()  -  prompt for a string with no existing value
 *                 (clears out string buffer first)
 */
void newprompt(char *prompt, char *str, int len)
{
	color(BRIGHT_MAGENTA);
	printf("%s", prompt);
	color(DIM_MAGENTA);
	getline(str, len);
	color(DIM_WHITE);
}


int lkey(void)
{				/* returns a lower case value */
	int a;
	a = inkey();
	if (isupper(a))
		a = tolower(a);
	return (a);
}

/*
 * parse the citadel.rc file
 */
void load_command_set(void)
{
	FILE *ccfile;
	char buf[256];
	struct citcmd *cptr;
	struct citcmd *lastcmd = NULL;
	int a, d;
	int b = 0;


	/* first, set up some defaults for non-required variables */

	strcpy(editor_path, "");
	strcpy(printcmd, "");
	strcpy(rc_username, "");
	strcpy(rc_password, "");
	rc_floor_mode = 0;
	rc_exp_beep = 1;
	rc_allow_attachments = 0;
	strcpy(rc_exp_cmd, "");
	rc_display_message_numbers = 0;
	rc_force_mail_prompts = 0;
	rc_ansi_color = 0;
	strcpy(rc_url_cmd, "");

	/* now try to open the citadel.rc file */

	ccfile = NULL;
	if (getenv("HOME") != NULL) {
		snprintf(buf, sizeof buf, "%s/.citadelrc", getenv("HOME"));
		ccfile = fopen(buf, "r");
	}
	if (ccfile == NULL) {
		ccfile = fopen("/usr/local/lib/citadel.rc", "r");
	}
	if (ccfile == NULL) {
		snprintf(buf, sizeof buf, "%s/citadel.rc", BBSDIR);
		ccfile = fopen(buf, "r");
	}
	if (ccfile == NULL) {
		ccfile = fopen("./citadel.rc", "r");
	}
	if (ccfile == NULL) {
		perror("commands: cannot open citadel.rc");
		logoff(errno);
	}
	while (fgets(buf, 256, ccfile) != NULL) {
		while ((strlen(buf) > 0) ? (isspace(buf[strlen(buf) - 1])) : 0)
			buf[strlen(buf) - 1] = 0;

		if (!struncmp(buf, "editor=", 7))
			strcpy(editor_path, &buf[7]);

		if (!struncmp(buf, "printcmd=", 9))
			strcpy(printcmd, &buf[9]);

		if (!struncmp(buf, "expcmd=", 7))
			strcpy(rc_exp_cmd, &buf[7]);

		if (!struncmp(buf, "local_screen_dimensions=", 24))
			have_xterm = (char) atoi(&buf[24]);

		if (!struncmp(buf, "use_floors=", 11)) {
			if (!strucmp(&buf[11], "yes"))
				rc_floor_mode = RC_YES;
			if (!strucmp(&buf[11], "no"))
				rc_floor_mode = RC_NO;
			if (!strucmp(&buf[11], "default"))
				rc_floor_mode = RC_DEFAULT;
		}
		if (!struncmp(buf, "beep=", 5)) {
			rc_exp_beep = atoi(&buf[5]);
		}
		if (!struncmp(buf, "allow_attachments=", 18)) {
			rc_allow_attachments = atoi(&buf[18]);
		}
		if (!struncmp(buf, "display_message_numbers=", 24)) {
			rc_display_message_numbers = atoi(&buf[24]);
		}
		if (!struncmp(buf, "force_mail_prompts=", 19)) {
			rc_force_mail_prompts = atoi(&buf[19]);
		}
		if (!struncmp(buf, "ansi_color=", 11)) {
			if (!struncmp(&buf[11], "on", 2))
				rc_ansi_color = 1;
			if (!struncmp(&buf[11], "auto", 4))
				rc_ansi_color = 2;	/* autodetect */
			if (!struncmp(&buf[11], "user", 4))
				rc_ansi_color = 3;	/* user config */
		}
		if (!struncmp(buf, "username=", 9))
			strcpy(rc_username, &buf[9]);

		if (!struncmp(buf, "password=", 9))
			strcpy(rc_password, &buf[9]);

		if (!struncmp(buf, "urlcmd=", 7))
			strcpy(rc_url_cmd, &buf[7]);

		if (!struncmp(buf, "cmd=", 4)) {
			strcpy(buf, &buf[4]);

			cptr = (struct citcmd *) malloc(sizeof(struct citcmd));

			cptr->c_cmdnum = atoi(buf);
			for (d = strlen(buf); d >= 0; --d)
				if (buf[d] == ',')
					b = d;
			strcpy(buf, &buf[b + 1]);

			cptr->c_axlevel = atoi(buf);
			for (d = strlen(buf); d >= 0; --d)
				if (buf[d] == ',')
					b = d;
			strcpy(buf, &buf[b + 1]);

			for (a = 0; a < 5; ++a)
				cptr->c_keys[a][0] = 0;

			a = 0;
			b = 0;
			buf[strlen(buf) + 1] = 0;
			while (strlen(buf) > 0) {
				b = strlen(buf);
				for (d = strlen(buf); d >= 0; --d)
					if (buf[d] == ',')
						b = d;
				strncpy(cptr->c_keys[a], buf, b);
				cptr->c_keys[a][b] = 0;
				if (buf[b] == ',')
					strcpy(buf, &buf[b + 1]);
				else
					strcpy(buf, "");
				++a;
			}

			cptr->next = NULL;
			if (cmdlist == NULL)
				cmdlist = cptr;
			else
				lastcmd->next = cptr;
			lastcmd = cptr;
		}
	}
	fclose(ccfile);
}



/*
 * return the key associated with a command
 */
char keycmd(char *cmdstr)
{
	int a;

	for (a = 0; a < strlen(cmdstr); ++a)
		if (cmdstr[a] == '&')
			return (tolower(cmdstr[a + 1]));
	return (0);
}


/*
 * Output the string from a key command without the ampersand
 * "mode" should be set to 0 for normal or 1 for <C>ommand key highlighting
 */
char *cmd_expand(char *strbuf, int mode)
{
	int a;
	static char exp[64];
	char buf[256];

	strcpy(exp, strbuf);

	for (a = 0; a < strlen(exp); ++a) {
		if (strbuf[a] == '&') {

			if (mode == 0) {
				strcpy(&exp[a], &exp[a + 1]);
			}
			if (mode == 1) {
				exp[a] = '<';
				strcpy(buf, &exp[a + 2]);
				exp[a + 2] = '>';
				exp[a + 3] = 0;
				strcat(exp, buf);
			}
		}
		if (!strncmp(&exp[a], "^r", 2)) {
			strcpy(buf, exp);
			strcpy(&exp[a], room_name);
			strcat(exp, &buf[a + 2]);
		}
		if (!strncmp(&exp[a], "^c", 2)) {
			exp[a] = ',';
			strcpy(&exp[a + 1], &exp[a + 2]);
		}
	}

	return (exp);
}



/*
 * Comparison function to determine if entered commands match a
 * command loaded from the config file.
 */
int cmdmatch(char *cmdbuf, struct citcmd *cptr, int ncomp)
{
	int a;
	int cmdax;

	cmdax = 0;
	if (is_room_aide)
		cmdax = 1;
	if (axlevel >= 6)
		cmdax = 2;

	for (a = 0; a < ncomp; ++a) {
		if ((tolower(cmdbuf[a]) != keycmd(cptr->c_keys[a]))
		    || (cptr->c_axlevel > cmdax))
			return (0);
	}
	return (1);
}


/*
 * This function returns 1 if a given command requires a string input
 */
int requires_string(struct citcmd *cptr, int ncomp)
{
	int a;
	char buf[64];

	strcpy(buf, cptr->c_keys[ncomp - 1]);
	for (a = 0; a < strlen(buf); ++a) {
		if (buf[a] == ':')
			return (1);
	}
	return (0);
}


/*
 * Input a command at the main prompt.
 * This function returns an integer command number.  If the command prompts
 * for a string then it is placed in the supplied buffer.
 */
int getcmd(char *argbuf)
{
	char cmdbuf[5];
	int cmdspaces[5];
	int cmdpos;
	int ch;
	int a;
	int got;
	int this_lazy_cmd;
	struct citcmd *cptr;

	/* Switch color support on or off if we're in user mode */
	if (rc_ansi_color == 3) {
		if (userflags & US_COLOR)
			enable_color = 1;
		else
			enable_color = 0;
	}
	/* if we're running in idiot mode, display a cute little menu */
	IFNEXPERT formout("mainmenu");

	print_express();	/* print express messages if there are any */
	strcpy(argbuf, "");
	cmdpos = 0;
	for (a = 0; a < 5; ++a)
		cmdbuf[a] = 0;
	/* now the room prompt... */
	ok_to_interrupt = 1;
	color(BRIGHT_WHITE);
	printf("\n%s", room_name);
	color(DIM_WHITE);
	printf("%c ", room_prompt(room_flags));
	fflush(stdout);

	while (1) {
		ch = inkey();
		ok_to_interrupt = 0;

		/* Handle the backspace key, but only if there's something
		 * to backspace over...
		 */
		if ((ch == 8) && (cmdpos > 0)) {
			back(cmdspaces[cmdpos - 1] + 1);
			cmdbuf[cmdpos] = 0;
			--cmdpos;
		}
		/* Spacebar invokes "lazy traversal" commands */
		if ((ch == 32) && (cmdpos == 0)) {
			this_lazy_cmd = next_lazy_cmd;
			if (this_lazy_cmd == 13)
				next_lazy_cmd = 5;
			if (this_lazy_cmd == 5)
				next_lazy_cmd = 13;
			for (cptr = cmdlist; cptr != NULL; cptr = cptr->next) {
				if (cptr->c_cmdnum == this_lazy_cmd) {
					for (a = 0; a < 5; ++a)
						if (cptr->c_keys[a][0] != 0)
							printf("%s ", cmd_expand(
											cptr->c_keys[a], 0));
					printf("\n");
					return (this_lazy_cmd);
				}
			}
			printf("\n");
			return (this_lazy_cmd);
		}
		/* Otherwise, process the command */
		cmdbuf[cmdpos] = tolower(ch);

		for (cptr = cmdlist; cptr != NULL; cptr = cptr->next) {
			if (cmdmatch(cmdbuf, cptr, cmdpos + 1)) {

				printf("%s", cmd_expand(cptr->c_keys[cmdpos], 0));
				cmdspaces[cmdpos] = strlen(
				    cmd_expand(cptr->c_keys[cmdpos], 0));
				if (cmdpos < 4)
					if ((cptr->c_keys[cmdpos + 1]) != 0)
						putc(' ', stdout);
				++cmdpos;
			}
		}

		for (cptr = cmdlist; cptr != NULL; cptr = cptr->next) {
			if (cmdmatch(cmdbuf, cptr, 5)) {
				/* We've found our command. */
				if (requires_string(cptr, cmdpos)) {
					getline(argbuf, 32);
				} else {
					printf("\n");
				}

				/* If this command is one that changes rooms,
				 * then the next lazy-command (space bar)
				 * should be "read new" instead of "goto"
				 */
				if ((cptr->c_cmdnum == 5)
				    || (cptr->c_cmdnum == 6)
				    || (cptr->c_cmdnum == 47)
				    || (cptr->c_cmdnum == 52)
				    || (cptr->c_cmdnum == 16)
				    || (cptr->c_cmdnum == 20))
					next_lazy_cmd = 13;

				return (cptr->c_cmdnum);

			}
		}

		if (ch == '?') {
			printf("\rOne of ...                         \n");
			for (cptr = cmdlist; cptr != NULL; cptr = cptr->next) {
				if (cmdmatch(cmdbuf, cptr, cmdpos)) {
					for (a = 0; a < 5; ++a) {
						printf("%s ", cmd_expand(cptr->c_keys[a], 1));
					}
					printf("\n");
				}
			}

			printf("\n%s%c ", room_name, room_prompt(room_flags));
			got = 0;
			for (cptr = cmdlist; cptr != NULL; cptr = cptr->next) {
				if ((got == 0) && (cmdmatch(cmdbuf, cptr, cmdpos))) {
					for (a = 0; a < cmdpos; ++a) {
						printf("%s ",
						       cmd_expand(cptr->c_keys[a], 0));
					}
					got = 1;
				}
			}
		}
	}

}





/*
 * set tty modes.  commands are:
 * 
 * 0 - set to bbs mode, intr/quit disabled
 * 1 - set to bbs mode, intr/quit enabled
 * 2 - save current settings for later restoral
 * 3 - restore saved settings
 */
#ifdef HAVE_TERMIOS_H
void sttybbs(int cmd)
{				/* SysV version of sttybbs() */
	struct termios live;
	static struct termios saved_settings;
	static int last_cmd = 0;

	if (cmd == SB_LAST)
		cmd = last_cmd;
	else
		last_cmd = cmd;

	if ((cmd == 0) || (cmd == 1)) {
		tcgetattr(0, &live);
		live.c_iflag = ISTRIP | IXON | IXANY;
		live.c_oflag = OPOST | ONLCR;
		live.c_lflag = ISIG | NOFLSH;

		if (cmd == SB_YES_INTR) {
			live.c_cc[VINTR] = NEXT_KEY;
			live.c_cc[VQUIT] = STOP_KEY;
			signal(SIGINT, *sighandler);
			signal(SIGQUIT, *sighandler);
		} else {
			signal(SIGINT, SIG_IGN);
			signal(SIGQUIT, SIG_IGN);
			live.c_cc[VINTR] = (-1);
			live.c_cc[VQUIT] = (-1);
		}

		/* do we even need this stuff anymore? */
		/* live.c_line=0; */
		live.c_cc[VERASE] = 8;
		live.c_cc[VKILL] = 24;
		live.c_cc[VEOF] = 1;
		live.c_cc[VEOL] = 255;
		live.c_cc[VEOL2] = 0;
		live.c_cc[VSTART] = 0;
		tcsetattr(0, TCSADRAIN, &live);
	}
	if (cmd == 2) {
		tcgetattr(0, &saved_settings);
	}
	if (cmd == 3) {
		tcsetattr(0, TCSADRAIN, &saved_settings);
	}
}
#else
void sttybbs(int cmd)
{				/* BSD version of sttybbs() */
	struct sgttyb live;
	static struct sgttyb saved_settings;

	if ((cmd == 0) || (cmd == 1)) {
		gtty(0, &live);
		live.sg_flags |= CBREAK;
		live.sg_flags |= CRMOD;
		live.sg_flags |= NL1;
		live.sg_flags &= ~ECHO;
		if (cmd == 1)
			live.sg_flags |= NOFLSH;
		stty(0, &live);
	}
	if (cmd == 2) {
		gtty(0, &saved_settings);
	}
	if (cmd == 3) {
		stty(0, &saved_settings);
	}
}
#endif


/*
 * display_help()  -  help file viewer
 */
void display_help(char *name)
{
	formout(name);
}


/*
 * fmout()  -  Citadel text formatter and paginator
 */
int fmout(int width, FILE * fp, char pagin, int height, int starting_lp, char subst)
			/* screen width to use */
			/* file to read from, or NULL to read from server */
			/* nonzero if we should use the paginator */
			/* screen height to use */
			/* starting value for lines_printed, -1 for global */
			/* nonzero if we should use hypertext mode */
{
	int a, b, c, d, old;
	int real = (-1);
	char aaa[140];
	char buffer[512];
	int eof_flag = 0;

	num_urls = 0;	/* Start with a clean slate of embedded URL's */

	if (starting_lp >= 0) {
		lines_printed = starting_lp;
	}
	strcpy(aaa, "");
	old = 255;
	strcpy(buffer, "");
	c = 1;			/* c is the current pos */

	sigcaught = 0;
	sttybbs(1);

FMTA:	while ((eof_flag == 0) && (strlen(buffer) < 126)) {
	
		if (sigcaught)
			goto OOPS;
		if (fp != NULL) {	/* read from file */
			if (feof(fp))
				eof_flag = 1;
			if (eof_flag == 0) {
				a = getc(fp);
				buffer[strlen(buffer) + 1] = 0;
				buffer[strlen(buffer)] = a;
			}
		} else {	/* read from server */
			d = strlen(buffer);
			serv_gets(&buffer[d]);
			while ((!isspace(buffer[d])) && (isspace(buffer[strlen(buffer) - 1])))
				buffer[strlen(buffer) - 1] = 0;
			if (!strcmp(&buffer[d], "000")) {
				buffer[d] = 0;
				eof_flag = 1;
				while (isspace(buffer[strlen(buffer) - 1]))
					buffer[strlen(buffer) - 1] = 0;
			}
			d = strlen(buffer);
			buffer[d] = 10;
			buffer[d + 1] = 0;
		}
	}

	if ( (!struncmp(buffer, "http://", 7))
	   || (!struncmp(buffer, "ftp://", 6)) ) {
		safestrncpy(urls[num_urls], buffer, 255);
		for (a=0; a<strlen(urls[num_urls]); ++a) {
			b = urls[num_urls][a];
			if ( (b==' ') || (b==')') || (b=='>') || (b==10)
			   || (b==13) || (b==9) || (b=='\"') )
				urls[num_urls][a] = 0;
		}
		++num_urls;
	}

	buffer[strlen(buffer) + 1] = 0;
	a = buffer[0];
	strcpy(buffer, &buffer[1]);

	old = real;
	real = a;
	if (a <= 0)
		goto FMTEND;

	if (((a == 13) || (a == 10)) && (old != 13) && (old != 10))
		a = 32;
	if (((old == 13) || (old == 10)) && (isspace(real))) {
		printf("\n");
		++lines_printed;
		lines_printed = checkpagin(lines_printed, pagin, height);
		c = 1;
	}
	if (a > 126)
		goto FMTA;

	if (a > 32) {
		if (((strlen(aaa) + c) > (width - 5)) && (strlen(aaa) > (width - 5))) {
			printf("\n%s", aaa);
			c = strlen(aaa);
			aaa[0] = 0;
			++lines_printed;
			lines_printed = checkpagin(lines_printed, pagin, height);
		}
		b = strlen(aaa);
		aaa[b] = a;
		aaa[b + 1] = 0;
	}
	if (a == 32) {
		if ((strlen(aaa) + c) > (width - 5)) {
			c = 1;
			printf("\n");
			++lines_printed;
			lines_printed = checkpagin(lines_printed, pagin, height);
		}
		printf("%s ", aaa);
		++c;
		c = c + strlen(aaa);
		strcpy(aaa, "");
		goto FMTA;
	}
	if ((a == 13) || (a == 10)) {
		printf("%s\n", aaa);
		c = 1;
		++lines_printed;
		lines_printed = checkpagin(lines_printed, pagin, height);
		strcpy(aaa, "");
		goto FMTA;
	}
	goto FMTA;

	/* signal caught; drain the server */
      OOPS:do {
		serv_gets(aaa);
	} while (strcmp(aaa, "000"));

      FMTEND:printf("\n");
	++lines_printed;
	lines_printed = checkpagin(lines_printed, pagin, height);
	return (sigcaught);
}


/*
 * support ANSI color if defined
 */
void color(int colornum)
{
	static int is_bold = 0;
	static int hold_color, current_color;

	if (colornum == COLOR_PUSH) {
		hold_color = current_color;
		return;
	}

	if (colornum == COLOR_POP) {
		color(hold_color);
		return;
	}

	current_color = colornum;
	if (enable_color) {
		/* Don't switch to black or white explicitly as this confuses
		 * black-on-white terminals. Instead, output the "original
		 * pair" sequence.
		 */
		if ((colornum & 7) == DIM_WHITE || (colornum & 7) == DIM_BLACK)
			printf("\033[39;49m");
		else
			printf("\033[3%dm", (colornum & 7));

		if ((colornum >= 8) && (is_bold == 0)) {
			printf("\033[1m");
			is_bold = 1;
		} else if ((colornum < 8) && (is_bold == 1)) {
			printf("\033[0m");
			is_bold = 0;
		}
		fflush(stdout);
	}
}

void cls(int colornum)
{
	if (enable_color) {
		printf("\033[4%dm\033[2J\033[H\033[0m", colornum);
		fflush(stdout);
	}
}


/*
 * Detect whether ANSI color is available (answerback)
 */
void send_ansi_detect(void)
{
	if (rc_ansi_color == 2) {
		printf("\033[c");
		fflush(stdout);
		time(&AnsiDetect);
	}
}

void look_for_ansi(void)
{
	fd_set rfds;
	struct timeval tv;
	char abuf[512];
	time_t now;
	int a;

	if (rc_ansi_color == 0) {
		enable_color = 0;
	} else if (rc_ansi_color == 1) {
		enable_color = 1;
	} else if (rc_ansi_color == 2) {

		/* otherwise, do the auto-detect */

		strcpy(abuf, "");

		time(&now);
		if ((now - AnsiDetect) < 2)
			sleep(1);

		do {
			FD_ZERO(&rfds);
			FD_SET(0, &rfds);
			tv.tv_sec = 0;
			tv.tv_usec = 1;

			select(1, &rfds, NULL, NULL, &tv);
			if (FD_ISSET(0, &rfds)) {
				abuf[strlen(abuf) + 1] = 0;
				read(0, &abuf[strlen(abuf)], 1);
			}
		} while (FD_ISSET(0, &rfds));

		for (a = 0; a < strlen(abuf); ++a) {
			if ((abuf[a] == 27) && (abuf[a + 1] == '[')
			    && (abuf[a + 2] == '?')) {
				enable_color = 1;
			}
		}
	}
}


/*
 * Display key options (highlight hotkeys inside angle brackets)
 */
void keyopt(char *buf) {
	int i;

	color(DIM_WHITE);
	for (i=0; i<strlen(buf); ++i) {
		if (buf[i]=='<') {
			putc(buf[i], stdout);
			color(BRIGHT_MAGENTA);
		} else {
			if (buf[i]=='>') {
				color(DIM_WHITE);
			}
			putc(buf[i], stdout);
		}
	}
	color(DIM_WHITE);
}



/*
 * Present a key-menu line choice type of thing
 */
char keymenu(char *menuprompt, char *menustring) {
	int i, c, a;
	int choices;
	int do_prompt = 0;
	char buf[256];
	int ch;
	int display_prompt = 1;

	choices = num_tokens(menustring, '|');

	if (menuprompt != NULL) do_prompt = 1;
	if (menuprompt != NULL) if (strlen(menuprompt)==0) do_prompt = 0;

	while (1) {
		if (display_prompt) {
			if (do_prompt) {
				printf("%s ", menuprompt);
			} 
			else {
				for (i=0; i<choices; ++i) {
					extract(buf, menustring, i);
					keyopt(buf);
					printf(" ");
				}
			}
			printf(" -> ");
			display_prompt = 0;
		}
		ch = lkey();
	
		if ( (do_prompt) && (ch=='?') ) {
			printf("\rOne of...                               ");
			printf("                                      \n");
			for (i=0; i<choices; ++i) {
				extract(buf, menustring, i);
				printf("   ");
				keyopt(buf);
				printf("\n");
			}
			printf("\n");
			display_prompt = 1;
		}

		for (i=0; i<choices; ++i) {
			extract(buf, menustring, i);
			for (c=1; c<strlen(buf); ++c) {
				if ( (ch == tolower(buf[c]))
				   && (buf[c-1]=='<')
				   && (buf[c+1]=='>') ) {
					for (a=0; a<strlen(buf); ++a) {
						if ( (a!=(c-1)) && (a!=(c+1))) {
							putc(buf[a], stdout);
						}
					}
					printf("\n\n");
					return ch;
				}
			}
		}
	}
}
