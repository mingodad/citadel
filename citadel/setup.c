/*
 * Citadel/UX setup program
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
#include <netdb.h>
#include <errno.h>
#include <limits.h>

#include "citadel.h"
#include "axdefs.h"
#include "sysdep.h"
#include "config.h"

#ifdef HAVE_CURSES_H
# ifdef OK
# undef OK
# endif
#include <curses.h>
#endif

#define MAXSETUP 19

#define UI_TEXT		0	/* Default setup type -- text only */
#define UI_DIALOG	1	/* Use the 'dialog' program */
#define UI_CURSES	2	/* Use curses */

#define SERVICE_NAME	"citadel"
#define PROTO_NAME	"tcp"

int setup_type;
char setup_directory[128];
int need_init_q = 0;

char *setup_titles[] = {
	"BBS Home Directory",
	"Citadel node name",
	"Fully Qualified Domain Name (FQDN)",
	"Human-readable node name",
	"Phone number",
	"BBS City and State",
	"System Administrator",
	"BBS User ID",
	"'Room Creator = Room Aide' flag",
	"Server timeout period",
	"Initial access level",
	"Registration requirements",
	"Twit Detect!",
	"Twit Detect target room",
	"Maximum concurrent sessions",
	"Paginator prompt",
	"Restrict Internet mail flag",
	"Name of bit bucket subdirectory",
	"System net password",
	"Server port number",
	};


char *setup_text[] = {

"0",
"Enter the full pathname of the directory in which the BBS you are",
"creating or updating resides.  If you specify a directory other than the",
"default, you will need to specify the -h flag to the server when you start",
"it up.",

"1",
"This is the name your system is known by on a Citadel/UX network.  It",
"should be 8 characters or less, and should generally be comprised only of",
"letters.  You can also use numbers and hyphens if necessary.",

"2",
"This is the name your system is known by on the Internet.",
"If you're not on the Internet, simply set this to your",
"node name followed by '.UUCP'.",

"3",
"This is a longer description of your system, readable by",
"us mere humans.  It can be up to 20 characters long and it",
"can have spaces in it.  Note that if you are part of a",
"Cit86Net, this is the name your system will be known by on",
"that network.",

"4",
"This is the main dialup number for your system.  If yours",
"can not be dialed into, then make one up!  It should be in",
"the format 'US 000 000 0000' - the US is your country code",
"(look it up if you're not in the United States) and the",
"rest is, of course, your area code and phone number.",
"This doesn't have any use in Citadel/UX, but gateways to",
"other networks may require it, and someday we may use this",
"to have the networker automatically build a BBS list.",

"5",
"Enter the geographical location of your system (city and",
"state/province/country etc.)",

"6",
"Enter the name of the system administrator (which is probably you).",
"When an account is created with this name, it will automatically be",
"assigned the highest access level.",

"7",
"You should create a user called 'bbs', 'guest', 'citadel', or something",
"similar, that will allow users a way into your BBS.  The server will run",
"under this user ID.  Please specify that (numeric) user ID here.",

"8",
"This is a boolean value.  If you set it to 1, anyone who",
"creates a class 3 (passworded) or class 4 (invitation",
"only) room will automatically become the Room Aide for",
"that room, allowing them to edit it, delete/move messages,",
"etc.  This is an administrative decision: it works well on",
"some systems, and not so well on others.  Set this to 0 to",
"disable this function.",

"9",
"This setting specifies how long a server session may sit idle before it is",
"automatically terminated.  The recommended value is 900 seconds (15",
"minutes).  Note that this has *nothing* to do with any watchdog timer that",
"is presented to the user.  The server's timeout is intended to kill idle or",
"zombie sessions running on a network, etc.  ",
"You MUST set this to a reasonable value.  Setting it to zero will cause",
"the server to malfunction.",

"10",
"This is the access level new users are assigned.",
"",
"The most common settings for this will be either 1, for",
"systems which require new user validation by the system",
"administrator, or 4, for systems which give instant access.",
"The current access levels available are:",

"11",
"'Registration' refers to the boring part of logging into a BBS for the first",
"time: typing your name, address, and telephone number.  Set this value to 1",
"to automatically do registration for new users, or 0 to not auto-register.",
"Optionally, you could set it to, say, 2, to auto-register on a user's second",
"call, but there really isn't much point to doing this.  The recommended",
"value is 1 if you've set your inital access level to 1, or 0 if you've set",
"your initial access level to something higher.",

"12",
"Every BBS has its share of problem users.  This is one",
"good way to deal with them: if you enable this option,",
"anyone you flag as a 'problem user' (access level 2) can",
"post anywhere they want, but their messages will all be",
"automatically moved to a room of your choosing.  Set this",
"value to 1 to enable Twit Detect, or 0 to disable it.",

"13",
"This is the name of the room that problem user messages",
"get moved to if you have Twit Detect enabled.",
"(Note: don't forget to *create* this room!)",

"14",
"This is the maximum number of concurrent Citadel sessions which may be",
"running at any given time.  Use this to keep very busy systems from being",
"overloaded.",
"  Set this value to 0 to allow an unlimited number of sessions.",

"15",
"This is the prompt that appears after each screenful of",
"text - for users that have chosen that option.  Usually",
"a simple '<more>' will do, but some folks like to be",
"creative...",

"16",
"If you have a gateway set up to allow Citadel users to",
"send Internet mail, with sendmail, qmail, or whatever, and",
"you wish to restrict this to only users to whom you have",
"given this privilege, set this flag to 1.  Otherwise, set",
"it to 0 to allow everyone to send Internet mail.",
"(Obviously, if your system doesn't have the ability to",
"send mail to the outside world, this is all irrelevant.)",

"17",
"Select the name of a subdirectory (relative to the main",
"Citadel directory - do not type an absolute pathname!) in",
"which to place arriving file transfers that otherwise",
"don't have a home.",

"18",
"If you use Citadel client/server sessions to transport network spool data",
"between systems, this is the password other systems will use to authenticate",
"themselves as network nodes rather than regular callers.",

"19",
"Specify the TCP port number on which your server will run.  Normally, this",
"will be port 504, which is the official port assigned by the IANA for",
"Citadel servers.  You'll only need to specify a different port number if",
"you run multiple BBS's on the same computer and there's something else",
"already using port 504.",

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

void cleanup(int exitcode) {
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


#ifdef HAVE_CURSES_H
void getlin(int yp, int xp, char *string, int lim)	/* Gets a line from the terminal */
          			/* Where on the screen to start */
              	 		/* Pointer to string buffer */
        			/* Maximum length - if negative, no-show */
{
int a,b; char flag;

	flag=0;
	if (lim<0) { lim=(0-lim); flag=1; }
	move(yp,xp);
	standout();
	for (a=0; a<lim; ++a) addch('-');
	refresh();
	move(yp,xp);
	for (a=0; a<lim; ++a) addch(' ');
	move(yp,xp);
	printw("%s", string);
GLA:	move(yp,xp+strlen(string));
	refresh();
	a=getch();
	if (a==127) a=8;
	a=(a&127);
	if (a==10) a=13;
	if ((a==8)&&(strlen(string)==0)) goto GLA;
	if ((a!=13)&&(a!=8)&&(strlen(string)==lim)) goto GLA;
	if ((a==8)&&(string[0]!=0)) {
		string[strlen(string)-1]=0;
		move(yp,xp+strlen(string));
		addch(' ');
		goto GLA;
		}
	if ((a==13)||(a==10)) {
		standend();
		move(yp,xp);
		for (a=0; a<lim; ++a) addch(' ');
		mvprintw(yp,xp,"%s",string);
		refresh();
		return;
		}
	b=strlen(string);
	string[b]=a;
	string[b+1]=0;
	if (flag==0) addch(a);
	if (flag==1) addch('*');
	goto GLA;
}
#endif



void title(char *text)
{
	if (setup_type == UI_TEXT) {
		printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n<%s>\n",text);
		}
	}


void hit_any_key(void) {
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

	switch(setup_type) {

		case UI_TEXT:
			do {
				printf("%s\nYes/No --> ",question);
				fgets(buf, 4096, stdin);
				answer=tolower(buf[0]);
				if (answer=='y') answer=1;
				else if (answer=='n') answer=0;
				} while ((answer<0)||(answer>1));
			break;

		case UI_DIALOG:
			sprintf(buf, "dialog --yesno \"%s\" 7 80", question);
			answer = ( (system(buf)==0) ? 1 : 0);
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
				answer=tolower(answer);
				if (answer=='y') answer=1;
				else if (answer=='n') answer=0;
				} while ((answer<0)||(answer>1));
			break;
#endif

		}
	return(answer);
	}



void dump_access_levels(void) {
	int a;
	for (a=0; a<=6; ++a) printf("%d %s\n",a,axdefs[a]);
	}

void get_setup_msg(char *dispbuf, int msgnum) {
	int a,b;

	a=0;
	b=0;
	while (atol(setup_text[a]) != msgnum) ++a;
	++a;
	strcpy(dispbuf, "");
	do {
		strcat(dispbuf, setup_text[a++]);
		strcat(dispbuf, "\n");
		} while(atol(setup_text[a])!=(msgnum+1));
	}

void print_setup(int msgnum) {
	char dispbuf[4096];

	get_setup_msg(dispbuf, msgnum);
	printf("\n\n%s\n\n", dispbuf);
	}


void important_message(char *title, char *msgtext) {
	char buf[4096];

	switch(setup_type) {
		
		case UI_TEXT:
			printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
			printf("       %s \n\n%s\n\n", title, msgtext);
			hit_any_key();
			break;

		case UI_DIALOG:
			sprintf(buf, "dialog --title \"%s\" --msgbox \"\n%s\" 20 80",
				title, msgtext);
			system(buf);
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

void important_msgnum(int msgnum) {
	char dispbuf[4096];
	
	get_setup_msg(dispbuf, msgnum);
	important_message("Important Message", dispbuf);
	}

void display_error(char *error_message) {
	important_message("Error", error_message);
	}

void progress(char *text, long int curr, long int cmax)
{
	static long dots_printed;
	long a;
	static long prev;
	static FILE *gauge = NULL;
	char gcmd[256];

	switch(setup_type) {

		case UI_TEXT:
			if (curr==0) {
				printf("%s\n",text);
				printf("..........................");
				printf("..........................");
				printf("..........................\r");
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
			break;

#ifdef HAVE_CURSES_H
		case UI_CURSES:
			if (curr==0) {
				clear();
				move(5, 20);
				printw("%s\n",text);
				move(10, 1);
				printf("..........................");
				printf("..........................");
				printf("..........................\r");
				refresh();
				dots_printed = 0;
				}
			else if (curr==cmax) {
				clear();
				refresh();
				}
			else {
				a=(curr * 100) / cmax;
				a=a*78; a=a/100;
				move(10,1);
				dots_printed = 0;
				while (dots_printed < a) {
					printw("*");
					++dots_printed;
					}
				refresh();
				}
			break;
#endif
			
		case UI_DIALOG:
			if ( (curr == 0) && (gauge == NULL) ) {
				sprintf(gcmd, "dialog --guage \"%s\" 7 80 0",
					text);
				gauge = (FILE *) popen(gcmd, "w");
				prev = 0;
				}
			else if (curr==cmax) {
				fprintf(gauge, "100\n");
				pclose(gauge);
				gauge = NULL;
				}
			else {
				a=(curr * 100) / cmax;
				if (a != prev) {
					fprintf(gauge, "%ld\n", a);
					fflush(gauge);
					}
				prev = a;
				}
			break;
		}
	}



/*
 * check_services_entry()  -- Make sure "citadel" is in /etc/services
 *
 */
void check_services_entry(void) {
	char question[128];
	FILE *sfp;

	sprintf(question,
"There is no '%s' entry in /etc/services.  Would you like to add one?",
		SERVICE_NAME);

	if (getservbyname(SERVICE_NAME, PROTO_NAME) == NULL) {
		if (yesno(question)==1) {
			sfp = fopen("/etc/services", "a");
			if (sfp == NULL) {
				display_error(strerror(errno));
				}
			else {
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
void check_inittab_entry(void) {
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
		display_error(strerror(errno));
		}
	else {
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
	if (have_entry > 0) return;

	/* Otherwise, prompt the user to create an entry. */
	sprintf(question,
"There is no '%s' entry in /etc/inittab.\nWould you like to add one?",
		looking_for);
	if (yesno(question)==0) return;

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
		} while(system(buf)==0);

	/* Now write it out to /etc/inittab */
	infp = fopen("/etc/inittab", "a");
	if (infp == NULL) {
		display_error(strerror(errno));
		}
	else {
		fprintf(infp, "# Start the Citadel/UX server...\n");
		fprintf(infp,"%s:2345:respawn:%s -h%s\n",
			entryname, looking_for, setup_directory);
		fclose(infp);
		need_init_q = 1;
		}
	}



void set_str_val(int msgpos, char str[]) {
	char buf[4096];
	char setupmsg[4096];
	char tempfile[64];
	FILE *fp;

	sprintf(tempfile, "/tmp/setup.%d", getpid());

	switch (setup_type) {
		case UI_TEXT:
			title(setup_titles[msgpos]);
			print_setup(msgpos);
			if (msgpos==11) dump_access_levels();
			printf("This is currently set to:\n%s\n",str);
			printf("Enter new value or press return to leave unchanged:\n");
			fgets(buf, 4096, stdin);
			buf[strlen(buf)-1] = 0;
			if (strlen(buf)!=0) strcpy(str,buf);
			break;
		case UI_DIALOG:
			get_setup_msg(setupmsg, msgpos);
			sprintf(buf,
				"dialog --title \"%s\" --inputbox \"\n%s\n\" 20 80 \"%s\" 2>%s",
				setup_titles[msgpos],
				setupmsg,
				str, tempfile);
			if (system(buf)==0) {
				fp = fopen(tempfile, "rb");
				fgets(str, 4095, fp);
				fclose(fp);
				if (strlen(str)>0) 
					if (str[strlen(str)-1]==10)
						str[strlen(str)-1]=0;
				}
			break;
#ifdef HAVE_CURSES_H
		case UI_CURSES:
			clear();
			move(1, ((80-strlen(setup_titles[msgpos]))/2) );
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
	sprintf(buf,"%d",(int)*ip);
	set_str_val(msgpos, buf);
	*ip = atoi(buf);
	}


void set_char_val(int msgpos, char *ip)
{
	char buf[16];
	sprintf(buf,"%d",(int)*ip);
	set_str_val(msgpos, buf);
	*ip = (char)atoi(buf);
	}


void set_long_val(int msgpos, long int *ip)
{
	char buf[16];
	sprintf(buf,"%ld",*ip);
	set_str_val(msgpos, buf);
	*ip = atol(buf);
	}


void edit_value(int curr)
{
 int a;
 
 switch(curr) {

case 1:
	set_str_val(curr, config.c_nodename);
	break;

case 2:
	set_str_val(curr, config.c_fqdn);
	break;

case 3:
	set_str_val(curr, config.c_humannode);
	break;

case 4:
	set_str_val(curr, config.c_phonenum);
	break;

case 5:
	set_str_val(curr, config.c_bbs_city);
	break;

case 6:
	set_str_val(curr, config.c_sysadm);
	break;

case 7:
	set_int_val(curr, &config.c_bbsuid);
	break;

case 8:
	set_char_val(curr, &config.c_creataide);
	break;

case 9:
	set_int_val(curr, &config.c_sleeping);
	break;

case 10:
	set_char_val(curr, &config.c_initax);
	break;

case 11:
	set_char_val(curr, &config.c_regiscall);
	break;

case 12:
	set_char_val(curr, &config.c_twitdetect);
	break;

case 13:
	set_str_val(curr, config.c_twitroom);
	break;

case 14:
	set_int_val(curr, &config.c_maxsessions);
	break;

case 15:
	set_str_val(curr, config.c_moreprompt);
	break;

case 16:
	set_char_val(curr, &config.c_restrict);
	break;

case 17:
	set_str_val(curr, config.c_bucket_dir);
	config.c_bucket_dir[14] = 0;
	for (a=0; a<strlen(config.c_bucket_dir); ++a)
		if (!isalpha(config.c_bucket_dir[a]))
			strcpy(&config.c_bucket_dir[a],
				&config.c_bucket_dir[a+1]);
	break;

case 18:
	set_str_val(curr, config.c_net_password);
	break;

case 19:
	set_int_val(curr, &config.c_port_number);
	break;


 }
}

/*
 * (re-)write the config data to disk
 */
void write_config_to_disk(void) {
	FILE *fp;

	fp=fopen("citadel.config","wb");
	if (fp==NULL) {
		display_error("setup: cannot open citadel.config");
		cleanup(1);
		}
	fwrite((char *)&config,sizeof(struct config),1,fp);
	fclose(fp);
	}




/*
 * Figure out what type of user interface we're going to use
 */
int discover_ui(void) {

#ifdef HAVE_CURSES_H
	return UI_CURSES;
#endif

	if (system("dialog -h </dev/null 2>&1 |grep Savio")==0) {
		return UI_DIALOG;
		}

	return UI_TEXT;
	}





void main(int argc, char *argv[]) {
	int a;
	int curr;
	char aaa[128];
	FILE *fp;
	int old_setup_level = 0;
	int info_only = 0;

	/* set an invalid setup type */
	setup_type = (-1);

	/* parse command line args */
	for (a=0; a<argc; ++a) {
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

	strcpy(setup_directory, BBSDIR);
	set_str_val(0, setup_directory);
	if (chdir(setup_directory) != 0) {
		important_message("Citadel/UX Setup",
			"The directory you specified does not exist.");
		cleanup(errno);
		}


	switch(setup_type) {
		
		case UI_TEXT:
			printf("\n\n\n               *** Citadel/UX setup program ***\n\n");
			break;
		
		case UI_DIALOG:
			system("exec clear");
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

	fp=fopen("citadel.config","ab");
	if (fp==NULL) {
		display_error("setup: cannot append citadel.config");
		cleanup(errno);
		}
	for (a=0; a<sizeof(struct config); ++a) putc(0,fp);
	fclose(fp);

	/* now we re-open it, and read the old or blank configuration */
	fp=fopen("citadel.config","rb");
	if (fp==NULL) {
		display_error("setup: cannot open citadel.config");
		cleanup(errno);
		}
	fread((char *)&config,sizeof(struct config),1,fp);
	fclose(fp);


	/* set some sample/default values in place of blanks... */
	if (strlen(config.c_nodename)==0)
		strcpy(config.c_nodename,"mysystem");
	if (strlen(config.c_fqdn)==0)
		sprintf(config.c_fqdn,"%s.UUCP",config.c_nodename);
	if (strlen(config.c_humannode)==0)
		strcpy(config.c_humannode,"My System");
	if (strlen(config.c_phonenum)==0)
		strcpy(config.c_phonenum,"US 800 555 1212");
	if (config.c_initax == 0)
		config.c_initax = 1;
	if (strlen(config.c_moreprompt)==0)
		strcpy(config.c_moreprompt,"<more>");
	if (strlen(config.c_twitroom)==0)
		strcpy(config.c_twitroom,"Trashcan");
	if (strlen(config.c_bucket_dir)==0)
		strcpy(config.c_bucket_dir,"bitbucket");
	if (strlen(config.c_net_password)==0)
		strcpy(config.c_net_password,"netpassword");
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

	if (old_setup_level == 0) goto NEW_INST;
	
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
	write_config_to_disk();

	system("mkdir info 2>/dev/null");		/* Create these */
	system("mkdir bio 2>/dev/null");
	system("mkdir userpics 2>/dev/null");
	system("mkdir messages 2>/dev/null");
	system("mkdir help 2>/dev/null");
	system("mkdir images 2>/dev/null");
	sprintf(aaa,"mkdir %s 2>/dev/null",config.c_bucket_dir);
	system(aaa);


	system("rm -fr ./chatpipes 2>/dev/null");	/* Don't need these */
	system("rm -fr ./expressmsgs 2>/dev/null");
	unlink("sessions");

	check_services_entry();		/* Check /etc/services */
	check_inittab_entry();		/* Check /etc/inittab */

	progress("Setting file permissions", 0, 3);
	chown(".", config.c_bbsuid, getgid());
	progress("Setting file permissions", 1, 3);
	chown("citadel.config", config.c_bbsuid, getgid());
	progress("Setting file permissions", 2, 3);
	sprintf(aaa, "find . -exec chown %d {} \\; 2>/dev/null",
		config.c_bbsuid);
	system(aaa);
	progress("Setting file permissions", 3, 3);

	important_message("Setup finished", 
		"Setup is finished.  You may now start the Citadel server.");


	cleanup(0);
}
