/*
 * $Id$
 *
 * WebCit setup utility
 * 
 * (This is basically just an install wizard.  It's not required.)
 *
 */

#include "config.h"
#include "webcit.h"
#include "webserver.h"


#define UI_TEXT		0	/* Default setup type -- text only */
#define UI_DIALOG	2	/* Use the 'dialog' program */
#define UI_SILENT	3	/* Silent running, for use in scripts */

int setup_type;
char setup_directory[SIZ];
int using_web_installer = 0;
char suggested_url[SIZ];

/*
 * Delete an entry from /etc/inittab
 */
void delete_init_entry(char *which_entry)
{
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
			extract_token(entry, buf, 0, ':', sizeof entry);
			extract_token(levels, buf, 1, ':', sizeof levels);
			extract_token(state, buf, 2, ':', sizeof state);
			extract_token(prog, buf, 3, ':', sizeof prog); /* includes 0x0a LF */

			if (!strcmp(entry, which_entry)) {
				strcpy(state, "off");	/* disable it */
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
 * Remove any /etc/inittab entries for webcit, because we don't
 * start it that way anymore.
 */
void delete_the_old_way(void) {
	FILE *infp;
	char buf[1024];
	char looking_for[1024];
	int have_entry = 0;
	char entry[1024];
	char prog[1024];
	char init_entry[1024];


	strcpy(init_entry, "");

	/* Determine the fully qualified path name of webserver */
	snprintf(looking_for, sizeof looking_for, "%s/webserver ", setup_directory);

	/* Pound through /etc/inittab line by line.  Set have_entry to 1 if
	 * an entry is found which we believe starts webserver.
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

	/* Bail out if there's nothing to do. */
	if (!have_entry) return;

	delete_init_entry(init_entry);
}



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



int yesno(char *question)
{
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

	}
	return (answer);
}

void set_value(char *prompt, char str[])
{
	char buf[SIZ];
	char dialog_result[PATH_MAX];
	char setupmsg[SIZ];
	FILE *fp;

	strcpy(setupmsg, "");

	switch (setup_type) {
	case UI_TEXT:
		title("WebCit setup");
		printf("\n%s\n", prompt);
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
			prompt,
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


void display_error(char *error_message)
{
	important_message("Error", error_message);
}

void progress(char *text, long int curr, long int cmax)
{
	static long dots_printed = 0L;
	long a = 0;
	char buf[SIZ];
	static FILE *fp = NULL;

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
 * install_init_scripts()  -- Make sure "webserver" is in /etc/inittab
 *
 */
void install_init_scripts(void)
{
	char question[SIZ];
	char http_port[128];
#ifdef HAVE_OPENSSL
	char https_port[128];
#endif
	char hostname[128];
	char portname[128];
	struct utsname my_utsname;

	FILE *fp;

	/* Otherwise, prompt the user to create an entry. */
	snprintf(question, sizeof question,
		"Would you like to automatically start WebCit at boot?"
	);
	if (yesno(question) == 0)
		return;

	snprintf(question, sizeof question,
		"On which port do you want WebCit to listen for HTTP "
		"requests?\n\nYou can use the standard port (80) if you are "
		"not running another\nweb server (such as Apache), otherwise "
		"select another port.");
	sprintf(http_port, "2000");
	set_value(question, http_port);
	uname(&my_utsname);
	sprintf(suggested_url, "http://%s:%s/", my_utsname.nodename, http_port);

#ifdef HAVE_OPENSSL
	snprintf(question, sizeof question,
		"On which port do you want WebCit to listen for HTTPS "
		"requests?\n\nYou can use the standard port (443) if you are "
		"not running another\nweb server (such as Apache), otherwise "
		"select another port.");
	sprintf(https_port, "443");
	set_value(question, https_port);
#endif

	/* Find out where Citadel is. */
	if ( (using_web_installer) && (getenv("CITADEL") != NULL) ) {
		strcpy(hostname, "uds");
		strcpy(portname, getenv("CITADEL"));
	}
	else {
		snprintf(question, sizeof question,
			"Is the Citadel service running on the same host as WebCit?");
		if (yesno(question)) {
			sprintf(hostname, "uds");
			sprintf(portname, "/usr/local/citadel");
			set_value("In what directory is Citadel installed?", portname);
		}
		else {
			sprintf(hostname, "127.0.0.1");
			sprintf(portname, "504");
			set_value("Enter the host name or IP address of your "
				"Citadel server.", hostname);
			set_value("Enter the port number on which Citadel is "
				"running (usually 504)", portname);
		}
	}


	fp = fopen("/etc/init.d/webcit", "w");
	if (fp == NULL) {
		display_error("Cannot create /etc/init.d/webcit");
		return;
	}

	fprintf(fp,	"#!/bin/sh\n"
			"\n"
			"WEBCIT_DIR=%s\n", setup_directory);
	fprintf(fp,	"HTTP_PORT=%s\n", http_port);
#ifdef HAVE_OPENSSL
	fprintf(fp,	"HTTPS_PORT=%s\n", https_port);
#endif
	fprintf(fp,	"CTDL_HOSTNAME=%s\n", hostname);
	fprintf(fp,	"CTDL_PORTNAME=%s\n", portname);
	fprintf(fp,	"\n"
			"test -d /var/run || exit 0\n"
			"\n"
			"case \"$1\" in\n"
			"\n"
			"start)		echo -n \"Starting WebCit... \"\n"
			"		if   $WEBCIT_DIR/webserver "
		                                        "-d/var/run/webcit.pid "
							"-t/dev/null "
							"-p$HTTP_PORT $CTDL_HOSTNAME $CTDL_PORTNAME\n"
			"		then\n"
			"			echo \"ok\"\n"
			"		else\n"
			"			echo \"failed\"\n"
			"		fi\n");
#ifdef HAVE_OPENSSL
	fprintf(fp,	"		echo -n \"Starting WebCit SSL... \"\n"
			"		if  $WEBCIT_DIR/webserver "
		                                        "-d/var/run/webcit-ssl.pid "
							"-t/dev/null "
							"-s -p$HTTPS_PORT $CTDL_HOSTNAME $CTDL_PORTNAME\n"
			"		then\n"
			"			echo \"ok\"\n"
			"		else\n"
			"			echo \"failed\"\n"
			"		fi\n");
#endif
	fprintf(fp,	"		;;\n"
			"stop)		echo -n \"Stopping WebCit... \"\n"
			"		if kill `cat /var/run/webcit.pid 2>/dev/null` 2>/dev/null\n"
			"		then\n"
			"			echo \"ok\"\n"
			"		else\n"
			"			echo \"failed\"\n"
			"		fi\n"
			"		rm -f /var/run/webcit.pid 2>/dev/null\n");
#ifdef HAVE_OPENSSL
	fprintf(fp,	"		echo -n \"Stopping WebCit SSL... \"\n"
			"		if kill `cat /var/run/webcit-ssl.pid 2>/dev/null` 2>/dev/null\n"
			"		then\n"
			"			echo \"ok\"\n"
			"		else\n"
			"			echo \"failed\"\n"
			"		fi\n"
			"		rm -f /var/run/webcit-ssl.pid 2>/dev/null\n");
#endif
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
	chmod("/etc/init.d/webcit", 0755);

	/* Set up the run levels. */
	system("/bin/rm -f /etc/rc?.d/[SK]??webcit 2>/dev/null");
	system("for x in 2 3 4 5 ; do [ -d /etc/rc$x.d ] && ln -s /etc/init.d/webcit /etc/rc$x.d/S84webcit ; done 2>/dev/null");
	system("for x in 0 6 S; do [ -d /etc/rc$x.d ] && ln -s /etc/init.d/webcit /etc/rc$x.d/K15webcit ; done 2>/dev/null");

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





int main(int argc, char *argv[])
{
	int a;
	char aaa[256];
	int info_only = 0;
	strcpy(suggested_url, "http://<your_host_name>:<port>/");

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
		important_message("WebCit Setup", "Welcome to WebCit setup");
		cleanup(0);
	}

	/* Get started in a valid setup directory. */
	strcpy(setup_directory, PREFIX);
	if ( (using_web_installer) && (getenv("WEBCIT") != NULL) ) {
		strcpy(setup_directory, getenv("WEBCIT"));
	}
	else {
		set_value("In what directory is WebCit installed?",
			setup_directory);
	}
	if (chdir(setup_directory) != 0) {
		important_message("WebCit Setup",
			  "The directory you specified does not exist.");
		cleanup(errno);
	}

	/*
	 * We used to start WebCit by putting it directly into /etc/inittab.
	 * Since some systems are moving away from init, we can't do this anymore.
	 */
	progress("Removing obsolete /etc/inittab entries...", 0, 1);
	delete_the_old_way();
	progress("Removing obsolete /etc/inittab entries...", 1, 1);

	/* Now begin. */
	switch (setup_type) {

	case UI_TEXT:
		printf("\n\n\n"
			"               *** WebCit setup program ***\n\n");
		break;

	}

	/* 
	 * If we're running on SysV, install init scripts.
	 */
	if (!access("/var/run", W_OK)) {
		install_init_scripts();

		if (!access("/etc/init.d/webcit", X_OK)) {
			system("/etc/init.d/webcit stop");
			system("/etc/init.d/webcit start");
		}

		sprintf(aaa,
			"Setup is finished.  You may now log in.\n"
			"Point your web browser at %s\n", suggested_url
		);
		important_message("Setup finished", aaa);
	}

	else {
		important_message("Setup finished",
			"Setup is finished.  You may now start the server.");
	}

	cleanup(0);
	return 0;
}
