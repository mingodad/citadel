/*
 * Citadel setup utility
 */

#define SHOW_ME_VAPPEND_PRINTF
#include "ctdl_module.h"
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
#include <libcitadel.h>
#include "citadel.h"
#include "axdefs.h"
#include "sysdep.h"
#include "config.h"
#include "citadel_dirs.h"
#if HAVE_BACKTRACE
#include <execinfo.h>
#endif

#ifdef ENABLE_NLS
#ifdef HAVE_XLOCALE_H
#include <xlocale.h>
#endif
#include <libintl.h>
#include <locale.h>
#define _(string)	gettext(string)
#else
#define _(string)	(string)
#endif


#define MAXSETUP 11	/* How many setup questions to ask */

#define UI_TEXT		0	/* Default setup type -- text only */
#define UI_DIALOG	2	/* Use the 'dialog' program */
#define UI_SILENT	3	/* Silent running, for use in scripts */

#define SERVICE_NAME	"citadel"
#define PROTO_NAME	"tcp"
#define NSSCONF		"/etc/nsswitch.conf"


typedef enum _SetupStep {
	eCitadelHomeDir = 0,
	eSysAdminName = 1,
	eSysAdminPW = 2,
	eUID = 3,
	eIP_ADDR = 4,
	eCTDL_Port = 5,
	eAuthType = 6,
	eLDAP_Host = 7,
	eLDAP_Port = 8,
	eLDAP_Base_DN = 9,
	eLDAP_Bind_DN = 10,
	eLDAP_Bind_PW = 11,
	eMaxQuestions = 12
} eSteupStep;

///"CREATE_XINETD_ENTRY";
/* Environment variables, don't translate! */
const char *EnvNames [eMaxQuestions] = {
        "HOME_DIRECTORY",
	"SYSADMIN_NAME",
	"SYSADMIN_PW",
	"CITADEL_UID",
	"IP_ADDR",
	"CITADEL_PORT",
	"ENABLE_UNIX_AUTH",
	"LDAP_HOST",
	"LDAP_PORT",
	"LDAP_BASE_DN",
	"LDAP_BIND_DN",
	"LDAP_BIND_PW"
};

int setup_type;
int using_web_installer = 0;
int enable_home = 1;
char admin_pass[SIZ];
char admin_cmd[SIZ];

const char *setup_titles[eMaxQuestions];
const char *setup_text[eMaxQuestions];

/* calculate all our path on a central place */
/* where to keep our config */
	


void SetTitles(void)
{
	int have_run_dir;
#ifndef HAVE_RUN_DIR
	have_run_dir = 1;
#else
	have_run_dir = 0;
#endif

#ifdef ENABLE_NLS
	setlocale(LC_MESSAGES, getenv("LANG"));

	bindtextdomain("citadel-setup", LOCALEDIR"/locale");
	textdomain("citadel-setup");
	bind_textdomain_codeset("citadel-setup","UTF8");
#endif

	setup_titles[eCitadelHomeDir] = _("Citadel Home Directory");
	if (have_run_dir)
		setup_text[eCitadelHomeDir] = _(
"Enter the full pathname of the directory in which the Citadel\n"
"installation you are creating or updating resides.  If you\n"
"specify a directory other than the default, you will need to\n"
"specify the -h flag to the server when you start it up.\n");
	else
		setup_text[eCitadelHomeDir] = _(
"Enter the subdirectory name for an alternate installation of "
"Citadel. To do a default installation just leave it blank."
"If you specify a directory other than the default, you will need to\n"
"specify the -h flag to the server when you start it up.\n"
"note that it may not have a leading /");


	setup_titles[eSysAdminName] = _("Citadel administrator username:");
	setup_text[eSysAdminName] = _(
"Please enter the name of the Citadel user account that should be granted "
"administrative privileges once created. If using internal authentication "
"this user account will be created if it does not exist. For external "
"authentication this user account has to exist.");


	setup_titles[eSysAdminPW] = _("Administrator password:");
	setup_text[eSysAdminPW] = _(
"Enter a password for the system administrator. When setup\n"
"completes it will attempt to create the administrator user\n"
"and set the password specified here.\n");

	setup_titles[eUID] = _("Citadel User ID:");
	setup_text[eUID] = _(
"Citadel needs to run under its own user ID.  This would\n"
"typically be called \"citadel\", but if you are running Citadel\n"
"as a public BBS, you might also call it \"bbs\" or \"guest\".\n"
"The server will run under this user ID.  Please specify that\n"
"user ID here.  You may specify either a user name or a numeric\n"
"UID.\n");

	setup_titles[eIP_ADDR] = _("Listening address for the Citadel server:");
	setup_text[eIP_ADDR] = _(
"Please specify the IP address which the server should be listening to. "
"You can name a specific IPv4 or IPv6 address, or you can specify\n"
"\"*\" for \"any address\", \"::\" for \"any IPv6 address\", or \"0.0.0.0\"\n"
"for \"any IPv4 address\". If you leave this blank, Citadel will\n"
"listen on all addresses. "
"This can usually be left to the default unless multiple instances of Citadel "
"are running on the same computer.");

	setup_titles[eCTDL_Port] = _("Server port number:");
	setup_text[eCTDL_Port] = _(
"Specify the TCP port number on which your server will run.\n"
"Normally, this will be port 504, which is the official port\n"
"assigned by the IANA for Citadel servers.  You will only need\n"
"to specify a different port number if you run multiple instances\n"
"of Citadel on the same computer and there is something else\n"
"already using port 504.\n");

	setup_titles[eAuthType] = _("Authentication method to use:");
	setup_text[eAuthType] = _(
"Please choose the user authentication mode. By default Citadel will use its "
"own internal user accounts database. If you choose Host, Citadel users will "
"have accounts on the host system, authenticated via /etc/passwd or a PAM "
"source. LDAP chooses an RFC 2307 compliant directory server, the last option "
"chooses the nonstandard MS Active Directory LDAP scheme."
"\n"
"Do not change this option unless you are sure it is required, since changing "
"back requires a full reinstall of Citadel."
"\n"
" 0. Self contained authentication\n"
" 1. Host system integrated authentication\n"
" 2. External LDAP - RFC 2307 compliant directory\n"
" 3. External LDAP - nonstandard MS Active Directory\n"
"\n"
"For help: http://www.citadel.org/doku.php/faq:installation:authmodes\n"
"\n"
"ANSWER \"0\" UNLESS YOU COMPLETELY UNDERSTAND THIS OPTION.\n");

	setup_titles[eLDAP_Host] = _("LDAP host:");
	setup_text[eLDAP_Host] = _(
"Please enter the host name or IP address of your LDAP server.\n");

	setup_titles[eLDAP_Port] = _("LDAP port number:");
	setup_text[eLDAP_Port] = _(
"Please enter the port number of the LDAP service (usually 389).\n");

	setup_titles[eLDAP_Base_DN] = _("LDAP base DN:");
	setup_text[eLDAP_Base_DN] = _(
"Please enter the Base DN to search for authentication\n"
"(for example: dc=example,dc=com)\n");

	setup_titles[eLDAP_Bind_DN] = _("LDAP bind DN:");
	setup_text[eLDAP_Bind_DN] = _(
"Please enter the DN of an account to use for binding to the LDAP server for "
"performing queries. The account does not require any other privileges. If "
"your LDAP server allows anonymous queries, you can leave this blank."
"Please enter the DN of an account to use for binding to the LDAP server\n"
"for performing queries.  The account does not require any other\n"
"privileges.  If your LDAP server allows anonymous queries, you can\n"
"leave this blank.\n");

	setup_titles[eLDAP_Bind_PW] = _("LDAP bind password:");
	setup_text[eLDAP_Bind_PW] = _(
"If you entered a Bind DN in the previous question, you must now enter\n"
"the password associated with that account.  Otherwise, you can leave this\n"
"blank.\n");

#if 0
// Debug loading of locales... Strace does a better job though.
	printf("Message catalog directory: %s\n", bindtextdomain("citadel-setup", LOCALEDIR"/locale"));
	printf("Text domain: %s\n", textdomain("citadel-setup"));
	printf("Text domain Charset: %s\n", bind_textdomain_codeset("citadel-setup","UTF8"));
	{
		int i;
		for (i = 0; i < eMaxQuestions; i++)
			printf("%s - %s\n", setup_titles[i], _(setup_titles[i]));
		exit(0);
	}
#endif
}

/*
 * print the actual stack frame.
 */
void cit_backtrace(void)
{
#ifdef HAVE_BACKTRACE
	void *stack_frames[50];
	size_t size, i;
	char **strings;

	size = backtrace(stack_frames, sizeof(stack_frames) / sizeof(void*));
	strings = backtrace_symbols(stack_frames, size);
	for (i = 0; i < size; i++) {
		if (strings != NULL)
			fprintf(stderr, "%s\n", strings[i]);
		else
			fprintf(stderr, "%p\n", stack_frames[i]);
	}
	free(strings);
#endif
}

struct config config;

int direction;


void cleanup(int exitcode)
{
//	printf("Exitcode: %d\n", exitcode);
//	cit_backtrace();
	exit(exitcode);
}



void title(const char *text)
{
	if (setup_type == UI_TEXT) {
		printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n<%s>\n", text);
	}
}



int yesno(const char *question, int default_value)
{
	int i = 0;
	int answer = 0;
	char buf[SIZ];

	switch (setup_type) {

	case UI_TEXT:
		do {
			printf("%s\n%s [%s] --> ",
			       question,
			       _("Yes/No"),
			       ( default_value ? _("Yes") : _("No") )
			);
			if (fgets(buf, sizeof buf, stdin))
			{
				answer = tolower(buf[0]);
				if ((buf[0]==0) || (buf[0]==13) || (buf[0]==10))
					answer = default_value;
				else if (answer == 'y')
					answer = 1;
				else if (answer == 'n')
					answer = 0;
			}
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
	case UI_SILENT:
		break;

	}
	return (answer);
}


void important_message(const char *title, const char *msgtext)
{
	char buf[SIZ];
	int rv;

	switch (setup_type) {

	case UI_TEXT:
		printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
		printf("       %s \n\n%s\n\n", title, msgtext);
		printf("%s", _("Press return to continue..."));
		if (fgets(buf, sizeof buf, stdin));
		break;

	case UI_DIALOG:
		sprintf(buf, "exec %s --msgbox '%s' 19 72",
			getenv("CTDL_DIALOG"),
			msgtext);
		rv = system(buf);
		if (rv != 0)
			fprintf(stderr, _("failed to run the dialog command\n"));
		break;
	case UI_SILENT:
		fprintf(stderr, "%s\n", msgtext);
		break;
	}
}

void important_msgnum(int msgnum)
{
	important_message(_("Important Message"), setup_text[msgnum]);
}

void display_error(char *error_message_format, ...)
{
	StrBuf *Msg;
	va_list arg_ptr;

	Msg = NewStrBuf();
	va_start(arg_ptr, error_message_format);
	StrBufVAppendPrintf(Msg, 
			    error_message_format, 
			    arg_ptr);
	va_end(arg_ptr);

	important_message(_("Error"), ChrPtr(Msg));
	FreeStrBuf(&Msg);
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
			printf("....................................................");
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
	case UI_SILENT:
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
			progress(_("Adding service entry..."), i, 2);
			if (i == 0) {
				sfp = fopen("/etc/services", "a");
				if (sfp == NULL) {
					sprintf(errmsg, "%s /etc/services: %s", _("Cannot open"), strerror(errno));
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
 */
void delete_inittab_entry(void)
{
	FILE *infp;
	FILE *outfp;
	char looking_for[256];
	char buf[1024];
	char outfilename[32];
	int changes_made = 0;
	int rv;

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
		sprintf(buf, "%s /etc/inittab: %s", _("Cannot open"), strerror(errno));
		display_error(buf);
		return;
	}

	strcpy(outfilename, "/tmp/ctdlsetup.XXXXXX");
	outfp = fdopen(mkstemp(outfilename), "w+");
	if (outfp == NULL) {
		sprintf(buf, "%s %s: %s", _("Cannot open"), outfilename, strerror(errno));
		display_error(buf);
		fclose(infp);
		return;
	}

	while (fgets(buf, sizeof buf, infp) != NULL) {
		if (strstr(buf, looking_for) != NULL) {
			rv = fwrite("#", 1, 1, outfp);
			if (rv == -1)
			{
				display_error("%s %s\n",
					      _("failed to modify inittab"), 
					      strerror(errno));
			}
			++changes_made;
		}
		rv = fwrite(buf, strlen(buf), 1, outfp);
		if (rv == -1)
		{
			display_error("%s %s\n",
				      _("failed to modify inittab"), 
				      strerror(errno));
		}
	}

	fclose(infp);
	fclose(outfp);

	if (changes_made) {
		sprintf(buf, "/bin/mv -f %s /etc/inittab 2>/dev/null", outfilename);
		rv = system(buf);
		rv = system("/sbin/init q 2>/dev/null");
	}
	else {
		unlink(outfilename);
	}
}


/*
 * install_init_scripts()  -- Try to configure to start Citadel at boot
 */
void install_init_scripts(void)
{
	struct stat etcinitd;
	FILE *fp;
	char *initfile = "/etc/init.d/citadel";
	char command[SIZ];
	int rv;

	if ((stat("/etc/init.d/", &etcinitd) == -1) && 
	    (errno == ENOENT))
	{
		if ((stat("/etc/rc.d/init.d/", &etcinitd) == -1) &&
		    (errno == ENOENT))
			initfile = CTDLDIR"/citadel.init";
		else
			initfile = "/etc/rc.d/init.d/citadel";
	}

	fp = fopen(initfile, "r");
	if (fp != NULL) {
		if (yesno(_("Citadel already appears to be configured to start at boot.\n"
			    "Would you like to keep your boot configuration as is?\n"), 1) == 1) {
			return;
		}
		fclose(fp);
		
	}

	if (yesno(_("Would you like to automatically start Citadel at boot?\n"), 1) == 0) {
		return;
	}

	fp = fopen(initfile, "w");
	if (fp == NULL) {
		display_error("%s /etc/init.d/citadel", _("Cannot create"));
		return;
	}

	fprintf(fp,	"#!/bin/sh\n"
		"#\n"
		"# Init file for Citadel\n"
		"#\n"
		"# chkconfig: - 79 30\n"
		"# description: Citadel service\n"
		"# processname: citserver\n"
		"# pidfile: %s/citadel.pid\n\n"
		"# uncomment this to create coredumps as described in\n"
		"# http://www.citadel.org/doku.php/faq:mastering_your_os:gdb#how.do.i.make.my.system.produce.core-files\n"
		"# ulimit -c unlimited\n"
		"\n"
		"CITADEL_DIR=%s\n"
		,
		ctdl_run_dir,
		ctdl_sbin_dir
		);
	fprintf(fp,	"\n"
		"test -d /var/run || exit 0\n"
		"\n"
		"case \"$1\" in\n"
		"\n"
		"start)		echo -n \"Starting Citadel... \"\n"
		"		if $CITADEL_DIR/citserver -lmail -d\n"
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
		ctdl_run_dir
		);
	fprintf(fp,	"		;;\n"
		"restart)	if $CITADEL_DIR/sendcommand DOWN 1 >/dev/null 2>&1 ; then\n"
		"			echo \"ok\"\n"
		"		else\n"
		"			echo \"failed\"\n"
		"		fi\n"
		"               ;;\n"
		"*)		echo \"Usage: $0 {start|stop|restart}\"\n"
		"		exit 1\n"
		"		;;\n"
		"esac\n"
		);

	fclose(fp);
	chmod(initfile, 0755);

	/* Set up the run levels. */
	rv = system("/bin/rm -f /etc/rc?.d/[SK]??citadel 2>/dev/null");
	if (rv != 0)
		display_error(_("failed to remove system V init links \n"));

	snprintf(command, sizeof(command), "for x in 2 3 4 5 ; do [ -d /etc/rc$x.d ] && ln -s %s /etc/rc$x.d/S79citadel ; done 2>/dev/null", initfile);
	rv = system(command);
	if (rv != 0)
		display_error(_("failed to set system V init links \n"));

	snprintf(command, sizeof(command),"for x in 0 6 S; do [ -d /etc/rc$x.d ] && ln -s %s /etc/rc$x.d/K30citadel ; done 2>/dev/null", initfile);
	rv = system(command);
	if (rv != 0)
		display_error(_("failed to set system V init links \n"));
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
	int rv;

	fp = fopen(filename, "r+");
	if (fp == NULL) return;		/* Not there.  Oh well... */

	while (fgets(buf, sizeof buf, fp) != NULL) {
		if (strstr(buf, "/citadel") != NULL) already_citadel = 1;
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
			 _("Setup can configure the \"xinetd\" service to automatically\n"
			   "connect incoming telnet sessions to Citadel, bypassing the\n"
			   "host system login: prompt.  Would you like to do this?\n"
				 ));
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
	rv = system("/etc/init.d/xinetd restart >/dev/null 2>&1");
	if (rv != 0)
		display_error(_("failed to restart xinetd.\n"));
}



/*
 * Offer to disable other MTA's
 */
void disable_other_mta(const char *mta) {
	char buf[SIZ];
	FILE *fp;
	int lines = 0;
	int rv;

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

	snprintf(buf, sizeof buf,
		 "%s \"%s\" %s%s%s%s%s%s%s", 
		 _("You appear to have the "), 
		 mta, 
		 _(" email program\n"
		   "running on your system.  If you want Citadel mail\n"
		   "connected with "), 
		 mta,
		 _(" you will have to manually integrate\n"
		   "them.  It is preferable to disable "), 
		 mta,
		 _(", and use Citadel's\n"
		   "SMTP, POP3, and IMAP services.\n\n"
		   "May we disable "), 
		 mta, 
		 _("so that Citadel has access to ports\n"
		   "25, 110, and 143?\n")
		);
	if (yesno(buf, 1) == 0) {
		return;
	}
	

	sprintf(buf, "for x in /etc/rc*.d/S*%s; do mv $x `echo $x |sed s/S/K/g`; done >/dev/null 2>&1", mta);
	rv = system(buf);
	if (rv != 0)
		display_error("%s %s.\n", _("failed to disable other mta"), mta);

	sprintf(buf, "/etc/init.d/%s stop >/dev/null 2>&1", mta);
	rv = system(buf);
	if (rv != 0)
		display_error(" %s.\n", _("failed to disable other mta"), mta);
}

const char *other_mtas[] = {
	"courier-authdaemon",
	"courier-imap",
	"courier-imap-ssl",
	"courier-pop",
	"courier-pop3",
	"courier-pop3d",
	"cyrmaster",
	"cyrus",
	"dovecot",
	"exim",
	"exim4",
	"imapd",
	"mta",
	"pop3d",
	"popd",
	"postfix",
	"qmail",
	"saslauthd",
	"sendmail",
	"vmailmgrd",
	""
};

void disable_other_mtas(void)
{
	int i = 0;
	if ((getenv("ACT_AS_MTA") == NULL) || 
	    (getenv("ACT_AS_MTA") &&
	     strcasecmp(getenv("ACT_AS_MTA"), "yes") == 0)) {
		/* Offer to disable other MTA's on the system. */
		while (!IsEmptyStr(other_mtas[i]))
		{
			disable_other_mta(other_mtas[i]);
			i++;
		}
	}
}

/* 
 * Check to see if our server really works.  Returns 0 on success.
 */
int test_server(char *relhomestr, int relhome) {
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

	if (relhome)
		sprintf(cmd, "%s/sendcommand -h%s ECHO %s 2>&1",
			ctdl_sbin_dir,
			relhomestr,
			cookie);
	else
		sprintf(cmd, "%s/sendcommand ECHO %s 2>&1",
			ctdl_sbin_dir,
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

void strprompt(const char *prompt_title, const char *prompt_text, char *Target, char *DefValue)
{
	char buf[SIZ] = "";
	char setupmsg[SIZ];
	char dialog_result[PATH_MAX];
	FILE *fp = NULL;
	int rv;

	strcpy(setupmsg, "");

	switch (setup_type) {
	case UI_TEXT:
		title(prompt_title);
		printf("\n%s\n", prompt_text);
		printf("%s\n%s\n", _("This is currently set to:"), Target);
		printf("%s\n", _("Enter new value or press return to leave unchanged:"));
		if (fgets(buf, sizeof buf, stdin)){
			buf[strlen(buf) - 1] = 0;
		}
		if (!IsEmptyStr(buf))
			strcpy(Target, buf);
		break;

	case UI_DIALOG:
		CtdlMakeTempFileName(dialog_result, sizeof dialog_result);
		sprintf(buf, "exec %s --inputbox '%s' 19 72 '%s' 2>%s",
			getenv("CTDL_DIALOG"),
			prompt_text,
			Target,
			dialog_result);
		rv = system(buf);
		if (rv != 0)
			fprintf(stderr, "failed to run Dialog.\n");
		
		fp = fopen(dialog_result, "r");
		if (fp != NULL) {
			if (fgets(Target, sizeof buf, fp)) {
				if (Target[strlen(Target)-1] == 10) {
					Target[strlen(Target)-1] = 0;
				}
			}
			fclose(fp);
			unlink(dialog_result);
		}
		break;
	case UI_SILENT:
		if (*DefValue != '\0')
			strcpy(Target, DefValue);
		break;
	}
}

void set_bool_val(int msgpos, int *ip, char *DefValue) 
{
	title(setup_titles[msgpos]);
	*ip = yesno(setup_text[msgpos], *ip);
}

void set_str_val(int msgpos, char *Target, char *DefValue) 
{
	strprompt(setup_titles[msgpos], 
		  setup_text[msgpos], 
		  Target, 
		  DefValue);
}

void set_int_val(int msgpos, int *ip, char *DefValue)
{
	char buf[16];
	snprintf(buf, sizeof buf, "%d", (int) *ip);
	set_str_val(msgpos, buf, DefValue);
	*ip = atoi(buf);
}


void set_char_val(int msgpos, char *ip, char *DefValue)
{
	char buf[16];
	snprintf(buf, sizeof buf, "%d", (int) *ip);
	set_str_val(msgpos, buf, DefValue);
	*ip = (char) atoi(buf);
}


void set_long_val(int msgpos, long int *ip, char *DefValue)
{
	char buf[16];
	snprintf(buf, sizeof buf, "%ld", *ip);
	set_str_val(msgpos, buf, DefValue);
	*ip = atol(buf);
}


void edit_value(int curr)
{
	int i;
	struct passwd *pw;
	char ctdluidname[256];
	char *Value = NULL;

	if (setup_type == UI_SILENT)
	{
		Value = getenv(EnvNames[curr]);
	}
	if (Value == NULL)
		Value = "";


	switch (curr) {

	case eSysAdminName:
		set_str_val(curr, config.c_sysadm, Value);
		break;

	case eSysAdminPW:
		set_str_val(curr, admin_pass, Value);
		break;
	
	case eUID:
		if (setup_type == UI_SILENT)
		{		
			if (Value) {
				config.c_ctdluid = atoi(Value);
			}					
		}
		else
		{
#ifdef __CYGWIN__
			config.c_ctdluid = 0;	/* XXX Windows hack, prob. insecure */
#else
			i = config.c_ctdluid;
			pw = getpwuid(i);
			if (pw == NULL) {
				set_int_val(curr, &i, Value);
				config.c_ctdluid = i;
			}
			else {
				strcpy(ctdluidname, pw->pw_name);
				set_str_val(curr, ctdluidname, Value);
				pw = getpwnam(ctdluidname);
				if (pw != NULL) {
					config.c_ctdluid = pw->pw_uid;
				}
				else if (atoi(ctdluidname) > 0) {
					config.c_ctdluid = atoi(ctdluidname);
				}
			}
#endif
		}
		break;

	case eIP_ADDR:
		set_str_val(curr, config.c_ip_addr, Value);
		break;

	case eCTDL_Port:
		set_int_val(curr, &config.c_port_number, Value);
		break;

	case eAuthType:
		if (setup_type == UI_SILENT)
		{
			const char *auth;
			config.c_auth_mode = AUTHMODE_NATIVE;
			auth = Value;
			if (auth != NULL)
			{
				if ((strcasecmp(auth, "yes") == 0) ||
				    (strcasecmp(auth, "host") == 0))
				{
					config.c_auth_mode = AUTHMODE_HOST;
				}
				else if (strcasecmp(auth, "ldap") == 0){
					config.c_auth_mode = AUTHMODE_LDAP;
				}
				else if ((strcasecmp(auth, "ldap_ad") == 0) ||
					 (strcasecmp(auth, "active directory") == 0)){
					config.c_auth_mode = AUTHMODE_LDAP_AD;
				}
			}
		}
		else {
			set_int_val(curr, &config.c_auth_mode, Value);
		}
		break;

	case eLDAP_Host:
		set_str_val(curr, config.c_ldap_host, Value);
		break;

	case eLDAP_Port:
		if (config.c_ldap_port == 0) {
			config.c_ldap_port = 389;
		}
		set_int_val(curr, &config.c_ldap_port, Value);
		break;

	case eLDAP_Base_DN:
		set_str_val(curr, config.c_ldap_base_dn, Value);
		break;

	case eLDAP_Bind_DN:
		set_str_val(curr, config.c_ldap_bind_dn, Value);
		break;

	case eLDAP_Bind_PW:
		set_str_val(curr, config.c_ldap_bind_pw, Value);
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
	int rv;

	if ((fd = creat(file_citadel_config, S_IRUSR | S_IWUSR)) == -1) {
		display_error("%s citadel.config [%s][%s]\n", _("setup: cannot open"), file_citadel_config, strerror(errno));
		cleanup(1);
	}
	fp = fdopen(fd, "wb");
	if (fp == NULL) {
		display_error("%s citadel.config [%s][%s]\n", _("setup: cannot open"), file_citadel_config, strerror(errno));
		cleanup(1);
		return;
	}
	rv = fwrite((char *) &config, sizeof(struct config), 1, fp);

	if (rv == -1)
		display_error("%s citadel.config [%s][%s]\n", _("setup: cannot write"), file_citadel_config, strerror(errno));

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



void migrate_old_installs(void)
{
	int rv;
	rv = system("exec /bin/rm -fr ./rooms ./chatpipes ./expressmsgs ./sessions 2>/dev/null");
	if (rv != 0)
		fprintf(stderr, _("failed to remove old style directories.\n"));
	unlink("citadel.log");
	unlink("weekly");
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
	int file_changed = 0;
	char new_filename[64];
	int rv;

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
		 _(
			 "\n"
			 "/etc/nsswitch.conf is configured to use the 'db' module for\n"
			 "one or more services.  This is not necessary on most systems,\n"
			 "and it is known to crash the Citadel server when delivering\n"
			 "mail to the Internet.\n"
			 "\n"
			 "Do you want this module to be automatically disabled?\n"
			 "\n"
			 )
	);

	if (yesno(question, 1)) {
		sprintf(buf, "/bin/mv -f %s %s", new_filename, NSSCONF);
		rv = system(buf);
		if (rv != 0)
			fprintf(stderr, "failed to edit %s.\n", NSSCONF);

		chmod(NSSCONF, 0644);
	}
	unlink(new_filename);
}

void check_init_script (char *relhome)
{
	int rv;
	FILE *fp;

	/* 
	 * If we're running on SysV, install init scripts.
	 */
	if (!access("/var/run", W_OK)) {

		if (getenv("NO_INIT_SCRIPTS") == NULL) {
			install_init_scripts();
		}

		if (!access("/etc/init.d/citadel", X_OK)) {
			rv = system("/etc/init.d/citadel start");
			if (rv != 0)
				fprintf(stderr, "failed to call our initscript.");
			sleep(3);
		}

		if (test_server(relhome, enable_home) == 0) {
			char buf[SIZ];
			int found_it = 0;

			if (config.c_auth_mode == AUTHMODE_NATIVE) {
				snprintf (admin_cmd, sizeof(admin_cmd), "%s/sendcommand \"CREU %s|%s\" 2>&1", 
				  	ctdl_sbin_dir, config.c_sysadm, admin_pass);
				fp = popen(admin_cmd, "r");
				if (fp != NULL) {
					while (fgets(buf, sizeof buf, fp) != NULL) 
					{
						if ((atol(buf) == 574) || (atol(buf) == 200))
							++found_it;
					}
					pclose(fp);
				}
			
				if (found_it == 0) {
					important_message("Error","Setup failed to create your admin user");
				}
			}

			if (setup_type != UI_SILENT)
				important_message(_("Setup finished"),
						  _("Setup of the Citadel server is complete.\n"
						    "If you will be using WebCit, please run its\n"
						    "setup program now; otherwise, run './citadel'\n"
						    "to log in.\n"));
		}
		else {
			important_message(_("Setup failed"),
					  _("Setup is finished, but the Citadel server failed to start.\n"
					    "Go back and check your configuration.\n")
				);
		}

	}

	else {
		important_message(_("Setup finished"),
				  _("Setup is finished.  You may now start the server."));
	}
}



#define GetDefaultVALINT(CFGNAME, DEFL) GetDefaultValInt(&config.CFGNAME, "CITADEL_"#CFGNAME, DEFL)
void GetDefaultValInt(int *WhereTo, const char *VarName, int DefVal)
{
	const char *ch;
	if (*WhereTo == 0) *WhereTo = DefVal;

	if ((setup_type == UI_SILENT) &&
	    (ch = getenv(VarName), ch != NULL))
	{
		*WhereTo = atoi(ch);
	}
}
#define GetDefaultVALCHAR(CFGNAME, DEFL) GetDefaultValChar(&config.CFGNAME, "CITADEL_"#CFGNAME, DEFL)
void GetDefaultValChar(char *WhereTo, const char *VarName, char DefVal)
{
	const char *ch;
	if (*WhereTo == 0) *WhereTo = DefVal;

	if ((setup_type == UI_SILENT) &&
	    (ch = getenv(VarName), ch != NULL))
	{
		*WhereTo = atoi(ch);
	}
}
#define GetDefaultVALSTR(CFGNAME, DEFL) GetDefaultValStr(&config.CFGNAME[0], sizeof(config.CFGNAME), "CITADEL_"#CFGNAME, DEFL)
void GetDefaultValStr(char *WhereTo, size_t nMax, const char *VarName, const char *DefVal)
{
	const char *ch;
	if (*WhereTo == '\0') 
		safestrncpy(WhereTo, DefVal, nMax);

	if ((setup_type == UI_SILENT) &&
	    (ch = getenv(VarName), ch != NULL))
	{
		safestrncpy(WhereTo, ch, nMax);
	}
}


void set_default_values(void)
{
	struct passwd *pw;
	struct utsname my_utsname;
	struct hostent *he;

	/* Determine our host name, in case we need to use it as a default */
	uname(&my_utsname);

	/* set some sample/default values in place of blanks... */
	GetDefaultVALSTR(c_nodename, my_utsname.nodename);
	strtok(config.c_nodename, ".");
	if (IsEmptyStr(config.c_fqdn) ) {
		if ((he = gethostbyname(my_utsname.nodename)) != NULL) {
			safestrncpy(config.c_fqdn, he->h_name, sizeof config.c_fqdn);
		} else {
			safestrncpy(config.c_fqdn, my_utsname.nodename, sizeof config.c_fqdn);
		}
	}
	GetDefaultVALSTR(c_humannode, _("My System"));
	GetDefaultVALSTR(c_phonenum, _("US 800 555 1212"));

	GetDefaultVALCHAR(c_initax, 4);

	GetDefaultVALSTR(c_moreprompt, "<more>");
	GetDefaultVALSTR(c_twitroom, "Trashcan");
	GetDefaultVALSTR(c_baseroom, BASEROOM);
	GetDefaultVALSTR(c_aideroom, "Aide");
	GetDefaultVALINT(c_port_number, 504);
	
	GetDefaultVALINT(c_sleeping, 900);

	if (config.c_ctdluid == 0) {
		pw = getpwnam("citadel");
		if (pw != NULL) {
			config.c_ctdluid = pw->pw_uid;
		}
	}
	if (config.c_ctdluid == 0) {
		pw = getpwnam("bbs");
		if (pw != NULL) {
			config.c_ctdluid = pw->pw_uid;
		}
	}
	if (config.c_ctdluid == 0) {
		pw = getpwnam("guest");
		if (pw != NULL) {
			config.c_ctdluid = pw->pw_uid;
		}
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
	GetDefaultVALINT(c_smtp_port, 25);
	GetDefaultVALINT(c_pop3_port, 110);
	GetDefaultVALINT(c_imap_port, 143);
	GetDefaultVALINT(c_msa_port, 587);
	GetDefaultVALINT(c_smtps_port, 465);
	GetDefaultVALINT(c_pop3s_port, 995);
	GetDefaultVALINT(c_imaps_port, 993);
	GetDefaultVALINT(c_pftcpdict_port, -1);
	GetDefaultVALINT(c_managesieve_port, 2020);
	GetDefaultVALINT(c_xmpp_c2s_port, 5222);
	GetDefaultVALINT(c_xmpp_s2s_port, 5269);
}


void get_config (void)
{
	int a;
	int rv;
	FILE *fp;

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
		display_error("%s citadel.config [%s][%s]\n", _("setup: cannot append"), file_citadel_config, strerror(errno));
		cleanup(errno);
	}
	fp = fdopen(a, "ab");
	if (fp == NULL) {
		display_error("%s citadel.config [%s][%s]\n", _("setup: cannot append"), file_citadel_config, strerror(errno));
		cleanup(errno);
	}
	for (a = 0; a < sizeof(struct config); ++a) {
		putc(0, fp);
	}
	fclose(fp);

	/* now we re-open it, and read the old or blank configuration */
	fp = fopen(file_citadel_config, "rb");
	if (fp == NULL) {
		display_error("%s citadel.config [%s][%s]\n", _("setup: cannot open"), file_citadel_config, strerror(errno));
		cleanup(errno);
		return;
	}
	rv = fread((char *) &config, sizeof(struct config), 1, fp);
	if (rv == -1)
		display_error("%s citadel.config [%s][%s]\n", _("setup: cannot write"), file_citadel_config, strerror(errno));
	fclose(fp);

}

int main(int argc, char *argv[])
{
	int a;
	int curr; 
	char aaa[128];
	int old_setup_level = 0;
	int info_only = 0;
	int relh=0;
	int home=0;
	char relhome[PATH_MAX]="";
	char ctdldir[PATH_MAX]=CTDLDIR;
	int rv;
	struct passwd *pw;
	gid_t gid;
	
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
		else if (!strcmp(argv[a], "-i")) {
			info_only = 1;
		}
		else if (!strcmp(argv[a], "-q")) {
			setup_type = UI_SILENT;
		}
		else if (!strncmp(argv[a], "-h", 2)) {
			relh=argv[a][2]!='/';
			if (!relh) {
				safestrncpy(ctdl_home_directory, &argv[a][2], sizeof ctdl_home_directory);
			} else {
				safestrncpy(relhome, &argv[a][2], sizeof relhome);
			}
			home = 1;
		}

	}

	calc_dirs_n_files(relh, home, relhome, ctdldir, 0);
	SetTitles();

	/* If a setup type was not specified, try to determine automatically
	 * the best one to use out of all available types.
	 */
	if (setup_type < 0) {
		setup_type = discover_ui();
	}
	if (info_only == 1) {
		important_message(_("Citadel Setup"), CITADEL);
		cleanup(0);
	}

	enable_home = ( relh | home );

	if (chdir(ctdl_run_dir) != 0) {
		display_error(_("Citadel Setup"), 
			      "%s: [%s]\n", 
			      _("The directory you specified does not exist"), 
			      ctdl_run_dir);
		cleanup(errno);
	}


	/* Try to stop Citadel if we can */
	if (!access("/etc/init.d/citadel", X_OK)) {
		rv = system("/etc/init.d/citadel stop");
		if (rv != 0)
			fprintf(stderr, _("failed to stop us using the initscript.\n"));
	}

	/* Make sure Citadel is not running. */
	if (test_server(relhome, enable_home) == 0) {
		important_message(_("Citadel Setup"),
				  _("The Citadel service is still running.\n"
				    "Please stop the service manually and run "
				    "setup again."));
		cleanup(1);
	}

	/* Now begin. */
	switch (setup_type) {

	case UI_TEXT:
		printf("\n\n\n"
		       "	       *** %s ***\n\n",
		       _("Citadel setup program"));
		break;

	}

	get_config ();

	set_default_values();

	/* Go through a series of dialogs prompting for config info */
	for (curr = 1; curr <= MAXSETUP; ++curr) {
		edit_value(curr);
		if ((curr == 6) && (config.c_auth_mode != AUTHMODE_LDAP) && (config.c_auth_mode != AUTHMODE_LDAP_AD)) {
			curr += 5;	/* skip LDAP questions if we're not authenticating against LDAP */
		}
	}

	/***** begin version update section *****/

	old_setup_level = config.c_setup_level;

	if (old_setup_level == 0) {
		goto NEW_INST;
	}

	if (old_setup_level < 555) {
		important_message(
			_("Citadel Setup"),
			_("This Citadel installation is too old to be upgraded.")
		);
		cleanup(1);
	}
	write_config_to_disk();

	old_setup_level = config.c_setup_level;

	/***** end of version update section *****/

NEW_INST:
	config.c_setup_level = REV_LEVEL;

	if ((pw = getpwuid(config.c_ctdluid)) == NULL) {
		gid = getgid();
	} else {
		gid = pw->pw_gid;
	}

	create_run_directories(config.c_ctdluid, gid);

	write_config_to_disk();

        migrate_old_installs();	/* Delete files and directories used by older Citadel versions */

	if (	((setup_type == UI_SILENT)
		&& (getenv("ALTER_ETC_SERVICES")!=NULL))
		|| (setup_type != UI_SILENT)
	) {
		check_services_entry();	/* Check /etc/services */
	}

#ifndef __CYGWIN__
	delete_inittab_entry();	/* Remove obsolete /etc/inittab entry */
	check_xinetd_entry();	/* Check /etc/xinetd.d/telnet */
	disable_other_mtas();   /* Offer to disable other MTAs */
	fixnss();		/* Check for the 'db' nss and offer to disable it */
#endif

	progress(_("Setting file permissions"), 1, 3);
	rv = chown(file_citadel_config, config.c_ctdluid, gid);
	progress(_("Setting file permissions"), 2, 3);
	rv = chmod(file_citadel_config, S_IRUSR | S_IWUSR);
	progress(_("Setting file permissions"), 3, 3);

	check_init_script(relhome);
	cleanup(0);
	return 0;
}


