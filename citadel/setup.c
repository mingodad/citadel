/*
 * $Id$
 *
 * Citadel/UX setup utility
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

#include "citadel.h"
#include "axdefs.h"
#include "sysdep.h"
#include "config.h"
#include "tools.h"

#ifdef HAVE_NEWT
#include <newt.h>
#endif


#define MAXSETUP 3	/* How many setup questions to ask */

#define UI_TEXT		0	/* Default setup type -- text only */
#define UI_SILENT	3	/* Silent running, for use in scripts */
#define UI_NEWT		4	/* Use the "newt" window library */

#define SERVICE_NAME	"citadel"
#define PROTO_NAME	"tcp"

int setup_type;
char setup_directory[SIZ];
char init_entry[SIZ];

char *setup_titles[] =
{
	"Citadel Home Directory",
	"System Administrator",
	"Citadel User ID",
	"Server port number",
};


char *setup_text[] =
{
"Enter the full pathname of the directory in which the Citadel installation\n"
"you are creating or updating resides.  If you specify a directory other\n"
"than the default, you will need to specify the -h flag to the server when\n"
"you start it up.\n",

"Enter the name of the system administrator (which is probably you).\n"
"When an account is created with this name, it will automatically be\n"
"assigned the highest access level.\n",

"Citadel needs to run under its own user ID.  This would typically be\n"
"called \"citadel\", but if you are running Citadel as a public BBS, you\n"
"might also call it \"bbs\" or \"guest\".  The server will run under this\n"
"user ID.  Please specify that user ID here.  You may specify either a\n"
"user name or a numeric UID.\n",

"Specify the TCP port number on which your server will run.  Normally, this\n"
"will be port 504, which is the official port assigned by the IANA for\n"
"Citadel servers.  You'll only need to specify a different port number if\n"
"you run multiple instances of Citadel on the same computer and there's\n"
"something else already using port 504.\n",

"Setup has detected that you currently have data files from a Citadel/UX\n"
"version 3.2x installation.  The program 'conv_32_40' can upgrade your\n"
"files to version 4.0x format.\n"
" Setup will now exit.  Please either run 'conv_32_40' or delete your data\n"
"files, and run setup again.\n"

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

	inittab = strdup("");
	if (inittab == NULL) return;

	fp = fopen("/etc/inittab", "r");
	if (fp == NULL) return;

	while(fgets(buf, sizeof buf, fp) != NULL) {

		if (num_tokens(buf, ':') == 4) {
			extract_token(entry, buf, 0, ':');
			extract_token(levels, buf, 1, ':');
			extract_token(state, buf, 2, ':');
			extract_token(prog, buf, 3, ':'); /* includes 0x0a LF */

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
 * Shut down the Citadel service if necessary, during setup.
 */
void shutdown_service(void) {
	FILE *infp;
	char buf[SIZ];
	char looking_for[SIZ];
	int have_entry = 0;
	char entry[SIZ];
	char prog[SIZ];

	strcpy(init_entry, "");

	/* Determine the fully qualified path name of citserver */
	snprintf(looking_for, sizeof looking_for, "%s/citserver ", BBSDIR);

	/* Pound through /etc/inittab line by line.  Set have_entry to 1 if
	 * an entry is found which we believe starts citserver.
	 */
	infp = fopen("/etc/inittab", "r");
	if (infp == NULL) {
		return;
	} else {
		while (fgets(buf, sizeof buf, infp) != NULL) {
			buf[strlen(buf) - 1] = 0;
			extract_token(entry, buf, 0, ':');	
			extract_token(prog, buf, 3, ':');
			if (!strncasecmp(prog, looking_for,
			   strlen(looking_for))) {
				++have_entry;
				strcpy(init_entry, entry);
			}
		}
		fclose(infp);
	}

	/* Bail out if there's nothing to do. */
	if (!have_entry) return;

	set_init_entry(init_entry, "off");
}


/*
 * Start the Citadel service.
 */
void start_the_service(void) {
	if (strlen(init_entry) > 0) {
		set_init_entry(init_entry, "respawn");
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
	int i = 0;
#endif
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

#ifdef HAVE_NEWT
	case UI_NEWT:
		newtCenteredWindow(76, 10, "Question");
		form = newtForm(NULL, NULL, 0);
		for (i=0; i<num_tokens(question, '\n'); ++i) {
			extract_token(buf, question, i, '\n');
			newtFormAddComponent(form, newtLabel(1, 1+i, buf));
		}
		yesbutton = newtButton(10, 5, "Yes");
		nobutton = newtButton(60, 5, "No");
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

#ifdef HAVE_NEWT
	case UI_NEWT:
		newtCenteredWindow(76, 10, title);
		form = newtForm(NULL, NULL, 0);
		for (i=0; i<num_tokens(msgtext, '\n'); ++i) {
			extract_token(buf, msgtext, i, '\n');
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
		for (i=0; i<3; ++i) {
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
 * check_inittab_entry()  -- Make sure "citadel" is in /etc/inittab
 *
 */
void check_inittab_entry(void)
{
	FILE *infp;
	char buf[SIZ];
	char looking_for[SIZ];
	char question[128];
	char entryname[5];

	/* Determine the fully qualified path name of citserver */
	snprintf(looking_for, sizeof looking_for, "%s/citserver ", BBSDIR);

	/* If there's already an entry, then we have nothing left to do. */
	if (strlen(init_entry) > 0) {
		return;
	}

	/* Otherwise, prompt the user to create an entry. */
	snprintf(question, sizeof question,
		"There is no '%s' entry in /etc/inittab.\n"
		"Would you like to add one?",
		looking_for);
	if (yesno(question) == 0)
		return;

	/* Generate a unique entry name for /etc/inittab */
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

	/* Now write it out to /etc/inittab */
	infp = fopen("/etc/inittab", "a");
	if (infp == NULL) {
		display_error(strerror(errno));
	} else {
		fprintf(infp, "# Start the Citadel/UX server...\n");
		fprintf(infp, "%s:2345:respawn:%s -h%s\n",
			entryname, looking_for, setup_directory);
		fclose(infp);
		strcpy(init_entry, entryname);
	}
}



void set_str_val(int msgpos, char str[])
{
#ifdef HAVE_NEWT
	newtComponent form;
	char *result;
	int i;
#endif
	char buf[SIZ];
	char tempfile[PATH_MAX];
	char setupmsg[SIZ];

	strcpy(tempfile, tmpnam(NULL));
	strcpy(setupmsg, "");

	switch (setup_type) {
	case UI_TEXT:
		title(setup_titles[msgpos]);
		printf("\n%s\n", setup_text[msgpos]);
		printf("This is currently set to:\n%s\n", str);
		printf("Enter new value or press return to leave unchanged:\n");
		fgets(buf, sizeof buf, stdin);
		buf[strlen(buf) - 1] = 0;
		if (strlen(buf) != 0)
			strcpy(str, buf);
		break;
#ifdef HAVE_NEWT
	case UI_NEWT:

		newtCenteredWindow(76, 10, setup_titles[msgpos]);
		form = newtForm(NULL, NULL, 0);
		for (i=0; i<num_tokens(setup_text[msgpos], '\n'); ++i) {
			extract_token(buf, setup_text[msgpos], i, '\n');
			newtFormAddComponent(form, newtLabel(1, 1+i, buf));
		}
		newtFormAddComponent(form, newtEntry(1, 8, str, 74, &result,
					NEWT_FLAG_RETURNEXIT));
		newtRunForm(form);
		strcpy(str, result);

		newtPopWindow();
		newtFormDestroy(form);	

#endif
	}
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
	char bbsuidname[SIZ];

	switch (curr) {

	case 1:
		set_str_val(curr, config.c_sysadm);
		break;

	case 2:
#ifdef __CYGWIN__
		config.c_bbsuid = 0;	/* XXX Windows hack, prob. insecure */
#else
		i = config.c_bbsuid;
		pw = getpwuid(i);
		if (pw == NULL) {
			set_int_val(curr, &i);
			config.c_bbsuid = i;
		}
		else {
			strcpy(bbsuidname, pw->pw_name);
			set_str_val(curr, bbsuidname);
			pw = getpwnam(bbsuidname);
			if (pw != NULL) {
				config.c_bbsuid = pw->pw_uid;
			}
			else if (atoi(bbsuidname) > 0) {
				config.c_bbsuid = atoi(bbsuidname);
			}
		}
#endif
		break;

	case 3:
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

	if ((fd = creat("citadel.config", S_IRUSR | S_IWUSR)) == -1) {
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

#ifdef HAVE_NEWT
	newtInit();
	newtCls();
	newtDrawRootText(0, 0, "Citadel/UX Setup");
	return UI_NEWT;
#endif
	return UI_TEXT;
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

	/* set an invalid setup type */
	setup_type = (-1);

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
		important_message("Citadel/UX Setup", CITADEL);
		cleanup(0);
	}

	/* Get started in a valid setup directory. */
	strcpy(setup_directory, BBSDIR);
	set_str_val(0, setup_directory);
	if (chdir(setup_directory) != 0) {
		important_message("Citadel/UX Setup",
			  "The directory you specified does not exist.");
		cleanup(errno);
	}

	/* Determine our host name, in case we need to use it as a default */
	uname(&my_utsname);

	/* See if we need to shut down the Citadel service. */
	for (a=0; a<=3; ++a) {
		progress("Shutting down the Citadel service...", a, 3);
		if (a == 0) shutdown_service();
		sleep(1);
	}

	/* Now begin. */
	switch (setup_type) {

	case UI_TEXT:
		printf("\n\n\n"
			"               *** Citadel/UX setup program ***\n\n");
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

	if ((a = open("citadel.config", O_WRONLY | O_CREAT | O_APPEND,
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
	fp = fopen("citadel.config", "rb");
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
	if (strlen(config.c_net_password) == 0)
		strcpy(config.c_net_password, "netpassword");
	if (strlen(config.c_baseroom) == 0)
		strcpy(config.c_baseroom, "Lobby");
	if (strlen(config.c_aideroom) == 0)
		strcpy(config.c_aideroom, "Aide");
	if (config.c_port_number == 0) {
		config.c_port_number = 504;
	}
	if (config.c_sleeping == 0) {
		config.c_sleeping = 900;
	}
	if (config.c_bbsuid == 0) {
		pw = getpwnam("citadel");
		if (pw != NULL)
			config.c_bbsuid = pw->pw_uid;
	}
	if (config.c_bbsuid == 0) {
		pw = getpwnam("bbs");
		if (pw != NULL)
			config.c_bbsuid = pw->pw_uid;
	}
	if (config.c_bbsuid == 0) {
		pw = getpwnam("guest");
		if (pw != NULL)
			config.c_bbsuid = pw->pw_uid;
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
	 */
	if (config.c_ep.expire_mode == 0) {
		config.c_ep.expire_mode = EXPIRE_NUMMSGS;
		config.c_ep.expire_value = 150;
	}

	/*
	 * Default port numbers for various services
	 */
	if (config.c_smtp_port == 0) config.c_smtp_port = 25;
	if (config.c_pop3_port == 0) config.c_pop3_port = 110;
	if (config.c_imap_port == 0) config.c_imap_port = 143;

	/* Go through a series of dialogs prompting for config info */
	if (setup_type != UI_SILENT) {
		for (curr = 1; curr <= MAXSETUP; ++curr) {
			edit_value(curr);
		}
	}

	/*
	   if (setuid(config.c_bbsuid) != 0) {
	   important_message("Citadel/UX Setup",
	   "Failed to change the user ID to your Citadel user.");
	   cleanup(errno);
	   }
	 */

/***** begin version update section ***** */
	/* take care of any updating that is necessary */

	old_setup_level = config.c_setup_level;

	if (old_setup_level == 0)
		goto NEW_INST;

	if (old_setup_level < 323) {
		important_message("Citadel/UX Setup",
				  "This Citadel/UX installation is too old "
				  "to be upgraded.");
		cleanup(1);
	}
	write_config_to_disk();

	if ((config.c_setup_level / 10) == 32) {
		important_msgnum(31);
		cleanup(0);
	}
	if (config.c_setup_level < 400) {
		config.c_setup_level = 400;
	}
	/* end of 3.23 -> 4.00 update section */

	/* end of 4.00 -> 4.02 update section */

	old_setup_level = config.c_setup_level;

	/* end of version update section */

NEW_INST:
	config.c_setup_level = REV_LEVEL;

/******************************************/

	write_config_to_disk();

	mkdir("info", 0700);
	mkdir("bio", 0700);
	mkdir("userpics", 0700);
	mkdir("messages", 0700);
	mkdir("help", 0700);
	mkdir("images", 0700);
	mkdir("netconfigs", 0700);

	/* Delete a bunch of old files from Citadel v4; don't need anymore */
	system("rm -fr ./chatpipes ./expressmsgs ./sessions 2>/dev/null");

	/* Delete the old citadel.log file; this facility has been removed */
	unlink("citadel.log");

	check_services_entry();	/* Check /etc/services */
#ifndef __CYGWIN__
	check_inittab_entry();	/* Check /etc/inittab */
#endif

	if ((pw = getpwuid(config.c_bbsuid)) == NULL)
		gid = getgid();
	else
		gid = pw->pw_gid;

	progress("Setting file permissions", 0, 4);
	chown(".", config.c_bbsuid, gid);
	progress("Setting file permissions", 1, 4);
	chown("citadel.config", config.c_bbsuid, gid);
	progress("Setting file permissions", 2, 4);
	snprintf(aaa, sizeof aaa,
		"find . | grep -v chkpwd | xargs chown %ld:%ld 2>/dev/null",
		(long)config.c_bbsuid, (long)gid);
	system(aaa);
	progress("Setting file permissions", 3, 4);
	chmod("citadel.config", S_IRUSR | S_IWUSR);
	progress("Setting file permissions", 4, 4);

	/* See if we can start the Citadel service. */
	if (strlen(init_entry) > 0) {
		for (a=0; a<=3; ++a) {
			progress("Starting the Citadel service...", a, 3);
			if (a == 0) start_the_service();
			sleep(1);
		}
		important_message("Setup finished",
			"Setup is finished.  You may now log in.");
	}
	else {
		important_message("Setup finished",
			"Setup is finished.  You may now start the server.");
	}

	cleanup(0);
	return 0;
}
