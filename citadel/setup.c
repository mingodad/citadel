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

#define MAXSETUP 5	/* How many setup questions to ask */

#define UI_TEXT		0	/* Default setup type -- text only */
#define UI_DIALOG	2	/* Use the 'dialog' program */
#define UI_SILENT	3	/* Silent running, for use in scripts */

#define SERVICE_NAME	"citadel"
#define PROTO_NAME	"tcp"
#define NSSCONF		"/etc/nsswitch.conf"

int setup_type;
char setup_directory[PATH_MAX];
int using_web_installer = 0;
int enable_home = 1;

char *setup_titles[] =
{
	"Citadel Home Directory",
	"System Administrator",
	"Citadel User ID",
	"Server IP address",
	"Server port number",
	"Authentication mode"
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

"Normally, a Citadel system uses a \"black box\" authentication mode.\n"
"This means that users do not have accounts or home directories on\n"
"the underlying host system -- Citadel manages its own user database.\n"
"However, if you wish to override this behavior, you can enable the\n"
"host based authentication mode which is traditional for Unix systems.\n"
"WARNING: do *not* change this setting once your system is installed.\n"
"\n"
"(Answer \"no\" unless you completely understand this option)\n"
"Do you want to enable host based authentication mode?\n"

};

struct config config;
int direction;


void cleanup(int exitcode)
{
	exit(exitcode);
}



void title(char *text)
{
	if (setup_type == UI_TEXT) {
		printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n<%s>\n", text);
	}
}



int yesno(char *question, int default_value)
{
	int i = 0;
	int answer = 0;
	char buf[SIZ];

	switch (setup_type) {

	case UI_TEXT:
		do {
			printf("%s\nYes/No [%s] --> ",
				question,
				( default_value ? "Yes" : "No" )
			);
			fgets(buf, sizeof buf, stdin);
			answer = tolower(buf[0]);
			if ((buf[0]==0) || (buf[0]==13) || (buf[0]==10))
				answer = default_value;
			else if (answer == 'y')
				answer = 1;
			else if (answer == 'n')
				answer = 0;
		} while ((answer < 0) || (answer > 1));
		break;

	case UI_DIALOG:
		sprintf(buf, "exec %s %s --yesno '%s' 15 75",
			getenv("CTDL_DIALOG"),
			( default_value ? "" : "--defaultno" ),
			question);
		i = system(buf);
		if (i == 0) {
			answer = 1;
		}
		else {
			answer = 0;
		}
		break;

	}
	return (answer);
}


void important_message(char *title, char *msgtext)
{
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
	char errmsg[256];

	if (getservbyname(SERVICE_NAME, PROTO_NAME) == NULL) {
		for (i=0; i<=2; ++i) {
			progress("Adding service entry...", i, 2);
			if (i == 0) {
				sfp = fopen("/etc/services", "a");
				if (sfp == NULL) {
					sprintf(errmsg, "Cannot open /etc/services: %s", strerror(errno));
					display_error(errmsg);
				} else {
					fprintf(sfp, "%s		504/tcp\n", SERVICE_NAME);
					fclose(sfp);
				}
			}
		}
	}
}




/*
 * delete_inittab_entry()  -- Remove obsolete /etc/inittab entry for Citadel
 *
 */
void delete_inittab_entry(void)
{
	FILE *infp;
	FILE *outfp;
	char looking_for[256];
	char buf[1024];
	char outfilename[32];
	int changes_made = 0;

	/* Determine the fully qualified path name of citserver */
	snprintf(looking_for, 
		 sizeof looking_for,
		 "%s/citserver", 
		 ctdl_sbin_dir
		 );

	/* Now tweak /etc/inittab */
	infp = fopen("/etc/inittab", "r");
	if (infp == NULL) {

		/* If /etc/inittab does not exist, return quietly.
		 * Not all host platforms have it.
		 */
		if (errno == ENOENT) {
			return;
		}

		/* Other errors might mean something really did go wrong.
		 */
		sprintf(buf, "Cannot open /etc/inittab: %s", strerror(errno));
		display_error(buf);
		return;
	}

	strcpy(outfilename, "/tmp/ctdlsetup.XXXXXX");
	outfp = fdopen(mkstemp(outfilename), "w+");
	if (outfp == NULL) {
		sprintf(buf, "Cannot open %s: %s", outfilename, strerror(errno));
		display_error(buf);
		fclose(infp);
		return;
	}

	while (fgets(buf, sizeof buf, infp) != NULL) {
		if (strstr(buf, looking_for) != NULL) {
			fwrite("#", 1, 1, outfp);
			++changes_made;
		}
		fwrite(buf, strlen(buf), 1, outfp);
	}

	fclose(infp);
	fclose(outfp);

	if (changes_made) {
		sprintf(buf, "/bin/mv -f %s /etc/inittab 2>/dev/null", outfilename);
		system(buf);
		system("/sbin/init q 2>/dev/null");
	}
	else {
		unlink(outfilename);
	}
}


/*
 * install_init_scripts()  -- Try to configure to start Citadel at boot
 *
 */
void install_init_scripts(void)
{
	struct stat etcinitd;
	FILE *fp;
	char *initfile = "/etc/init.d/citadel";

	if (yesno("Would you like to automatically start Citadel at boot?\n", 1) == 0) {
		return;
	}

	if ((stat("/etc/init.d/",&etcinitd) == -1) && 
	    (errno == ENOENT))
		initfile = CTDLDIR"/citadel.init";

	fp = fopen(initfile, "w");
	if (fp == NULL) {
		display_error("Cannot create /etc/init.d/citadel");
		return;
	}

	fprintf(fp,	"#!/bin/sh\n"
			"#\n"
			"# Init file for Citadel\n"
			"#\n"
			"# chkconfig: - 79 30\n"
			"# description: Citadel service\n"
			"# processname: citserver\n"
			"# pidfile: %s/citadel.pid\n"
			"\n"
			"CITADEL_DIR=%s\n"
			,
				setup_directory,
				setup_directory
			);
	fprintf(fp,	"\n"
			"test -d /var/run || exit 0\n"
			"\n"
			"case \"$1\" in\n"
			"\n"
			"start)		echo -n \"Starting Citadel... \"\n"
			"		if $CITADEL_DIR/citserver -d -h$CITADEL_DIR\n"
			"		then\n"
			"			echo \"ok\"\n"
			"		else\n"
			"			echo \"failed\"\n"
			"		fi\n");
	fprintf(fp,	"		;;\n"
			"stop)		echo -n \"Stopping Citadel... \"\n"
			"		if $CITADEL_DIR/sendcommand DOWN >/dev/null 2>&1 ; then\n"
			"			echo \"ok\"\n"
			"		else\n"
			"			echo \"failed\"\n"
			"		fi\n"
			"		rm -f %s/citadel.pid 2>/dev/null\n"
			,
				setup_directory
			);
	fprintf(fp,	"		;;\n"
			"restart)	$0 stop\n"
			"		$0 start\n"
			"		;;\n"
			"*)		echo \"Usage: $0 {start|stop|restart}\"\n"
			"		exit 1\n"
			"		;;\n"
			"esac\n"
	);

	fclose(fp);
	chmod("/etc/init.d/citadel", 0755);

	/* Set up the run levels. */
	system("/bin/rm -f /etc/rc?.d/[SK]??citadel 2>/dev/null");
	system("for x in 2 3 4 5 ; do [ -d /etc/rc$x.d ] && ln -s /etc/init.d/citadel /etc/rc$x.d/S79citadel ; done 2>/dev/null");
	system("for x in 0 6 S; do [ -d /etc/rc$x.d ] && ln -s /etc/init.d/citadel /etc/rc$x.d/K30citadel ; done 2>/dev/null");

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
		if (yesno(buf, 1) == 0) {
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
		ctdl_bin_dir);
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
		if (yesno(buf, 1) == 0) {
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
		ctdl_sbin_dir,
		(enable_home)?"-h":"", 
		(enable_home)?ctdl_run_dir:"",
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

	}
}

void set_bool_val(int msgpos, int *ip) {
	title(setup_titles[msgpos]);
	*ip = yesno(setup_text[msgpos], *ip);
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
	char ctdluidname[256];

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

	case 5:
		if (getenv("ENABLE_UNIX_AUTH")) {
			if (!strcasecmp(getenv("ENABLE_UNIX_AUTH"), "yes")) {
				config.c_auth_mode = 1;
			}
			else {
				config.c_auth_mode = 0;
			}
		}
		else {
			set_bool_val(curr, &config.c_auth_mode);
		}
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

	if (yesno(question, 1)) {
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
	strcpy(setup_directory, ctdl_run_dir);
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

	/* Try to stop Citadel if we can */
	if (!access("/etc/init.d/citadel", X_OK)) {
		system("/etc/init.d/citadel stop");
	}

	/* Make sure Citadel is not running. */
	if (test_server() == 0) {
		important_message("Citadel Setup",
			"The Citadel service is still running.\n"
			"Please stop the service manually and run "
			"setup again.");
		cleanup(1);
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
	if (config.c_pftcpdict_port == 0) config.c_pftcpdict_port = -1;
	if (config.c_managesieve_port == 0) config.c_managesieve_port = 2020;

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

	/* Delete files and directories used by older Citadel versions */
	system("exec /bin/rm -fr ./rooms ./chatpipes ./expressmsgs ./sessions 2>/dev/null");
	unlink("citadel.log");
	unlink("weekly");

	check_services_entry();	/* Check /etc/services */
#ifndef __CYGWIN__
	delete_inittab_entry();	/* Remove obsolete /etc/inittab entry */
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
	chown(ctdl_run_dir, config.c_ctdluid, gid);
	progress("Setting file permissions", 1, 4);
	chown(file_citadel_config, config.c_ctdluid, gid);
	progress("Setting file permissions", 2, 4);

	snprintf(aaa, sizeof aaa,
			 "%schkpwd",
			 ctdl_sbin_dir);
	chown(aaa,0,0); /*  config.c_ctdluid, gid); chkpwd needs to be root owned*/
	progress("Setting file permissions", 3, 4);
	chmod(aaa, 04755); 

	progress("Setting file permissions", 3, 4);
	chmod(file_citadel_config, S_IRUSR | S_IWUSR);
	progress("Setting file permissions", 4, 4);

	/* 
	 * If we're running on SysV, install init scripts.
	 */
	if (!access("/var/run", W_OK)) {

		if (getenv("NO_INIT_SCRIPTS") == NULL) {
			install_init_scripts();
		}

		if (!access("/etc/init.d/citadel", X_OK)) {
			system("/etc/init.d/citadel start");
			sleep(3);
		}

		if (test_server() == 0) {
			important_message("Setup finished",
				"Setup of the Citadel server is complete.\n"
				"If you will be using WebCit, please run its\n"
				"setup program now; otherwise, run './citadel'\n"
				"to log in.\n");
		}
		else {
			important_message("Setup failed",
				"Setup is finished, but the Citadel server failed to start.\n"
				"Go back and check your configuration.\n"
			);
		}

	}

	else {
		important_message("Setup finished",
			"Setup is finished.  You may now start the server.");
	}

	cleanup(0);
	return 0;
}


