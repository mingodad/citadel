/*
 * Citadel/UX setup program
 * $Id$
 *
 * *** YOU MUST EDIT sysconfig.h >BEFORE< COMPILING SETUP ***
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
#include <netdb.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>

#include "citadel.h"
#include "axdefs.h"
#include "sysdep.h"
#include "config.h"
#include "tools.h"

#ifdef HAVE_CURSES_H
#ifdef OK
#undef OK
#endif
#include <curses.h>
#endif

#define MAXSETUP 5

#define UI_TEXT		0	/* Default setup type -- text only */
#define UI_DIALOG	1	/* Use the 'dialog' program (REMOVED) */
#define UI_CURSES	2	/* Use curses */

#define SERVICE_NAME	"citadel"
#define PROTO_NAME	"tcp"

int setup_type;
char setup_directory[128];
int need_init_q = 0;

char *setup_titles[] =
{
	"BBS Home Directory",
	"System Administrator",
	"BBS User ID",
	"Name of bit bucket subdirectory",
	"Server port number",
};


char *setup_text[] =
{

"0",
"Enter the full pathname of the directory in which the BBS you are",
"creating or updating resides.  If you specify a directory other than the",
"default, you will need to specify the -h flag to the server when you start",
"it up.",

"1",
"Enter the name of the system administrator (which is probably you).",
"When an account is created with this name, it will automatically be",
"assigned the highest access level.",

"2",
"You should create a user called 'bbs', 'guest', 'citadel', or something",
"similar, that will allow users a way into your BBS.  The server will run",
"under this user ID.  Please specify that (numeric) user ID here.",

"3",
"Select the name of a subdirectory (relative to the main",
"Citadel directory - do not type an absolute pathname!) in",
"which to place arriving file transfers that otherwise",
"don't have a home.",

"4",
"Specify the TCP port number on which your server will run.  Normally, this",
"will be port 504, which is the official port assigned by the IANA for",
"Citadel servers.  You'll only need to specify a different port number if",
"you run multiple BBS's on the same computer and there's something else",
"already using port 504.",

"5",
"6",
"7",
"8",
"9",
"10",
"11",
"12",
"13",
"14",
"15",
"16",
"17",
"18",
"19",
"20",
"21",
"22",
"23",
"24",
"25",
"26",
"27",
"28",
"29",
"30",

"31",
"Setup has detected that you currently have data files from a Citadel/UX",
"version 3.2x installation.  The program 'conv_32_40' can upgrade your",
"files to version 4.0x format.",
" Setup will now exit.  Please either run 'conv_32_40' or delete your data",
"files, and run setup again.",

"32",

};

struct config config;
int direction;

void cleanup(int exitcode)
{
#ifdef HAVE_CURSES_H
	if (setup_type == UI_CURSES) {
		clear();
		refresh();
		endwin();
	}
#endif

	/* Do an 'init q' if we need to.  When we hit the right one, init
	 * will take over and setup won't come back because we didn't do a
	 * fork().  If init isn't found, we fall through the bottom of the
	 * loop and setup exits quietly.
	 */
	if (need_init_q) {
		execlp("/sbin/init", "init", "q", NULL);
		execlp("/usr/sbin/init", "init", "q", NULL);
		execlp("/bin/init", "init", "q", NULL);
		execlp("/usr/bin/init", "init", "q", NULL);
		execlp("init", "init", "q", NULL);
	}
	exit(exitcode);
}


/* Gets a line from the terminal */
/* Where on the screen to start */
/* Pointer to string buffer */
/* Maximum length - if negative, no-show */
#ifdef HAVE_CURSES_H
void getlin(int yp, int xp, char *string, int lim) {
	int a, b;
	char flag;

	flag = 0;
	if (lim < 0) {
		lim = (0 - lim);
		flag = 1;
	}
	move(yp, xp);
	standout();
	for (a = 0; a < lim; ++a)
		addch('-');
	refresh();
	move(yp, xp);
	for (a = 0; a < lim; ++a)
		addch(' ');
	move(yp, xp);
	printw("%s", string);
      GLA:move(yp, xp + strlen(string));
	refresh();
	a = getch();
	if (a == 127)
		a = 8;
	a = (a & 127);
	if (a == 10)
		a = 13;
	if ((a == 8) && (strlen(string) == 0))
		goto GLA;
	if ((a != 13) && (a != 8) && (strlen(string) == lim))
		goto GLA;
	if ((a == 8) && (string[0] != 0)) {
		string[strlen(string) - 1] = 0;
		move(yp, xp + strlen(string));
		addch(' ');
		goto GLA;
	}
	if ((a == 13) || (a == 10)) {
		standend();
		move(yp, xp);
		for (a = 0; a < lim; ++a)
			addch(' ');
		mvprintw(yp, xp, "%s", string);
		refresh();
		return;
	}
	b = strlen(string);
	string[b] = a;
	string[b + 1] = 0;
	if (flag == 0)
		addch(a);
	if (flag == 1)
		addch('*');
	goto GLA;
}
#endif



void title(char *text)
{
	if (setup_type == UI_TEXT) {
		printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n<%s>\n", text);
	}
}


void hit_any_key(void)
{
	char junk[5];

#ifdef HAVE_CURSES_H
	if (setup_type == UI_CURSES) {
		mvprintw(20, 0, "Press any key to continue... ");
		refresh();
		getch();
		return;
	}
#endif
	printf("Press return to continue...");
	fgets(junk, 5, stdin);
}

int yesno(char *question)
{
	int answer = 0;
	char buf[4096];

	switch (setup_type) {

	case UI_TEXT:
		do {
			printf("%s\nYes/No --> ", question);
			fgets(buf, 4096, stdin);
			answer = tolower(buf[0]);
			if (answer == 'y')
				answer = 1;
			else if (answer == 'n')
				answer = 0;
		} while ((answer < 0) || (answer > 1));
		break;

#ifdef HAVE_CURSES_H
	case UI_CURSES:
		do {
			clear();
			standout();
			mvprintw(1, 20, "Question");
			standend();
			mvprintw(10, 0, "%-80s", question);
			mvprintw(20, 0, "%80s", "");
			mvprintw(20, 0, "Yes/No -> ");
			refresh();
			answer = getch();
			answer = tolower(answer);
			if (answer == 'y')
				answer = 1;
			else if (answer == 'n')
				answer = 0;
		} while ((answer < 0) || (answer > 1));
		break;
#endif

	}
	return (answer);
}



void dump_access_levels(void)
{
	int a;
	for (a = 0; a <= 6; ++a)
		printf("%d %s\n", a, axdefs[a]);
}

void get_setup_msg(char *dispbuf, int msgnum)
{
	int a, b;

	a = 0;
	b = 0;
	while (atol(setup_text[a]) != msgnum)
		++a;
	++a;
	strcpy(dispbuf, "");
	do {
		strcat(dispbuf, setup_text[a++]);
		strcat(dispbuf, "\n");
	} while (atol(setup_text[a]) != (msgnum + 1));
}

void print_setup(int msgnum)
{
	char dispbuf[4096];

	get_setup_msg(dispbuf, msgnum);
	printf("\n\n%s\n\n", dispbuf);
}


void important_message(char *title, char *msgtext)
{

	switch (setup_type) {

	case UI_TEXT:
		printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
		printf("       %s \n\n%s\n\n", title, msgtext);
		hit_any_key();
		break;

#ifdef HAVE_CURSES_H
	case UI_CURSES:
		clear();
		move(1, 20);
		standout();
		printw("  Important Message  ");
		standend();
		move(3, 0);
		printw("%s", msgtext);
		refresh();
		hit_any_key();
		break;
#endif

	}
}

void important_msgnum(int msgnum)
{
	char dispbuf[4096];

	get_setup_msg(dispbuf, msgnum);
	important_message("Important Message", dispbuf);
}

void display_error(char *error_message)
{
	important_message("Error", error_message);
}

void progress(char *text, long int curr, long int cmax)
{
	static long dots_printed;
	long a;

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

#ifdef HAVE_CURSES_H
	case UI_CURSES:
		if (curr == 0) {
			clear();
			move(5, 20);
			printw("%s\n", text);
			move(10, 1);
			printf("..........................");
			printf("..........................");
			printf("..........................\r");
			refresh();
			dots_printed = 0;
		} else if (curr == cmax) {
			clear();
			refresh();
		} else {
			a = (curr * 100) / cmax;
			a = a * 78;
			a = a / 100;
			move(10, 1);
			dots_printed = 0;
			while (dots_printed < a) {
				printw("*");
				++dots_printed;
			}
			refresh();
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
	char question[128];
	FILE *sfp;

	sprintf(question,
		"There is no '%s' entry in /etc/services.  Would you like to add one?",
		SERVICE_NAME);

	if (getservbyname(SERVICE_NAME, PROTO_NAME) == NULL) {
		if (yesno(question) == 1) {
			sfp = fopen("/etc/services", "a");
			if (sfp == NULL) {
				display_error(strerror(errno));
			} else {
				fprintf(sfp, "%s		504/tcp\n",
					SERVICE_NAME);
				fclose(sfp);
			}
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
	char buf[256];
	char looking_for[256];
	char question[128];
	char *ptr;
	int have_entry = 0;
	char entryname[3];

	/* Determine the fully qualified path name of citserver */
	sprintf(looking_for, "%s/citserver ", BBSDIR);

	/* Pound through /etc/inittab line by line.  Set have_entry to 1 if
	 * an entry is found which we believe starts citserver.
	 */
	infp = fopen("/etc/inittab", "r");
	if (infp == NULL) {
		return;
	} else {
		while (fgets(buf, 256, infp) != NULL) {
			buf[strlen(buf) - 1] = 0;
			ptr = strtok(buf, ":");
			ptr = strtok(NULL, ":");
			ptr = strtok(NULL, ":");
			ptr = strtok(NULL, ":");
			if (ptr != NULL) {
				if (!strncmp(ptr, looking_for, strlen(looking_for))) {
					++have_entry;
				}
			}
		}
		fclose(infp);
	}

	/* If there's already an entry, then we have nothing left to do. */
	if (have_entry > 0)
		return;

	/* Otherwise, prompt the user to create an entry. */
	sprintf(question,
		"There is no '%s' entry in /etc/inittab.\nWould you like to add one?",
		looking_for);
	if (yesno(question) == 0)
		return;

	/* Generate a unique entry name for /etc/inittab */
	sprintf(entryname, "c0");
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
		sprintf(buf,
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
		need_init_q = 1;
	}
}



void set_str_val(int msgpos, char str[])
{
	char buf[4096];
	char tempfile[64];
	char setupmsg[256];

	sprintf(tempfile, tmpnam(NULL));
	strcpy(setupmsg, "");

	switch (setup_type) {
	case UI_TEXT:
		title(setup_titles[msgpos]);
		print_setup(msgpos);
		if (msgpos == 11)
			dump_access_levels();
		printf("This is currently set to:\n%s\n", str);
		printf("Enter new value or press return to leave unchanged:\n");
		fgets(buf, 4096, stdin);
		buf[strlen(buf) - 1] = 0;
		if (strlen(buf) != 0)
			strcpy(str, buf);
		break;
#ifdef HAVE_CURSES_H
	case UI_CURSES:
		clear();
		move(1, ((80 - strlen(setup_titles[msgpos])) / 2));
		standout();
		printw("%s", setup_titles[msgpos]);
		standend();
		move(3, 0);
		get_setup_msg(setupmsg, msgpos);
		printw("%s", setupmsg);
		refresh();
		getlin(20, 0, str, 80);
		break;
#endif
	}
}

void set_int_val(int msgpos, int *ip)
{
	char buf[16];
	sprintf(buf, "%d", (int) *ip);
	set_str_val(msgpos, buf);
	*ip = atoi(buf);
}


void set_char_val(int msgpos, char *ip)
{
	char buf[16];
	sprintf(buf, "%d", (int) *ip);
	set_str_val(msgpos, buf);
	*ip = (char) atoi(buf);
}


void set_long_val(int msgpos, long int *ip)
{
	char buf[16];
	sprintf(buf, "%ld", *ip);
	set_str_val(msgpos, buf);
	*ip = atol(buf);
}


void edit_value(int curr)
{
	int a;

	switch (curr) {

	case 1:
		set_str_val(curr, config.c_sysadm);
		break;

	case 2:
		set_int_val(curr, &config.c_bbsuid);
		break;

	case 3:
		set_str_val(curr, config.c_bucket_dir);
		config.c_bucket_dir[14] = 0;
		for (a = 0; a < strlen(config.c_bucket_dir); ++a)
			if (!isalpha(config.c_bucket_dir[a]))
				strcpy(&config.c_bucket_dir[a],
				       &config.c_bucket_dir[a + 1]);
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

#ifdef HAVE_CURSES_H
	return UI_CURSES;
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
	}


	/* If a setup type was not specified, try to determine automatically
	 * the best one to use out of all available types.
	 */
	if (setup_type < 0) {
		setup_type = discover_ui();
	}
#ifdef HAVE_CURSES_H
	if (setup_type == UI_CURSES) {
		initscr();
		raw();
		noecho();
	}
#endif

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

	/* Now begin. */
	switch (setup_type) {

	case UI_TEXT:
		printf("\n\n\n               *** Citadel/UX setup program ***\n\n");
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
	 * completely new copy.  (Neat, eh?)
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
	if (config.c_initax == 0)
		config.c_initax = 1;
	if (strlen(config.c_moreprompt) == 0)
		strcpy(config.c_moreprompt, "<more>");
	if (strlen(config.c_twitroom) == 0)
		strcpy(config.c_twitroom, "Trashcan");
	if (strlen(config.c_bucket_dir) == 0)
		strcpy(config.c_bucket_dir, "bitbucket");
	if (strlen(config.c_net_password) == 0)
		strcpy(config.c_net_password, "netpassword");
	if (config.c_port_number == 0) {
		config.c_port_number = 504;
	}
	if (config.c_ipgm_secret == 0) {
		srand(getpid());
		config.c_ipgm_secret = rand();
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
	if (config.c_pop3_port == 0) config.c_pop3_port = 110;
	if (config.c_smtp_port == 0) config.c_smtp_port = 25;


	/* Go through a series of dialogs prompting for config info */
	for (curr = 1; curr <= MAXSETUP; ++curr) {
		edit_value(curr);
	}

	/*
	   if (setuid(config.c_bbsuid) != 0) {
	   important_message("Citadel/UX Setup",
	   "Failed to change the user ID to your BBS user.");
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
				  "This Citadel/UX installation is too old to be upgraded.");
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

	system("mkdir info 2>/dev/null");	/* Create these */
	system("mkdir bio 2>/dev/null");
	system("mkdir userpics 2>/dev/null");
	system("mkdir messages 2>/dev/null");
	system("mkdir help 2>/dev/null");
	system("mkdir images 2>/dev/null");
	sprintf(aaa, "mkdir %s 2>/dev/null", config.c_bucket_dir);
	system(aaa);

	/* Delete a bunch of old files from Citadel v4; don't need anymore */
	system("rm -fr ./chatpipes ./expressmsgs sessions 2>/dev/null");

	check_services_entry();	/* Check /etc/services */
	check_inittab_entry();	/* Check /etc/inittab */

	if ((pw = getpwuid(config.c_bbsuid)) == NULL)
		gid = getgid();
	else
		gid = pw->pw_gid;

	progress("Setting file permissions", 0, 5);
	chown(".", config.c_bbsuid, gid);
	progress("Setting file permissions", 1, 5);
	chown("citadel.config", config.c_bbsuid, gid);
	progress("Setting file permissions", 2, 5);
	sprintf(aaa, "find . | grep -v chkpwd | xargs chown %d:%d 2>/dev/null",
		config.c_bbsuid, gid);
	system(aaa);
	progress("Setting file permissions", 3, 5);
	chmod("citadel.config", S_IRUSR | S_IWUSR);
	progress("Setting file permissions", 4, 5);

	important_message("Setup finished",
	    "Setup is finished.  You may now start the Citadel server.");


	cleanup(0);
	return 0;
}
