/*
 * $Id$
 *
 * WebCit setup utility
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

#include "webcit.h"

#ifdef HAVE_NEWT
#include <newt.h>
#endif


#define UI_TEXT		0	/* Default setup type -- text only */
#define UI_SILENT	3	/* Silent running, for use in scripts */
#define UI_NEWT		4	/* Use the "newt" window library */

int setup_type;
char setup_directory[SIZ];
char init_entry[SIZ];


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
	snprintf(looking_for, sizeof looking_for, "%s/citserver ", WEBCITDIR);

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

void set_value(char *prompt, char str[])
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
		title("WebCit setup");
		printf("\n%s\n", prompt);
		printf("This is currently set to:\n%s\n", str);
		printf("Enter new value or press return to leave unchanged:\n");
		fgets(buf, sizeof buf, stdin);
		buf[strlen(buf) - 1] = 0;
		if (strlen(buf) != 0)
			strcpy(str, buf);
		break;
#ifdef HAVE_NEWT
	case UI_NEWT:

		newtCenteredWindow(76, 10, "WebCit setup");
		form = newtForm(NULL, NULL, 0);
		for (i=0; i<num_tokens(prompt, '\n'); ++i) {
			extract_token(buf, prompt, i, '\n');
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
 * check_inittab_entry()  -- Make sure "webserver" is in /etc/inittab
 *
 */
void check_inittab_entry(void)
{
	FILE *infp;
	char buf[SIZ];
	char looking_for[SIZ];
	char question[128];
	char entryname[5];
	char listenport[128];
	char hostname[128];
	char portname[128];

	/* Determine the fully qualified path name of citserver */
	snprintf(looking_for, sizeof looking_for, "%s/webserver ", WEBCITDIR);

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

	snprintf(question, sizeof question,
		"On which port do you want WebCit to listen for HTTP "
		"requests?\n\nYou can use the standard port (80) if you are "
		"not running another web server\n(such as Apache), otherwise "
		"select another port.");
	sprintf(listenport, "2000");
	set_value(question, listenport);

	/* Find out where Citadel is. */
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
		fprintf(infp, "# Start the WebCit server...\n");
		fprintf(infp, "%s:2345:respawn:%s %s %s\n",
			entryname, looking_for, hostname, portname);
		fclose(infp);
		strcpy(init_entry, entryname);
	}
}




/* 
 * Check to see if our server really works.  Returns 0 on success.
 */
int test_server(void) {

	return 0;		/* FIXME ... stubbed out */

}




/*
 * Figure out what type of user interface we're going to use
 */
int discover_ui(void)
{

#ifdef HAVE_NEWT
	newtInit();
	newtCls();
	newtDrawRootText(0, 0, "WebCit Setup");
	return UI_NEWT;
#endif
	return UI_TEXT;
}





int main(int argc, char *argv[])
{
	int a;
	char aaa[128];
	int info_only = 0;

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
		important_message("WebCit Setup", "Welcome to WebCit setup");
		cleanup(0);
	}

	/* Get started in a valid setup directory. */
	strcpy(setup_directory, WEBCITDIR);
	set_value("FIXME enter your setup directory, dumbass.",
		setup_directory);
	if (chdir(setup_directory) != 0) {
		important_message("WebCit Setup",
			  "The directory you specified does not exist.");
		cleanup(errno);
	}

	/* See if we need to shut down the WebCit service. */
	for (a=0; a<=3; ++a) {
		progress("Shutting down the WebCit service...", a, 3);
		if (a == 0) shutdown_service();
		sleep(1);
	}

	/* Make sure it's stopped.  FIXME put back in
	if (test_server() == 0) {
		important_message("WebCit Setup",
			"The WebCit service is still running.\n"
			"Please stop the service manually and run "
			"setup again.");
		cleanup(1);
	}
	*/

	/* Now begin. */
	switch (setup_type) {

	case UI_TEXT:
		printf("\n\n\n"
			"               *** WebCit setup program ***\n\n");
		break;

	}

	check_inittab_entry();	/* Check /etc/inittab */

	/* See if we can start the WebCit service. */
	if (strlen(init_entry) > 0) {
		for (a=0; a<=3; ++a) {
			progress("Starting the WebCit service...", a, 3);
			if (a == 0) start_the_service();
			sleep(1);
		}
		if (test_server() == 0) {
			important_message("Setup finished",
				"Setup is finished.  You may now log in.");
		}
		else {
			important_message("Setup finished",
				"Setup is finished, but the WebCit service "
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
