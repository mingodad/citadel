/*
 * $Id$
 *
 * Citadel setup utility
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <signal.h>
#include <netdb.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <time.h>

#include "citadel.h"
#include "axdefs.h"
#include "sysdep.h"
#include "config.h"
#include "tools.h"
#include "citadel_dirs.h"

#ifdef HAVE_NEWT
#include <newt.h>
#endif


#define MAXSETUP 4	/* How many setup questions to ask */

#define UI_TEXT		0	/* Default setup type -- text only */
#define UI_DIALOG	2	/* Use the 'dialog' program */
#define UI_SILENT	3	/* Silent running, for use in scripts */
#define UI_NEWT		4	/* Use the "newt" window library */

#define SERVICE_NAME	"citadel"
#define PROTO_NAME	"tcp"
#define NSSCONF		"/etc/nsswitch.conf"

int setup_type;
char setup_directory[SIZ];
char citserver_init_entry[SIZ];
int using_web_installer = 0;
int enable_home = 1;

/**
 *	We will set have_sysv_init to nonzero if the host operating system
 *	has a System V style init driven by /etc/inittab.  This will cause
 *	setup to offer to automatically start and stop the Citadel service.
 */
int have_sysv_init = 0;

#ifdef HAVE_LDAP
void contemplate_ldap(void);
#endif

char *setup_titles[] =
{
	"Citadel Home Directory",
	"System Administrator",
	"Citadel User ID",
	"Server IP address",
	"Server port number",
};


struct config config;

	/* calculate all our path on a central place */
    /* where to keep our config */
	

char *setup_text[] = {
#ifndef HAVE_RUN_DIR
"Enter the full pathname of the directory in which the Citadel\n"
"installation you are creating or updating resides.  If you\n"
"specify a directory other than the default, you will need to\n"
"specify the -h flag to the server when you start it up.\n",
#else
"Enter the subdirectory name for an alternate installation of "
"Citadel. To do a default installation just leave it blank."
"If you specify a directory other than the default, you will need to\n"
"specify the -h flag to the server when you start it up.\n"
"note that it may not have a leading /",
#endif

"Enter the name of the system administrator (which is probably\n"
"you).  When an account is created with this name, it will\n"
"automatically be given administrator-level access.\n",

"Citadel needs to run under its own user ID.  This would\n"
"typically be called \"citadel\", but if you are running Citadel\n"
"as a public BBS, you might also call it \"bbs\" or \"guest\".\n"
"The server will run under this user ID.  Please specify that\n"
"user ID here.  You may specify either a user name or a numeric\n"
"UID.\n",

"Specify the IP address on which your server will run.  If you\n"
"leave this blank, or if you specify 0.0.0.0, Citadel will listen\n"
"on all addresses.  You can usually skip this unless you are\n"
"running multiple instances of Citadel on the same computer.\n",

"Specify the TCP port number on which your server will run.\n"
"Normally, this will be port 504, which is the official port\n"
"assigned by the IANA for Citadel servers.  You will only need\n"
"to specify a different port number if you run multiple instances\n"
"of Citadel on the same computer and there is something else\n"
"already using port 504.\n",

};

struct config config;
int direction;

/*
 * Set an entry in inittab to the desired state
 */
void set_init_entry(char *which_entry, char *new_state) {
	char *inittab = NULL;
	FILE *fp;
	char buf[SIZ];
	char entry[SIZ];
	char levels[SIZ];
	char state[SIZ];
	char prog[SIZ];

	if (which_entry == NULL) return;
	if (strlen(which_entry) == 0) return;

	inittab = strdup("");
	if (inittab == NULL) return;

	fp = fopen("/etc/inittab", "r");
	if (fp == NULL) return;

	while(fgets(buf, sizeof buf, fp) != NULL) {

		if (num_tokens(buf, ':') == 4) {
			extract_token(entry, buf, 0, ':', sizeof entry);
			extract_token(levels, buf, 1, ':', sizeof levels);
			extract_token(state, buf, 2, ':', sizeof state);
			extract_token(prog, buf, 3, ':', sizeof prog); /* includes 0x0a LF */

			if (!strcmp(entry, which_entry)) {
				strcpy(state, new_state);
				sprintf(buf, "%s:%s:%s:%s",
					entry, levels, state, prog);
			}
		}

		inittab = realloc(inittab, strlen(inittab) + strlen(buf) + 2);
		if (inittab == NULL) {
			fclose(fp);
			return;
		}
		
		strcat(inittab, buf);
	}
	fclose(fp);
	fp = fopen("/etc/inittab", "w");
	if (fp != NULL) {
		fwrite(inittab, strlen(inittab), 1, fp);
		fclose(fp);
		kill(1, SIGHUP);	/* Tell init to re-read /etc/inittab */
	}
	free(inittab);
}


/*
 * Locate the name of an inittab entry for a specific program
 */
void locate_init_entry(char *init_entry, char *looking_for) {

	FILE *infp;
	char buf[SIZ];
	int have_entry = 0;
	char entry[SIZ];
	char prog[SIZ];

	strcpy(init_entry, "");

	/* Pound through /etc/inittab line by line.  Set have_entry to 1 if
	 * an entry is found which we believe starts the specified program.
	 */
	infp = fopen("/etc/inittab", "r");
	if (infp == NULL) {
		return;
	} else {
		while (fgets(buf, sizeof buf, infp) != NULL) {
			buf[strlen(buf) - 1] = 0;
			extract_token(entry, buf, 0, ':', sizeof entry);
			extract_token(prog, buf, 3, ':', sizeof prog);
			if (!strncasecmp(prog, looking_for,
			   strlen(looking_for))) {
				++have_entry;
				strcpy(init_entry, entry);
			}
		}
		fclose(infp);
	}

}


/* 
 * Shut down the Citadel service if necessary, during setup.
 */
void shutdown_citserver(void) {
	char looking_for[SIZ];

	snprintf(looking_for, 
			 sizeof looking_for,
			 "%s/citserver", 
#ifndef HAVE_RUN_DIR
			 setup_directory
#else
			 CTDLDIR
#endif
			 );
	locate_init_entry(citserver_init_entry, looking_for);
	if (strlen(citserver_init_entry) > 0) {
		set_init_entry(citserver_init_entry, "off");
	}
}


/*
 * Start the Citadel service.
 */
void start_citserver(void) {
	if (strlen(citserver_init_entry) > 0) {
		set_init_entry(citserver_init_entry, "respawn");
	}
}



void cleanup(int exitcode)
{
#ifdef HAVE_NEWT
	newtCls();
	newtRefresh();
	newtFinished();
#endif
	exit(exitcode);
}



void title(char *text)
{
	if (setup_type == UI_TEXT) {
		printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n<%s>\n", text);
	}
}



int yesno(char *question)
{
#ifdef HAVE_NEWT
	newtComponent form = NULL;
	newtComponent yesbutton = NULL;
	newtComponent nobutton = NULL;
	int prompt_window_height = 0;
#endif
	int i = 0;
	int answer = 0;
	char buf[SIZ];

	switch (setup_type) {

	case UI_TEXT:
		do {
			printf("%s\nYes/No --> ", question);
			fgets(buf, sizeof buf, stdin);
			answer = tolower(buf[0]);
			if (answer == 'y')
				answer = 1;
			else if (answer == 'n')
				answer = 0;
		} while ((answer < 0) || (answer > 1));
		break;

	case UI_DIALOG:
		sprintf(buf, "exec %s --yesno '%s' 10 72",
			getenv("CTDL_DIALOG"),
			question);
		i = system(buf);
		if (i == 0) {
			answer = 1;
		}
		else {
			answer = 0;
		}
		break;

#ifdef HAVE_NEWT
	case UI_NEWT:
		prompt_window_height = num_tokens(question, '\n') + 5;
		newtCenteredWindow(76, prompt_window_height, "Question");
		form = newtForm(NULL, NULL, 0);
		for (i=0; i<num_tokens(question, '\n'); ++i) {
			extract_token(buf, question, i, '\n', sizeof buf);
			newtFormAddComponent(form, newtLabel(1, 1+i, buf));
		}
		yesbutton = newtButton(10, (prompt_window_height - 4), "Yes");
		nobutton = newtButton(60, (prompt_window_height - 4), "No");
		newtFormAddComponent(form, yesbutton);
		newtFormAddComponent(form, nobutton);
		if (newtRunForm(form) == yesbutton) {
			answer = 1;
		}
		else {
			answer = 0;
		}
		newtPopWindow();
		newtFormDestroy(form);	

		break;
#endif

	}
	return (answer);
}


void important_message(char *title, char *msgtext)
{
#ifdef HAVE_NEWT
	newtComponent form = NULL;
	int i = 0;
#endif
	char buf[SIZ];

	switch (setup_type) {

	case UI_TEXT:
		printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
		printf("       %s \n\n%s\n\n", title, msgtext);
		printf("Press return to continue...");
		fgets(buf, sizeof buf, stdin);
		break;

	case UI_DIALOG:
		sprintf(buf, "exec %s --msgbox '%s' 19 72",
			getenv("CTDL_DIALOG"),
			msgtext);
		system(buf);
		break;

#ifdef HAVE_NEWT
	case UI_NEWT:
		newtCenteredWindow(76, 10, title);
		form = newtForm(NULL, NULL, 0);
		for (i=0; i<num_tokens(msgtext, '\n'); ++i) {
			extract_token(buf, msgtext, i, '\n', sizeof buf);
			newtFormAddComponent(form, newtLabel(1, 1+i, buf));
		}
		newtFormAddComponent(form, newtButton(35, 5, "OK"));
		newtRunForm(form);
		newtPopWindow();
		newtFormDestroy(form);	
		break;
#endif

	}
}

void important_msgnum(int msgnum)
{
	important_message("Important Message", setup_text[msgnum]);
}

void display_error(char *error_message)
{
	important_message("Error", error_message);
}

void progress(char *text, long int curr, long int cmax)
{
#ifdef HAVE_NEWT

	/* These variables are static because progress() gets called
	 * multiple times during the course of whatever operation is
	 * being performed.  This makes setup non-threadsafe, but who
	 * cares?
	 */
	static newtComponent form = NULL;
	static newtComponent scale = NULL;
#endif
	static long dots_printed = 0L;
	long a = 0;
	static FILE *fp = NULL;
	char buf[SIZ];

	switch (setup_type) {

	case UI_TEXT:
		if (curr == 0) {
			printf("%s\n", text);
			printf("..........................");
			printf("..........................");
			printf("..........................\r");
			fflush(stdout);
			dots_printed = 0;
		} else if (curr == cmax) {
			printf("\r%79s\n", "");
		} else {
			a = (curr * 100) / cmax;
			a = a * 78;
			a = a / 100;
			while (dots_printed < a) {
				printf("*");
				++dots_printed;
				fflush(stdout);
			}
		}
		break;

	case UI_DIALOG:
		if (curr == 0) {
			sprintf(buf, "exec %s --gauge '%s' 7 72 0",
				getenv("CTDL_DIALOG"),
				text);
			fp = popen(buf, "w");
			if (fp != NULL) {
				fprintf(fp, "0\n");
				fflush(fp);
			}
		} 
		else if (curr == cmax) {
			if (fp != NULL) {
				fprintf(fp, "100\n");
				pclose(fp);
				fp = NULL;
			}
		}
		else {
			a = (curr * 100) / cmax;
			if (fp != NULL) {
				fprintf(fp, "%ld\n", a);
				fflush(fp);
			}
		}
		break;

#ifdef HAVE_NEWT
	case UI_NEWT:
		if (curr == 0) {
			newtCenteredWindow(76, 8, text);
			form = newtForm(NULL, NULL, 0);
			scale = newtScale(1, 3, 74, cmax);
			newtFormAddComponent(form, scale);
			newtDrawForm(form);
			newtRefresh();
		}
		if ((curr > 0) && (curr <= cmax)) {
			newtScaleSet(scale, curr);
			newtRefresh();
		}
		if (curr == cmax) {
			newtFormDestroy(form);	
			newtPopWindow();
			newtRefresh();
		}
		break;
#endif

	}
}



/*
 * check_services_entry()  -- Make sure "citadel" is in /etc/services
 *
 */
void check_services_entry(void)
{
	int i;
	FILE *sfp;

	if (getservbyname(SERVICE_NAME, PROTO_NAME) == NULL) {
		for (i=0; i<=3; ++i) {
			progress("Adding service entry...", i, 3);
			if (i == 0) {
				sfp = fopen("/etc/services", "a");
				if (sfp == NULL) {
					display_error(strerror(errno));
				} else {
					fprintf(sfp, "%s		504/tcp\n",
						SERVICE_NAME);
					fclose(sfp);
				}
			}
			sleep(1);
		}
	}
}


/*
 * Generate a unique entry name for a new inittab entry
 */
void generate_entry_name(char *entryname) {
	char buf[SIZ];

	snprintf(entryname, sizeof entryname, "c0");
	do {
		++entryname[1];
		if (entryname[1] > '9') {
			entryname[1] = 0;
			++entryname[0];
			if (entryname[0] > 'z') {
				display_error(
				   "Can't generate a unique entry name");
				return;
			}
		}
		snprintf(buf, sizeof buf,
		     "grep %s: /etc/inittab >/dev/null 2>&1", entryname);
	} while (system(buf) == 0);
}



/*
 * check_inittab_entry()  -- Make sure "citadel" is in /etc/inittab
 *
 */
void check_inittab_entry(void)
{
	FILE *infp;
	char looking_for[SIZ];
	char question[SIZ];
	char entryname[5];

	/* Determine the fully qualified path name of citserver */
	snprintf(looking_for, 
			 sizeof looking_for,
			 "%s/citserver", 
#ifndef HAVE_RUN_DIR
			 setup_directory
#else
			 CTDLDIR
#endif
			 );
	locate_init_entry(citserver_init_entry, looking_for);

	/* If there's already an entry, then we have nothing left to do. */
	if (strlen(citserver_init_entry) > 0) {
		return;
	}

	/* Otherwise, prompt the user to create an entry. */
	if (getenv("CREATE_INITTAB_ENTRY") != NULL) {
		if (strcasecmp(getenv("CREATE_INITTAB_ENTRY"), "yes")) {
			return;
		}
	}
	else {
		snprintf(question, sizeof question,
			"Do you want this computer configured to start the Citadel\n"
			"service automatically?  (If you answer yes, an entry in\n"
			"/etc/inittab pointing to %s will be added.)\n",
			looking_for);
		if (yesno(question) == 0) {
			return;
		}
	}

	/* Generate a unique entry name for /etc/inittab */
	generate_entry_name(entryname);

	/* Now write it out to /etc/inittab */
	infp = fopen("/etc/inittab", "a");
	if (infp == NULL) {
		display_error(strerror(errno));
	} else {
		fprintf(infp, "# Start the Citadel server...\n");
		fprintf(infp, "%s:2345:respawn:%s %s%s -llocal4\n",
				entryname, 
				looking_for, 
				(enable_home)?"-h":"", 
				(enable_home)?setup_directory:"");
		fclose(infp);
		strcpy(citserver_init_entry, entryname);
	}
}


/*
 * On systems which use xinetd, see if we can offer to install Citadel as
 * the default telnet target.
 */
void check_xinetd_entry(void) {
	char *filename = "/etc/xinetd.d/telnet";
	FILE *fp;
	char buf[SIZ];
	int already_citadel = 0;

	fp = fopen(filename, "r+");
	if (fp == NULL) return;		/* Not there.  Oh well... */

	while (fgets(buf, sizeof buf, fp) != NULL) {
		if (strstr(buf, setup_directory) != NULL) already_citadel = 1;
	}
	fclose(fp);
	if (already_citadel) return;	/* Already set up this way. */

	/* Otherwise, prompt the user to create an entry. */
	if (getenv("CREATE_XINETD_ENTRY") != NULL) {
		if (strcasecmp(getenv("CREATE_XINETD_ENTRY"), "yes")) {
			return;
		}
	}
	else {
		snprintf(buf, sizeof buf,
			"Setup can configure the \"xinetd\" service to automatically\n"
			"connect incoming telnet sessions to Citadel, bypassing the\n"
			"host system login: prompt.  Would you like to do this?\n"
		);
		if (yesno(buf) == 0) {
			return;
		}
	}

	fp = fopen(filename, "w");
	fprintf(fp,
		"# description: telnet service for Citadel users\n"
		"service telnet\n"
		"{\n"
		"	disable	= no\n"
		"	flags		= REUSE\n"
		"	socket_type	= stream\n"
		"	wait		= no\n"
		"	user		= root\n"
		"	server		= /usr/sbin/in.telnetd\n"
		"	server_args	= -h -L %s/citadel\n"
		"	log_on_failure	+= USERID\n"
		"}\n",
#ifndef HAVE_RUN_DIR
			setup_directory
#else
			RUN_DIR
#endif
			);
	fclose(fp);

	/* Now try to restart the service */
	system("/etc/init.d/xinetd restart >/dev/null 2>&1");
}



/*
 * Offer to disable other MTA's
 */
void disable_other_mta(char *mta) {
	char buf[SIZ];
	FILE *fp;
	int lines = 0;

	sprintf(buf, "/bin/ls -l /etc/rc*.d/S*%s 2>/dev/null; "
		"/bin/ls -l /etc/rc.d/rc*.d/S*%s 2>/dev/null",
		mta, mta);
	fp = popen(buf, "r");
	if (fp == NULL) return;

	while (fgets(buf, sizeof buf, fp) != NULL) {
		++lines;
	}
	fclose(fp);
	if (lines == 0) return;		/* Nothing to do. */


	/* Offer to replace other MTA with the vastly superior Citadel :)  */

	if (getenv("ACT_AS_MTA")) {
		if (strcasecmp(getenv("ACT_AS_MTA"), "yes")) {
			return;
		}
	}
	else {
		snprintf(buf, sizeof buf,
			"You appear to have the \"%s\" email program\n"
			"running on your system.  If you want Citadel mail\n"
			"connected with %s, you will have to manually integrate\n"
			"them.  It is preferable to disable %s, and use Citadel's\n"
			"SMTP, POP3, and IMAP services.\n\n"
			"May we disable %s so that Citadel has access to ports\n"
			"25, 110, and 143?\n",
			mta, mta, mta, mta
		);
		if (yesno(buf) == 0) {
			return;
		}
	}

	sprintf(buf, "for x in /etc/rc*.d/S*%s; do mv $x `echo $x |sed s/S/K/g`; done >/dev/null 2>&1", mta);
	system(buf);
	sprintf(buf, "/etc/init.d/%s stop >/dev/null 2>&1", mta);
	system(buf);
}




/* 
 * Check to see if our server really works.  Returns 0 on success.
 */
int test_server(void) {
	char cmd[256];
	char cookie[256];
	FILE *fp;
	char buf[4096];
	int found_it = 0;

	/* Generate a silly little cookie.  We're going to write it out
	 * to the server and try to get it back.  The cookie does not
	 * have to be secret ... just unique.
	 */
	sprintf(cookie, "--test--%d--", getpid());

	sprintf(cmd, "%s/sendcommand %s%s ECHO %s 2>&1",
#ifndef HAVE_RUN_DIR
			setup_directory,
#else
			CTDLDIR,
#endif
			(enable_home)?"-h":"", 
			(enable_home)?setup_directory:"",
			cookie);

	fp = popen(cmd, "r");
	if (fp == NULL) return(errno);

	while (fgets(buf, sizeof buf, fp) != NULL) {
		if ( (buf[0]=='2')
		   && (strstr(buf, cookie) != NULL) ) {
			++found_it;
		}
	}
	pclose(fp);

	if (found_it) {
		return(0);
	}
	return(-1);
}

void strprompt(char *prompt_title, char *prompt_text, char *str)
{
#ifdef HAVE_NEWT
	newtComponent form;
	char *result;
	int i;
	int prompt_window_height = 0;
#endif
	char buf[SIZ];
	char setupmsg[SIZ];
	char dialog_result[PATH_MAX];
	FILE *fp = NULL;

	strcpy(setupmsg, "");

	switch (setup_type) {
	case UI_TEXT:
		title(prompt_title);
		printf("\n%s\n", prompt_text);
		printf("This is currently set to:\n%s\n", str);
		printf("Enter new value or press return to leave unchanged:\n");
		fgets(buf, sizeof buf, stdin);
		buf[strlen(buf) - 1] = 0;
		if (strlen(buf) != 0)
			strcpy(str, buf);
		break;

	case UI_DIALOG:
		CtdlMakeTempFileName(dialog_result, sizeof dialog_result);
		sprintf(buf, "exec %s --inputbox '%s' 19 72 '%s' 2>%s",
			getenv("CTDL_DIALOG"),
			prompt_text,
			str,
			dialog_result);
		system(buf);
		fp = fopen(dialog_result, "r");
		if (fp != NULL) {
			fgets(str, sizeof buf, fp);
			if (str[strlen(str)-1] == 10) {
				str[strlen(str)-1] = 0;
			}
			fclose(fp);
			unlink(dialog_result);
		}
		break;

#ifdef HAVE_NEWT
	case UI_NEWT:

		prompt_window_height = num_tokens(prompt_text, '\n') + 5 ;
		newtCenteredWindow(76,
				prompt_window_height,
				prompt_title);
		form = newtForm(NULL, NULL, 0);
		for (i=0; i<num_tokens(prompt_text, '\n'); ++i) {
			extract_token(buf, prompt_text, i, '\n', sizeof buf);
			newtFormAddComponent(form, newtLabel(1, 1+i, buf));
		}
		newtFormAddComponent(form,
			newtEntry(1,
				(prompt_window_height - 2),
				str,
				74,
				(const char **) &result,
				NEWT_FLAG_RETURNEXIT)
		);
		newtRunForm(form);
		strcpy(str, result);

		newtPopWindow();
		newtFormDestroy(form);	

#endif
	}
}

void set_str_val(int msgpos, char *str) {
	strprompt(setup_titles[msgpos], setup_text[msgpos], str);
}



void set_int_val(int msgpos, int *ip)
{
	char buf[16];
	snprintf(buf, sizeof buf, "%d", (int) *ip);
	set_str_val(msgpos, buf);
	*ip = atoi(buf);
}


void set_char_val(int msgpos, char *ip)
{
	char buf[16];
	snprintf(buf, sizeof buf, "%d", (int) *ip);
	set_str_val(msgpos, buf);
	*ip = (char) atoi(buf);
}


void set_long_val(int msgpos, long int *ip)
{
	char buf[16];
	snprintf(buf, sizeof buf, "%ld", *ip);
	set_str_val(msgpos, buf);
	*ip = atol(buf);
}


void edit_value(int curr)
{
	int i;
	struct passwd *pw;
	char ctdluidname[SIZ];

	switch (curr) {

	case 1:
		if (getenv("SYSADMIN_NAME")) {
			strcpy(config.c_sysadm, getenv("SYSADMIN_NAME"));
		}
		else {
			set_str_val(curr, config.c_sysadm);
		}
		break;

	case 2:
#ifdef __CYGWIN__
		config.c_ctdluid = 0;	/* XXX Windows hack, prob. insecure */
#else
		i = config.c_ctdluid;
		pw = getpwuid(i);
		if (pw == NULL) {
			set_int_val(curr, &i);
			config.c_ctdluid = i;
		}
		else {
			strcpy(ctdluidname, pw->pw_name);
			set_str_val(curr, ctdluidname);
			pw = getpwnam(ctdluidname);
			if (pw != NULL) {
				config.c_ctdluid = pw->pw_uid;
			}
			else if (atoi(ctdluidname) > 0) {
				config.c_ctdluid = atoi(ctdluidname);
			}
		}
#endif
		break;

	case 3:
		set_str_val(curr, config.c_ip_addr);
		break;

	case 4:
		set_int_val(curr, &config.c_port_number);
		break;


	}
}

/*
 * (re-)write the config data to disk
 */
void write_config_to_disk(void)
{
	FILE *fp;
	int fd;

	if ((fd = creat(file_citadel_config, S_IRUSR | S_IWUSR)) == -1) {
		display_error("setup: cannot open citadel.config");
		cleanup(1);
	}
	fp = fdopen(fd, "wb");
	if (fp == NULL) {
		display_error("setup: cannot open citadel.config");
		cleanup(1);
	}
	fwrite((char *) &config, sizeof(struct config), 1, fp);
	fclose(fp);
}




/*
 * Figure out what type of user interface we're going to use
 */
int discover_ui(void)
{

	/* Use "dialog" if we have it */
	if (getenv("CTDL_DIALOG") != NULL) {
		return UI_DIALOG;
	}
		

#ifdef HAVE_NEWT
	newtInit();
	newtCls();
	newtDrawRootText(0, 0, "Citadel Setup");
	return UI_NEWT;
#endif
	return UI_TEXT;
}





/*
 * Strip "db" entries out of /etc/nsswitch.conf
 */
void fixnss(void) {
	FILE *fp_read;
	int fd_write;
	char buf[256];
	char buf_nc[256];
	char question[512];
	int i;
	int changed = 0;
	int file_changed = 0;
	char new_filename[64];

	fp_read = fopen(NSSCONF, "r");
	if (fp_read == NULL) {
		return;
	}

	strcpy(new_filename, "/tmp/ctdl_fixnss_XXXXXX");
	fd_write = mkstemp(new_filename);
	if (fd_write < 0) {
		fclose(fp_read);
		return;
	}

	while (fgets(buf, sizeof buf, fp_read) != NULL) {
		changed = 0;
		strcpy(buf_nc, buf);
		for (i=0; i<strlen(buf_nc); ++i) {
			if (buf_nc[i] == '#') {
				buf_nc[i] = 0;
			}
		}
		for (i=0; i<strlen(buf_nc); ++i) {
			if (!strncasecmp(&buf_nc[i], "db", 2)) {
				if (i > 0) {
					if ((isspace(buf_nc[i+2])) || (buf_nc[i+2]==0)) {
						changed = 1;
						file_changed = 1;
						strcpy(&buf_nc[i], &buf_nc[i+2]);
						strcpy(&buf[i], &buf[i+2]);
						if (buf[i]==32) {
							strcpy(&buf_nc[i], &buf_nc[i+1]);
							strcpy(&buf[i], &buf[i+1]);
						}
					}
				}
			}
		}
		if (write(fd_write, buf, strlen(buf)) != strlen(buf)) {
			fclose(fp_read);
			close(fd_write);
			unlink(new_filename);
			return;
		}
	}

	fclose(fp_read);
	
	if (!file_changed) {
		unlink(new_filename);
		return;
	}

	snprintf(question, sizeof question,
		"\n"
		"/etc/nsswitch.conf is configured to use the 'db' module for\n"
		"one or more services.  This is not necessary on most systems,\n"
		"and it is known to crash the Citadel server when delivering\n"
		"mail to the Internet.\n"
		"\n"
		"Do you want this module to be automatically disabled?\n"
		"\n"
	);

	if (yesno(question)) {
		sprintf(buf, "/bin/mv -f %s %s", new_filename, NSSCONF);
		system(buf);
	}
	unlink(new_filename);
}








int main(int argc, char *argv[])
{
	int a;
	int curr; 
	char aaa[128];
	FILE *fp;
	int old_setup_level = 0;
	int info_only = 0;
	struct utsname my_utsname;
	struct passwd *pw;
	struct hostent *he;
	gid_t gid;
	int relh=0;
	int home=0;
	char relhome[PATH_MAX]="";
	char ctdldir[PATH_MAX]=CTDLDIR;
	
	/* set an invalid setup type */
	setup_type = (-1);

	/* Learn whether to skip the auto-service-start questions */
	fp = fopen("/etc/inittab", "r");
	if (fp != NULL) {
		have_sysv_init = 1;
		fclose(fp);
	}

	/* Check to see if we're running the web installer */
	if (getenv("CITADEL_INSTALLER") != NULL) {
		using_web_installer = 1;
	}

	/* parse command line args */
	for (a = 0; a < argc; ++a) {
		if (!strncmp(argv[a], "-u", 2)) {
			strcpy(aaa, argv[a]);
			strcpy(aaa, &aaa[2]);
			setup_type = atoi(aaa);
		}
		if (!strcmp(argv[a], "-i")) {
			info_only = 1;
		}
		if (!strcmp(argv[a], "-q")) {
			setup_type = UI_SILENT;
		}
	}


	/* If a setup type was not specified, try to determine automatically
	 * the best one to use out of all available types.
	 */
	if (setup_type < 0) {
		setup_type = discover_ui();
	}
	if (info_only == 1) {
		important_message("Citadel Setup", CITADEL);
		cleanup(0);
	}

	/* Get started in a valid setup directory. */
	strcpy(setup_directory, 
#ifdef HAVE_RUN_DIR
		   ""
#else
		   CTDLDIR
#endif
		   );
	if ( (using_web_installer) && (getenv("CITADEL") != NULL) ) {
		strcpy(setup_directory, getenv("CITADEL"));
	}
	else {
		set_str_val(0, setup_directory);
	}

	home=(setup_directory[1]!='\0');
	relh=home&(setup_directory[1]!='/');
	if (!relh) {
		safestrncpy(ctdl_home_directory, setup_directory, sizeof ctdl_home_directory);
	}
	else {
		safestrncpy(relhome, ctdl_home_directory, sizeof relhome);
	}

	calc_dirs_n_files(relh, home, relhome, ctdldir);
	
	enable_home=(relh|home);

	if (home) {
		if (chdir(setup_directory) == 0) {
			strcpy(file_citadel_config, "./citadel.config");
		}
		else {
			important_message("Citadel Setup",
			  	"The directory you specified does not exist.");
			cleanup(errno);
		}
	}

	/* Determine our host name, in case we need to use it as a default */
	uname(&my_utsname);

	if (have_sysv_init) {
		/* See if we need to shut down the Citadel service. */
		for (a=0; a<=3; ++a) {
			progress("Shutting down the Citadel service...", a, 3);
			if (a == 0) shutdown_citserver();
			sleep(1);
		}

		/* Make sure it's stopped. */
		if (test_server() == 0) {
			important_message("Citadel Setup",
				"The Citadel service is still running.\n"
				"Please stop the service manually and run "
				"setup again.");
			cleanup(1);
		}
	}

	/* Now begin. */
	switch (setup_type) {

	case UI_TEXT:
		printf("\n\n\n"
			"	       *** Citadel setup program ***\n\n");
		break;

	}

	/*
	 * What we're going to try to do here is append a whole bunch of
	 * nulls to the citadel.config file, so we can keep the old config
	 * values if they exist, but if the file is missing or from an
	 * earlier version with a shorter config structure, when setup tries
	 * to read the old config parameters, they'll all come up zero.
	 * The length of the config file will be set to what it's supposed
	 * to be when we rewrite it, because we replace the old file with a
	 * completely new copy.
	 */
	if ((a = open(file_citadel_config, O_WRONLY | O_CREAT | O_APPEND,
		      S_IRUSR | S_IWUSR)) == -1) {
		display_error("setup: cannot append citadel.config");
		cleanup(errno);
	}
	fp = fdopen(a, "ab");
	if (fp == NULL) {
		display_error("setup: cannot append citadel.config");
		cleanup(errno);
	}
	for (a = 0; a < sizeof(struct config); ++a)
		putc(0, fp);
	fclose(fp);

	/* now we re-open it, and read the old or blank configuration */
	fp = fopen(file_citadel_config, "rb");
	if (fp == NULL) {
		display_error("setup: cannot open citadel.config");
		cleanup(errno);
	}
	fread((char *) &config, sizeof(struct config), 1, fp);
	fclose(fp);

	/* set some sample/default values in place of blanks... */
	if (strlen(config.c_nodename) == 0)
		safestrncpy(config.c_nodename, my_utsname.nodename,
			    sizeof config.c_nodename);
	strtok(config.c_nodename, ".");
	if (strlen(config.c_fqdn) == 0) {
		if ((he = gethostbyname(my_utsname.nodename)) != NULL)
			safestrncpy(config.c_fqdn, he->h_name,
				    sizeof config.c_fqdn);
		else
			safestrncpy(config.c_fqdn, my_utsname.nodename,
				    sizeof config.c_fqdn);
	}
	if (strlen(config.c_humannode) == 0)
		strcpy(config.c_humannode, "My System");
	if (strlen(config.c_phonenum) == 0)
		strcpy(config.c_phonenum, "US 800 555 1212");
	if (config.c_initax == 0) {
		config.c_initax = 4;
	}
	if (strlen(config.c_moreprompt) == 0)
		strcpy(config.c_moreprompt, "<more>");
	if (strlen(config.c_twitroom) == 0)
		strcpy(config.c_twitroom, "Trashcan");
	if (strlen(config.c_baseroom) == 0)
		strcpy(config.c_baseroom, BASEROOM);
	if (strlen(config.c_aideroom) == 0)
		strcpy(config.c_aideroom, "Aide");
	if (config.c_port_number == 0) {
		config.c_port_number = 504;
	}
	if (config.c_sleeping == 0) {
		config.c_sleeping = 900;
	}
	if (config.c_ctdluid == 0) {
		pw = getpwnam("citadel");
		if (pw != NULL)
			config.c_ctdluid = pw->pw_uid;
	}
	if (config.c_ctdluid == 0) {
		pw = getpwnam("bbs");
		if (pw != NULL)
			config.c_ctdluid = pw->pw_uid;
	}
	if (config.c_ctdluid == 0) {
		pw = getpwnam("guest");
		if (pw != NULL)
			config.c_ctdluid = pw->pw_uid;
	}
	if (config.c_createax == 0) {
		config.c_createax = 3;
	}
	/*
	 * Negative values for maxsessions are not allowed.
	 */
	if (config.c_maxsessions < 0) {
		config.c_maxsessions = 0;
	}
	/* We need a system default message expiry policy, because this is
	 * the top level and there's no 'higher' policy to fall back on.
	 * By default, do not expire messages at all.
	 */
	if (config.c_ep.expire_mode == 0) {
		config.c_ep.expire_mode = EXPIRE_MANUAL;
		config.c_ep.expire_value = 0;
	}

	/*
	 * Default port numbers for various services
	 */
	if (config.c_smtp_port == 0) config.c_smtp_port = 25;
	if (config.c_pop3_port == 0) config.c_pop3_port = 110;
	if (config.c_imap_port == 0) config.c_imap_port = 143;
	if (config.c_msa_port == 0) config.c_msa_port = 587;
	if (config.c_smtps_port == 0) config.c_smtps_port = 465;
	if (config.c_pop3s_port == 0) config.c_pop3s_port = 995;
	if (config.c_imaps_port == 0) config.c_imaps_port = 993;

	/* Go through a series of dialogs prompting for config info */
	if (setup_type != UI_SILENT) {
		for (curr = 1; curr <= MAXSETUP; ++curr) {
			edit_value(curr);
		}
	}

/***** begin version update section ***** */
	/* take care of any updating that is necessary */

	old_setup_level = config.c_setup_level;

	if (old_setup_level == 0) {
		goto NEW_INST;
	}

	if (old_setup_level < 555) {
		important_message("Citadel Setup",
				  "This Citadel installation is too old "
				  "to be upgraded.");
		cleanup(1);
	}
	write_config_to_disk();

	old_setup_level = config.c_setup_level;

	/* end of version update section */

NEW_INST:
	config.c_setup_level = REV_LEVEL;

/******************************************/

	write_config_to_disk();

	mkdir(ctdl_info_dir, 0700);
	chmod(ctdl_info_dir, 0700);
	chown(ctdl_info_dir, config.c_ctdluid, -1);

	mkdir(ctdl_bio_dir, 0700);
	chmod(ctdl_bio_dir, 0700);
	chown(ctdl_bio_dir, config.c_ctdluid, -1);

	mkdir(ctdl_usrpic_dir, 0700);
	chmod(ctdl_usrpic_dir, 0700);
	chown(ctdl_usrpic_dir, config.c_ctdluid, -1);

	mkdir(ctdl_message_dir, 0700);
	chmod(ctdl_message_dir, 0700);
	chown(ctdl_message_dir, config.c_ctdluid, -1);

	mkdir(ctdl_hlp_dir, 0700);
	chmod(ctdl_hlp_dir, 0700);
	chown(ctdl_hlp_dir, config.c_ctdluid, -1);

	mkdir(ctdl_image_dir, 0700);
	chmod(ctdl_image_dir, 0700);
	chown(ctdl_image_dir, config.c_ctdluid, -1);

	mkdir(ctdl_bb_dir, 0700);
	chmod(ctdl_bb_dir, 0700);
	chown(ctdl_bb_dir, config.c_ctdluid, -1);

	mkdir(ctdl_file_dir, 0700);
	chmod(ctdl_file_dir, 0700);
	chown(ctdl_file_dir, config.c_ctdluid, -1);

	mkdir(ctdl_netcfg_dir, 0700);
	chmod(ctdl_netcfg_dir, 0700);
	chown(ctdl_netcfg_dir, config.c_ctdluid, -1);

	/* TODO: where to put this? */
	mkdir("netconfigs", 0700);
	chmod("netconfigs", 0700);
	chown("netconfigs", config.c_ctdluid, -1);

	/* Delete files and directories used by older Citadel versions */
	system("exec /bin/rm -fr ./rooms ./chatpipes ./expressmsgs ./sessions 2>/dev/null");
	unlink("citadel.log");
	unlink("weekly");

	check_services_entry();	/* Check /etc/services */
#ifndef __CYGWIN__
	if (have_sysv_init) {
		check_inittab_entry();	/* Check /etc/inittab */
	}
	check_xinetd_entry();	/* Check /etc/xinetd.d/telnet */

	/* Offer to disable other MTA's on the system. */
	disable_other_mta("courier-authdaemon");
	disable_other_mta("courier-imap");
	disable_other_mta("courier-imap-ssl");
	disable_other_mta("courier-pop");
	disable_other_mta("courier-pop3");
	disable_other_mta("courier-pop3d");
	disable_other_mta("cyrmaster");
	disable_other_mta("cyrus");
	disable_other_mta("dovecot");
	disable_other_mta("exim");
	disable_other_mta("exim4");
	disable_other_mta("hula");
	disable_other_mta("imapd");
	disable_other_mta("mta");
	disable_other_mta("pop3d");
	disable_other_mta("popd");
	disable_other_mta("postfix");
	disable_other_mta("qmail");
	disable_other_mta("saslauthd");
	disable_other_mta("sendmail");
	disable_other_mta("vmailmgrd");
	disable_other_mta("zimbra");
#endif

	/* Check for the 'db' nss and offer to disable it */
	fixnss();

	if ((pw = getpwuid(config.c_ctdluid)) == NULL)
		gid = getgid();
	else
		gid = pw->pw_gid;

	progress("Setting file permissions", 0, 4);
	chown(".", config.c_ctdluid, gid);
	sleep(1);
	progress("Setting file permissions", 1, 4);
	chown(file_citadel_config, config.c_ctdluid, gid);
	sleep(1);
	progress("Setting file permissions", 2, 4);

	snprintf(aaa, sizeof aaa,
			 "%schkpwd",
			 ctdl_sbin_dir);
	chown(aaa,0,0); /*  config.c_ctdluid, gid); chkpwd needs to be root owned*/
	sleep(1);
	progress("Setting file permissions", 3, 4);
	chmod(aaa, 04755); 

	sleep(1);
	progress("Setting file permissions", 3, 4);
	chmod(file_citadel_config, S_IRUSR | S_IWUSR);
	sleep(1);
	progress("Setting file permissions", 4, 4);

#ifdef HAVE_LDAP
	/* Contemplate the possibility of auto-configuring OpenLDAP */
	contemplate_ldap();
#endif

	/* See if we can start the Citadel service. */
	if ( (have_sysv_init) && (strlen(citserver_init_entry) > 0) ) {
		for (a=0; a<=3; ++a) {
			progress("Starting the Citadel service...", a, 3);
			if (a == 0) start_citserver();
			sleep(1);
		}
		if (test_server() == 0) {
			important_message("Setup finished",
				"Setup of the Citadel server is complete.\n"
				"If you will be using WebCit, please run its\n"
				"setup program now; otherwise, run './citadel'\n"
				"to log in.\n");
		}
		else {
			important_message("Setup finished",
				"Setup is finished, but the Citadel service "
				"failed to start.\n"
				"Go back and check your configuration.");
		}
	}
	else {
		important_message("Setup finished",
			"Setup is finished.  You may now start the server.");
	}

	cleanup(0);
	return 0;
}


#ifdef HAVE_LDAP
/*
 * If we're in the middle of an Easy Install, we might just be able to
 * auto-configure a standalone OpenLDAP server.
 */
void contemplate_ldap(void) {
	char question[SIZ];
	char slapd_init_entry[SIZ];
	FILE *fp;

	/* If conditions are not ideal, give up on this idea... */
	if (!have_sysv_init) return;
	if (using_web_installer == 0) return;
	if (getenv("LDAP_CONFIG") == NULL) return;
	if (getenv("SUPPORT") == NULL) return;
	if (getenv("SLAPD_BINARY") == NULL) return;
	if (getenv("CITADEL") == NULL) return;

	/* And if inittab is already starting slapd, bail out... */
	locate_init_entry(slapd_init_entry, getenv("SLAPD_BINARY"));
	if (strlen(slapd_init_entry) > 0) {
		important_message("Citadel Setup",
			"You appear to already have a standalone LDAP "
			"service\nconfigured for use with Citadel.  No "
			"changes will be made.\n");
		/* set_init_entry(slapd_init_entry, "off"); */
		return;
	}

	/* Generate a unique entry name for slapd if we don't have one. */
	else {
		generate_entry_name(slapd_init_entry);
	}

	/* Ask the user if it's ok to set up slapd automatically. */
	snprintf(question, sizeof question,
		"\n"
		"Do you want this computer configured to start a standalone\n"
		"LDAP service automatically?  (If you answer yes, a new\n"
		"slapd.conf will be written, and an /etc/inittab entry\n"
		"pointing to %s will be added.)\n"
		"\n",
		getenv("SLAPD_BINARY")
	);
	if (yesno(question) == 0)
		return;

	strcpy(config.c_ldap_base_dn, "dc=example,dc=com");
	strprompt("Base DN",
		"\n"
		"Please enter the Base DN for your directory.  This will\n"
		"generally be something based on the primary DNS domain in\n"
		"which you receive mail, but it does not have to be.  Your\n"
		"LDAP tree will be built using this Distinguished Name.\n"
		"\n",
		config.c_ldap_base_dn
	);

	strcpy(config.c_ldap_host, "localhost");
	config.c_ldap_port = 389;
	sprintf(config.c_ldap_bind_dn, "cn=manager,%s", config.c_ldap_base_dn);

	/*
	 * Generate a bind password.  If you're some grey hat hacker who
	 * is just dying to get some street cred on Bugtraq, and you think
	 * this password generation scheme is too weak, please submit a patch
	 * instead of just whining about it, ok?
	 */
	sprintf(config.c_ldap_bind_pw, "%d%ld", getpid(), (long)time(NULL));

	write_config_to_disk();

	fp = fopen(getenv("LDAP_CONFIG"), "w");
	if (fp == NULL) {
		sprintf(question, "\nCannot create %s:\n%s\n\n"
				"Citadel will still function, but you will "
				"not have an LDAP service.\n\n",
				getenv("LDAP_CONFIG"),
				strerror(errno)
		);
		important_message("Error", question);
		return;
	}

	fprintf(fp, "include	%s/citadel-openldap.schema\n",
		getenv("CITADEL"));
	fprintf(fp, "pidfile	%s/openldap-data/slapd.pid\n",
		getenv("CITADEL"));
	fprintf(fp, "argsfile	%s/openldap-data/slapd.args\n",
		getenv("CITADEL"));
	fprintf(fp,	"allow		bind_v2\n"
			"database	bdb\n"
			"schemacheck	off\n"
	);
	fprintf(fp,	"suffix		\"%s\"\n", config.c_ldap_base_dn);
	fprintf(fp,	"rootdn		\"%s\"\n", config.c_ldap_bind_dn);
	fprintf(fp,	"rootpw		%s\n", config.c_ldap_bind_pw);
	fprintf(fp,	"directory	%s/openldap-data\n",
		getenv("CITADEL"));
	fprintf(fp,	"index		objectClass	eq\n");

	fclose(fp);

	/* This is where our OpenLDAP server will keep its data. */
	mkdir("openldap-data", 0700);

	/* Now write it out to /etc/inittab.
	 * FIXME make it run as some non-root user.
	 * The "-d 0" seems superfluous, but it's actually a way to make
	 * slapd run in the foreground without spewing messages to the console.
	 */
	fp = fopen("/etc/inittab", "a");
	if (fp == NULL) {
		display_error(strerror(errno));
	} else {
		fprintf(fp, "# Start the OpenLDAP server for Citadel...\n");
		fprintf(fp, "%s:2345:respawn:%s -d 0 -f %s\n",
			slapd_init_entry,
			getenv("SLAPD_BINARY"),
			getenv("LDAP_CONFIG")
		);
		fclose(fp);
	}

}
#endif	/* HAVE_LDAP */
