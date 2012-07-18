/*
 * Configuration screens that are part of the text mode client.
 *
 * Copyright (c) 1987-2012 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>

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

#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <libcitadel.h>
#include "sysdep.h"
///#include "citadel.h"
#include "citadel_ipc.h"
#include "citadel_decls.h"
#include "tuiconfig.h"
#include "messages.h"
#include "routines.h"
#include "commands.h"
///#ifndef HAVE_SNPRINTF
///#include "snprintf.h"
///#endif
#include "screen.h"

/* work around solaris include files */
#ifdef reg
#undef reg
#endif

extern char temp[];
extern char tempdir[];
extern char *axdefs[8];
extern long highest_msg_read;
extern long maxmsgnum;
extern unsigned room_flags;
extern int screenwidth;
char editor_path[PATH_MAX];


/* 
 * General system configuration command
 */
void do_system_configuration(CtdlIPC *ipc)
{

	/* NUM_CONFIGS is now defined in citadel.h */

	char buf[256];
	char sc[NUM_CONFIGS][256];
	char *resp = NULL;
	struct ExpirePolicy *site_expirepolicy = NULL;
	struct ExpirePolicy *mbx_expirepolicy = NULL;
	int a;
	int logpages = 0;
	int r;			/* IPC response code */
	int server_configs = 0;

	/* Clear out the config buffers */
	memset(&sc[0][0], 0, sizeof(sc));

	/* Fetch the current config */
	r = CtdlIPCGetSystemConfig(ipc, &resp, buf);
	if (r / 100 == 1) {
		server_configs = num_tokens(resp, '\n');
		for (a=0; a<server_configs; ++a) {
			if (a < NUM_CONFIGS) {
				extract_token(&sc[a][0], resp, a, '\n', sizeof sc[a]);
			}
		}
	}
	if (resp) free(resp);
	resp = NULL;
	/* Fetch the expire policy (this will silently fail on old servers,
	 * resulting in "default" policy)
	 */
	r = CtdlIPCGetMessageExpirationPolicy(ipc, 2, &site_expirepolicy, buf);
	r = CtdlIPCGetMessageExpirationPolicy(ipc, 3, &mbx_expirepolicy, buf);

	/* Identification parameters */

	strprompt("Node name", &sc[0][0], 15);
	strprompt("Fully qualified domain name", &sc[1][0], 63);
	strprompt("Human readable node name", &sc[2][0], 20);
	strprompt("Telephone number", &sc[3][0], 15);
	strprompt("Geographic location of this system", &sc[12][0], 31);
	strprompt("Name of system administrator", &sc[13][0], 25);
	strprompt("Paginator prompt", &sc[10][0], 79);

	/* Security parameters */

	snprintf(sc[7], sizeof sc[7], "%d", (boolprompt(
		"Require registration for new users",
		atoi(&sc[7][0]))));
	snprintf(sc[29], sizeof sc[29], "%d", (boolprompt(
		"Disable self-service user account creation",
		atoi(&sc[29][0]))));
	strprompt("Initial access level for new users", &sc[6][0], 1);
	strprompt("Access level required to create rooms", &sc[19][0], 1);
	snprintf(sc[67], sizeof sc[67], "%d", (boolprompt(
		"Allow anonymous guest logins",
		atoi(&sc[67][0]))));
	snprintf(sc[4], sizeof sc[4], "%d", (boolprompt(
		"Automatically give room aide privs to a user who creates a private room",
		atoi(&sc[4][0]))));

	snprintf(sc[8], sizeof sc[8], "%d", (boolprompt(
		"Automatically move problem user messages to twit room",
		atoi(&sc[8][0]))));

	strprompt("Name of twit room", &sc[9][0], ROOMNAMELEN);
	snprintf(sc[11], sizeof sc[11], "%d", (boolprompt(
		"Restrict Internet mail to only those with that privilege",
		atoi(&sc[11][0]))));
	snprintf(sc[26], sizeof sc[26], "%d", (boolprompt(
		"Allow Aides to Zap (forget) rooms",
		atoi(&sc[26][0]))));

	if (!IsEmptyStr(&sc[18][0])) {
		logpages = 1;
	}
	else {
		logpages = 0;
	}
	logpages = boolprompt("Log all instant messages", logpages);
	if (logpages) {
		strprompt("Name of logging room", &sc[18][0], ROOMNAMELEN);
	}
	else {
		sc[18][0] = 0;
	}

	/* Commented out because this setting isn't really appropriate to
	 * change while the server is running.
	 *
	 * snprintf(sc[52], sizeof sc[52], "%d", (boolprompt(
	 * 	"Use system authentication",
	 *	atoi(&sc[52][0]))));
	 */

	/* Server tuning */

	strprompt("Server connection idle timeout (in seconds)", &sc[5][0], 4);
	strprompt("Maximum concurrent sessions", &sc[14][0], 4);
	strprompt("Maximum message length", &sc[20][0], 20);
	strprompt("Minimum number of worker threads", &sc[21][0], 3);
	strprompt("Maximum number of worker threads", &sc[22][0], 3);
	snprintf(sc[43], sizeof sc[43], "%d", (boolprompt(
		"Automatically delete committed database logs",
		atoi(&sc[43][0]))));

	strprompt("Server IP address (* for 'any')", &sc[37][0], 15);
	strprompt("POP3 server port (-1 to disable)", &sc[23][0], 5);
	strprompt("POP3S server port (-1 to disable)", &sc[40][0], 5);
	strprompt("IMAP server port (-1 to disable)", &sc[27][0], 5);
	strprompt("IMAPS server port (-1 to disable)", &sc[39][0], 5);
	strprompt("SMTP MTA server port (-1 to disable)", &sc[24][0], 5);
	strprompt("SMTP MSA server port (-1 to disable)", &sc[38][0], 5);
	strprompt("SMTPS server port (-1 to disable)", &sc[41][0], 5);
	strprompt("Postfix TCP Dictionary Port server port (-1 to disable)", &sc[50][0], 5);
	strprompt("ManageSieve server port (-1 to disable)", &sc[51][0], 5);

	strprompt("XMPP (Jabber) client to server port (-1 to disable)", &sc[62][0], 5);
	/* No prompt because we don't implement this service yet, it's just a placeholder */
	/* strprompt("XMPP (Jabber) server to server port (-1 to disable)", &sc[63][0], 5); */

	/* This logic flips the question around, because it's one of those
	 * situations where 0=yes and 1=no
	 */
	a = atoi(sc[25]);
	a = (a ? 0 : 1);
	a = boolprompt("Correct forged From: lines during authenticated SMTP",
		a);
	a = (a ? 0 : 1);
	snprintf(sc[25], sizeof sc[25], "%d", a);

	snprintf(sc[66], sizeof sc[66], "%d", (boolprompt(
		"Flag messages as spam instead of rejecting",
		atoi(&sc[66][0]))));

	/* This logic flips the question around, because it's one of those
	 * situations where 0=yes and 1=no
	 */
	a = atoi(sc[61]);
	a = (a ? 0 : 1);
	a = boolprompt("Force IMAP posts in public rooms to be from the user who submitted them", a);
	a = (a ? 0 : 1);
	snprintf(sc[61], sizeof sc[61], "%d", a);

	snprintf(sc[45], sizeof sc[45], "%d", (boolprompt(
		"Allow unauthenticated SMTP clients to spoof my domains",
		atoi(&sc[45][0]))));
	snprintf(sc[57], sizeof sc[57], "%d", (boolprompt(
		"Perform RBL checks at greeting instead of after RCPT",
		atoi(&sc[57][0]))));
	snprintf(sc[44], sizeof sc[44], "%d", (boolprompt(
		"Instantly expunge deleted IMAP messages",
		atoi(&sc[44][0]))));

	/* LDAP settings */
	if (ipc->ServInfo.supports_ldap) {
		a = strlen(&sc[32][0]);
		a = (a ? 1 : 0);	/* Set only to 1 or 0 */
		a = boolprompt("Do you want to configure LDAP authentication?", a);
		if (a) {
			strprompt("Host name of LDAP server",
				&sc[32][0], 127);
			strprompt("Port number of LDAP service",
				&sc[33][0], 5);
			strprompt("Base DN", &sc[34][0], 255);
			strprompt("Bind DN (or blank for anonymous bind)", &sc[35][0], 255);
			strprompt("Password for bind DN (or blank for anonymous bind)", &sc[36][0], 255);
		}
		else {
			strcpy(&sc[32][0], "");
		}
	}

	/* Expiry settings */
	strprompt("Default user purge time (days)", &sc[16][0], 5);
	strprompt("Default room purge time (days)", &sc[17][0], 5);

	/* Angels and demons dancing in my head... */
	do {
		snprintf(buf, sizeof buf, "%d", site_expirepolicy->expire_mode);
		strprompt("System default message expire policy (? for list)",
			  buf, 1);
		if (buf[0] == '?') {
			scr_printf("\n"
				"1. Never automatically expire messages\n"
				"2. Expire by message count\n"
				"3. Expire by message age\n");
		}
	} while ((buf[0] < '1') || (buf[0] > '3'));
	site_expirepolicy->expire_mode = buf[0] - '0';

	/* ...lunatics and monsters underneath my bed */
	if (site_expirepolicy->expire_mode == 2) {
		snprintf(buf, sizeof buf, "%d", site_expirepolicy->expire_value);
		strprompt("Keep how many messages online?", buf, 10);
		site_expirepolicy->expire_value = atol(buf);
	}
	if (site_expirepolicy->expire_mode == 3) {
		snprintf(buf, sizeof buf, "%d", site_expirepolicy->expire_value);
		strprompt("Keep messages for how many days?", buf, 10);
		site_expirepolicy->expire_value = atol(buf);
	}

	/* Media messiahs preying on my fears... */
	do {
		snprintf(buf, sizeof buf, "%d", mbx_expirepolicy->expire_mode);
		strprompt("Mailbox default message expire policy (? for list)",
			  buf, 1);
		if (buf[0] == '?') {
			scr_printf("\n"
				"0. Go with the system default\n"
				"1. Never automatically expire messages\n"
				"2. Expire by message count\n"
				"3. Expire by message age\n");
		}
	} while ((buf[0] < '0') || (buf[0] > '3'));
	mbx_expirepolicy->expire_mode = buf[0] - '0';

	/* ...Pop culture prophets playing in my ears */
	if (mbx_expirepolicy->expire_mode == 2) {
		snprintf(buf, sizeof buf, "%d", mbx_expirepolicy->expire_value);
		strprompt("Keep how many messages online?", buf, 10);
		mbx_expirepolicy->expire_value = atol(buf);
	}
	if (mbx_expirepolicy->expire_mode == 3) {
		snprintf(buf, sizeof buf, "%d", mbx_expirepolicy->expire_value);
		strprompt("Keep messages for how many days?", buf, 10);
		mbx_expirepolicy->expire_value = atol(buf);
	}

	strprompt("How often to run network jobs (in seconds)", &sc[28][0], 5);
	strprompt("Default frequency to run POP3 collection (in seconds)", &sc[64][0], 5);
	strprompt("Fastest frequency to run POP3 collection (in seconds)", &sc[65][0], 5);
	strprompt("Hour to run purges (0-23)", &sc[31][0], 2);
	snprintf(sc[42], sizeof sc[42], "%d", (boolprompt(
		"Enable full text search index (warning: resource intensive)",
		atoi(&sc[42][0]))));

	snprintf(sc[46], sizeof sc[46], "%d", (boolprompt(
		"Perform journaling of email messages",
		atoi(&sc[46][0]))));
	snprintf(sc[47], sizeof sc[47], "%d", (boolprompt(
		"Perform journaling of non-email messages",
		atoi(&sc[47][0]))));
	if ( (atoi(&sc[46][0])) || (atoi(&sc[47][0])) ) {
		strprompt("Email destination of journalized messages",
			&sc[48][0], 127);
	}

	/* Funambol push stuff */
	int yes_funambol = 0;
	if (strlen(sc[53]) > 0) yes_funambol = 1;
	yes_funambol = boolprompt("Connect to an external Funambol sync server", yes_funambol);
	if (yes_funambol) {
		strprompt("Funambol server (blank to disable)", &sc[53][0], 63);
		strprompt("Funambol server port", &sc[54][0], 5);
		strprompt("Funambol sync source", &sc[55][0], 63);
		strprompt("Funambol authentication details (user:pass in Base64)", &sc[56][0],63);
	}
	else {
		sc[53][0] = 0;
		sc[54][0] = 0;
		sc[55][0] = 0;
		sc[56][0] = 0;
	}

	/* External pager stuff */
	int yes_pager = 0;
	if (strlen(sc[60]) > 0) yes_pager = 1;
	yes_pager = boolprompt("Configure an external pager tool", yes_pager);
	if (yes_pager) {
		strprompt("External pager tool", &sc[60][0], 255);
	}
	else {
		sc[60][0] = 0;
	}

	/* Master user account */
	int yes_muacct = 0;
	if (strlen(sc[58]) > 0) yes_muacct = 1;
	yes_muacct = boolprompt("Enable a 'master user' account", yes_muacct);
	if (yes_muacct) {
		strprompt("Master user name", &sc[58][0], 31);
		strprompt("Master user password", &sc[59][0], -31);
	}
	else {
		strcpy(&sc[58][0], "");
		strcpy(&sc[59][0], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
	}

	/* Save it */
	scr_printf("Save this configuration? ");
	if (yesno()) {
		r = 1;
		for (a = 0; a < NUM_CONFIGS; a++) {
			r += 1 + strlen(sc[a]);
		}
		resp = (char *)calloc(1, r);
		if (!resp) {
			scr_printf("Can't save config - out of memory!\n");
			logoff(ipc, 1);
		}
		for (a = 0; a < NUM_CONFIGS; a++) {
			strcat(resp, sc[a]);
			strcat(resp, "\n");
		}
		r = CtdlIPCSetSystemConfig(ipc, resp, buf);
		if (r / 100 != 4) {
			scr_printf("%s\n", buf);
		}
		free(resp);

		r = CtdlIPCSetMessageExpirationPolicy(ipc, 2, site_expirepolicy, buf);
		if (r / 100 != 2) {
			scr_printf("%s\n", buf);
		}

		r = CtdlIPCSetMessageExpirationPolicy(ipc, 3, mbx_expirepolicy, buf);
		if (r / 100 != 2) {
			scr_printf("%s\n", buf);
		}

	}
    if (site_expirepolicy) free(site_expirepolicy);
    if (mbx_expirepolicy) free(mbx_expirepolicy);
}


/*
 * support function for do_internet_configuration()
 */
void get_inet_rec_type(CtdlIPC *ipc, char *buf) {
	int sel;

	keyopt(" <1> localhost      (Alias for this computer)\n");
	keyopt(" <2> smart host     (Forward all outbound mail to this host)\n");
	keyopt(" <3> fallback host  (Send mail to this host only if direct delivery fails)\n");
	keyopt(" <4> directory      (Consult the Global Address Book)\n");
	keyopt(" <5> SpamAssassin   (Address of SpamAssassin server)\n");
	keyopt(" <6> RBL            (domain suffix of spam hunting RBL)\n");
	keyopt(" <7> masq domains   (Domains as which users are allowed to masquerade)\n");
	keyopt(" <8> ClamAV         (Address of ClamAV clamd server)\n");
	sel = intprompt("Which one", 1, 1, 8);
	switch(sel) {
		case 1:	strcpy(buf, "localhost");
			return;
		case 2:	strcpy(buf, "smarthost");
			return;
		case 3:	strcpy(buf, "fallbackhost");
			return;
		case 4:	strcpy(buf, "directory");
			return;
		case 5:	strcpy(buf, "spamassassin");
			return;
		case 6:	strcpy(buf, "rbl");
			return;
		case 7:	strcpy(buf, "masqdomain");
			return;
		case 8:	strcpy(buf, "clamav");
			return;
	}
}


/*
 * Internet mail configuration
 */
void do_internet_configuration(CtdlIPC *ipc)
{
	char buf[256];
	char *resp = NULL;
	int num_recs = 0;
	char **recs = NULL;
	char ch;
	int i, j;
	int quitting = 0;
	int modified = 0;
	int r;
	
	r = CtdlIPCGetSystemConfigByType(ipc, INTERNETCFG, &resp, buf);
	if (r / 100 == 1) {
		while (!IsEmptyStr(resp)) {
			extract_token(buf, resp, 0, '\n', sizeof buf);
			remove_token(resp, 0, '\n');
			++num_recs;
			if (num_recs == 1) recs = malloc(sizeof(char *));
			else recs = realloc(recs, (sizeof(char *)) * num_recs);
			recs[num_recs-1] = malloc(strlen(buf) + 1);
			strcpy(recs[num_recs-1], buf);
		}
	}
	if (resp) free(resp);

	do {
		scr_printf("\n");
		color(BRIGHT_WHITE);
		scr_printf("###                    Host or domain                     Record type      \n");
		color(DIM_WHITE);
		scr_printf("--- -------------------------------------------------- --------------------\n");
		for (i=0; i<num_recs; ++i) {
		color(DIM_WHITE);
		scr_printf("%3d ", i+1);
		extract_token(buf, recs[i], 0, '|', sizeof buf);
		color(BRIGHT_CYAN);
		scr_printf("%-50s ", buf);
		extract_token(buf, recs[i], 1, '|', sizeof buf);
		color(BRIGHT_MAGENTA);
		scr_printf("%-20s\n", buf);
		color(DIM_WHITE);
		}

		ch = keymenu("", "<A>dd|<D>elete|<S>ave|<Q>uit");
		switch(ch) {
			case 'a':
				newprompt("Enter host name: ",
					buf, 50);
				striplt(buf);
				if (!IsEmptyStr(buf)) {
					++num_recs;
					if (num_recs == 1)
						recs = malloc(sizeof(char *));
					else recs = realloc(recs,
						(sizeof(char *)) * num_recs);
					strcat(buf, "|");
					get_inet_rec_type(ipc,
							&buf[strlen(buf)]);
					recs[num_recs-1] = strdup(buf);
				}
				modified = 1;
				break;
			case 'd':
				i = intprompt("Delete which one",
					1, 1, num_recs) - 1;
				free(recs[i]);
				--num_recs;
				for (j=i; j<num_recs; ++j)
					recs[j] = recs[j+1];
				modified = 1;
				break;
			case 's':
				r = 1;
				for (i = 0; i < num_recs; i++)
					r += 1 + strlen(recs[i]);
				resp = (char *)calloc(1, r);
				if (!resp) {
					scr_printf("Can't save config - out of memory!\n");
					logoff(ipc, 1);
				}
				if (num_recs) for (i = 0; i < num_recs; i++) {
					strcat(resp, recs[i]);
					strcat(resp, "\n");
				}
				r = CtdlIPCSetSystemConfigByType(ipc, INTERNETCFG, resp, buf);
				if (r / 100 != 4) {
					scr_printf("%s\n", buf);
				} else {
					scr_printf("Wrote %d records.\n", num_recs);
					modified = 0;
				}
                free(resp);
				break;
			case 'q':
				quitting = !modified || boolprompt(
					"Quit without saving", 0);
				break;
			default:
				break;
		}
	} while (!quitting);

	if (recs != NULL) {
		for (i=0; i<num_recs; ++i) free(recs[i]);
		free(recs);
	}
}



/*
 * Edit network configuration for room sharing, mailing lists, etc.
 */
void network_config_management(CtdlIPC *ipc, char *entrytype, char *comment)
{
	char filename[PATH_MAX];
	char changefile[PATH_MAX];
	int e_ex_code;
	pid_t editor_pid;
	int cksum;
	int b, i, tokens;
	char buf[1024];
	char instr[1024];
	char addr[1024];
	FILE *tempfp;
	FILE *changefp;
	char *listing = NULL;
	int r;

	if (IsEmptyStr(editor_path)) {
		scr_printf("You must have an external editor configured in"
			" order to use this function.\n");
		return;
	}

	CtdlMakeTempFileName(filename, sizeof filename);
	CtdlMakeTempFileName(changefile, sizeof changefile);

	tempfp = fopen(filename, "w");
	if (tempfp == NULL) {
		scr_printf("Cannot open %s: %s\n", filename, strerror(errno));
		return;
	}

	fprintf(tempfp, "# Configuration for room: %s\n", room_name);
	fprintf(tempfp, "# %s\n", comment);
	fprintf(tempfp, "# Specify one per line.\n"
			"\n\n");

	r = CtdlIPCGetRoomNetworkConfig(ipc, &listing, buf);
	if (r / 100 == 1) {
		while(listing && !IsEmptyStr(listing)) {
			extract_token(buf, listing, 0, '\n', sizeof buf);
			remove_token(listing, 0, '\n');
			extract_token(instr, buf, 0, '|', sizeof instr);
			if (!strcasecmp(instr, entrytype)) {
				tokens = num_tokens(buf, '|');
				for (i=1; i<tokens; ++i) {
					extract_token(addr, buf, i, '|', sizeof addr);
					fprintf(tempfp, "%s", addr);
					if (i < (tokens-1)) {
						fprintf(tempfp, "|");
					}
				}
				fprintf(tempfp, "\n");
			}
		}
	}
	if (listing) {
		free(listing);
		listing = NULL;
	}
	fclose(tempfp);

	e_ex_code = 1;	/* start with a failed exit code */
	stty_ctdl(SB_RESTORE);
	editor_pid = fork();
	cksum = file_checksum(filename);
	if (editor_pid == 0) {
		chmod(filename, 0600);
		putenv("WINDOW_TITLE=Network configuration");
		execlp(editor_path, editor_path, filename, NULL);
		exit(1);
	}
	if (editor_pid > 0) {
		do {
			e_ex_code = 0;
			b = ka_wait(&e_ex_code);
		} while ((b != editor_pid) && (b >= 0));
	editor_pid = (-1);
	stty_ctdl(0);
	}

	if (file_checksum(filename) == cksum) {
		scr_printf("*** No changes to save.\n");
		e_ex_code = 1;
	}

	if (e_ex_code == 0) { 		/* Save changes */
		changefp = fopen(changefile, "w");

		/* Load all netconfig entries that are *not* of the type we are editing */
		r = CtdlIPCGetRoomNetworkConfig(ipc, &listing, buf);
		if (r / 100 == 1) {
			while(listing && !IsEmptyStr(listing)) {
				extract_token(buf, listing, 0, '\n', sizeof buf);
				remove_token(listing, 0, '\n');
				extract_token(instr, buf, 0, '|', sizeof instr);
				if (strcasecmp(instr, entrytype)) {
					fprintf(changefp, "%s\n", buf);
				}
			}
		}
		if (listing) {
			free(listing);
			listing = NULL;
		}

		/* ...and merge that with the data we just edited */
		tempfp = fopen(filename, "r");
		while (fgets(buf, sizeof buf, tempfp) != NULL) {
			for (i=0; i<strlen(buf); ++i) {
				if (buf[i] == '#') buf[i] = 0;
			}
			striplt(buf);
			if (!IsEmptyStr(buf)) {
				fprintf(changefp, "%s|%s\n", entrytype, buf);
			}
		}
		fclose(tempfp);
		fclose(changefp);

		/* now write it to the server... */
		changefp = fopen(changefile, "r");
		if (changefp != NULL) {
			listing = load_message_from_file(changefp);
			if (listing) {
				r = CtdlIPCSetRoomNetworkConfig(ipc, listing, buf);
				free(listing);
				listing = NULL;
			}
			fclose(changefp);
		}
	}

	unlink(filename);		/* Delete the temporary files */
	unlink(changefile);
}


/*
 * IGnet node configuration
 */
void do_ignet_configuration(CtdlIPC *ipc) {
	char buf[SIZ];
	int num_recs = 0;
	char **recs = NULL;
	char ch;
	int i, j;
	int quitting = 0;
	int modified = 0;
	char *listing = NULL;
	int r;

	r = CtdlIPCGetSystemConfigByType(ipc, IGNETCFG, &listing, buf);
	if (r / 100 == 1) while (*listing && !IsEmptyStr(listing)) {
		extract_token(buf, listing, 0, '\n', sizeof buf);
		remove_token(listing, 0, '\n');

		++num_recs;
		if (num_recs == 1) recs = malloc(sizeof(char *));
		else recs = realloc(recs, (sizeof(char *)) * num_recs);
		recs[num_recs-1] = malloc(SIZ);
		strcpy(recs[num_recs-1], buf);
	}
	if (listing) free(listing);

	do {
		scr_printf("\n");
		color(BRIGHT_WHITE);
		scr_printf(	"### "
			"   Node          "
			"  Secret           "
			"          Host or IP             "
			"Port#\n");
		color(DIM_WHITE);
		scr_printf(	"--- "
			"---------------- "
			"------------------ "
			"-------------------------------- "
			"-----\n");
		for (i=0; i<num_recs; ++i) {
		color(DIM_WHITE);
		scr_printf("%3d ", i+1);
		extract_token(buf, recs[i], 0, '|', sizeof buf);
		color(BRIGHT_CYAN);
		scr_printf("%-16s ", buf);
		extract_token(buf, recs[i], 1, '|', sizeof buf);
		color(BRIGHT_MAGENTA);
		scr_printf("%-18s ", buf);
		extract_token(buf, recs[i], 2, '|', sizeof buf);
		color(BRIGHT_CYAN);
		scr_printf("%-32s ", buf);
		extract_token(buf, recs[i], 3, '|', sizeof buf);
		color(BRIGHT_MAGENTA);
		scr_printf("%-3s\n", buf);
		color(DIM_WHITE);
		}
		scr_printf("\n");

		ch = keymenu("", "<A>dd|<D>elete|<S>ave|<Q>uit");
		switch(ch) {
			case 'a':
				++num_recs;
				if (num_recs == 1)
					recs = malloc(sizeof(char *));
				else recs = realloc(recs,
					(sizeof(char *)) * num_recs);
				newprompt("Enter node name    : ", buf, 16);
				strcat(buf, "|");
				newprompt("Enter shared secret: ",
					&buf[strlen(buf)], 18);
				strcat(buf, "|");
				newprompt("Enter host or IP   : ",
					&buf[strlen(buf)], 32);
				strcat(buf, "|504");
				strprompt("Enter port number  : ",
					&buf[strlen(buf)-3], 5);
				recs[num_recs-1] = strdup(buf);
				modified = 1;
				break;
			case 'd':
				i = intprompt("Delete which one",
					1, 1, num_recs) - 1;
				free(recs[i]);
				--num_recs;
				for (j=i; j<num_recs; ++j)
					recs[j] = recs[j+1];
				modified = 1;
				break;
			case 's':
				r = 1;
				for (i = 0; i < num_recs; ++i)
					r += 1 + strlen(recs[i]);
				listing = (char*) calloc(1, r);
				if (!listing) {
					scr_printf("Can't save config - out of memory!\n");
					logoff(ipc, 1);
				}
				if (num_recs) for (i = 0; i < num_recs; ++i) {
					strcat(listing, recs[i]);
					strcat(listing, "\n");
				}
				r = CtdlIPCSetSystemConfigByType(ipc, IGNETCFG, listing, buf);
				if (r / 100 != 4) {
					scr_printf("%s\n", buf);
				} else {
					scr_printf("Wrote %d records.\n", num_recs);
					modified = 0;
				}
                free(listing);
				break;
			case 'q':
				quitting = !modified || boolprompt(
					"Quit without saving", 0);
				break;
			default:
				break;
		}
	} while (!quitting);

	if (recs != NULL) {
		for (i=0; i<num_recs; ++i) free(recs[i]);
		free(recs);
	}
}


/*
 * Filter list configuration
 */
void do_filterlist_configuration(CtdlIPC *ipc)
{
	char buf[SIZ];
	int num_recs = 0;
	char **recs = NULL;
	char ch;
	int i, j;
	int quitting = 0;
	int modified = 0;
	char *listing = NULL;
	int r;

	r = CtdlIPCGetSystemConfigByType(ipc, FILTERLIST, &listing, buf);
	if (r / 100 == 1) while (*listing && !IsEmptyStr(listing)) {
		extract_token(buf, listing, 0, '\n', sizeof buf);
		remove_token(listing, 0, '\n');

		++num_recs;
		if (num_recs == 1) recs = malloc(sizeof(char *));
		else recs = realloc(recs, (sizeof(char *)) * num_recs);
		recs[num_recs-1] = malloc(SIZ);
		strcpy(recs[num_recs-1], buf);
	}
	if (listing) free(listing);

	do {
		scr_printf("\n");
		color(BRIGHT_WHITE);
		scr_printf(	"### "
			"         User name           "
			"         Room name           "
			"    Node name    "
			"\n");
		color(DIM_WHITE);
		scr_printf(	"--- "
			"---------------------------- "
			"---------------------------- "
			"---------------- "
			"\n");
		for (i=0; i<num_recs; ++i) {
		color(DIM_WHITE);
		scr_printf("%3d ", i+1);
		extract_token(buf, recs[i], 0, '|', sizeof buf);
		color(BRIGHT_CYAN);
		scr_printf("%-28s ", buf);
		extract_token(buf, recs[i], 1, '|', sizeof buf);
		color(BRIGHT_MAGENTA);
		scr_printf("%-28s ", buf);
		extract_token(buf, recs[i], 2, '|', sizeof buf);
		color(BRIGHT_CYAN);
		scr_printf("%-16s\n", buf);
		extract_token(buf, recs[i], 3, '|', sizeof buf);
		color(DIM_WHITE);
		}

		ch = keymenu("", "<A>dd|<D>elete|<S>ave|<Q>uit");
		switch(ch) {
			case 'a':
				++num_recs;
				if (num_recs == 1)
					recs = malloc(sizeof(char *));
				else recs = realloc(recs,
					(sizeof(char *)) * num_recs);
				newprompt("Enter user name: ", buf, 28);
				strcat(buf, "|");
				newprompt("Enter room name: ",
					&buf[strlen(buf)], 28);
				strcat(buf, "|");
				newprompt("Enter node name: ",
					&buf[strlen(buf)], 16);
				strcat(buf, "|");
				recs[num_recs-1] = strdup(buf);
				modified = 1;
				break;
			case 'd':
				i = intprompt("Delete which one",
					1, 1, num_recs) - 1;
				free(recs[i]);
				--num_recs;
				for (j=i; j<num_recs; ++j)
					recs[j] = recs[j+1];
				modified = 1;
				break;
			case 's':
				r = 1;
				for (i = 0; i < num_recs; ++i)
					r += 1 + strlen(recs[i]);
				listing = (char*) calloc(1, r);
				if (!listing) {
					scr_printf("Can't save config - out of memory!\n");
					logoff(ipc, 1);
				}
				if (num_recs) for (i = 0; i < num_recs; ++i) {
					strcat(listing, recs[i]);
					strcat(listing, "\n");
				}
				r = CtdlIPCSetSystemConfigByType(ipc, FILTERLIST, listing, buf);
				if (r / 100 != 4) {
					scr_printf("%s\n", buf);
				} else {
					scr_printf("Wrote %d records.\n", num_recs);
					modified = 0;
				}
                free(listing);
				break;
			case 'q':
				quitting = !modified || boolprompt(
					"Quit without saving", 0);
				break;
			default:
				break;
		}
	} while (!quitting);

	if (recs != NULL) {
		for (i=0; i<num_recs; ++i) free(recs[i]);
		free(recs);
	}
}




/*
 * POP3 aggregation client configuration
 */
void do_pop3client_configuration(CtdlIPC *ipc)
{
	char buf[SIZ];
	int num_recs = 0;
	char **recs = NULL;
	char ch;
	int i, j;
	int quitting = 0;
	int modified = 0;
	char *listing = NULL;
	char *other_listing = NULL;
	int r;
	char instr[SIZ];

	r = CtdlIPCGetRoomNetworkConfig(ipc, &listing, buf);
	if (r / 100 == 1) {
		while(listing && !IsEmptyStr(listing)) {
			extract_token(buf, listing, 0, '\n', sizeof buf);
			remove_token(listing, 0, '\n');
			extract_token(instr, buf, 0, '|', sizeof instr);
			if (!strcasecmp(instr, "pop3client")) {

				++num_recs;
				if (num_recs == 1) recs = malloc(sizeof(char *));
				else recs = realloc(recs, (sizeof(char *)) * num_recs);
				recs[num_recs-1] = malloc(SIZ);
				strcpy(recs[num_recs-1], buf);

			}
		}
	}
	if (listing) {
		free(listing);
		listing = NULL;
	}

	do {
		scr_printf("\n");
		color(BRIGHT_WHITE);
		scr_printf(	"### "
			"      Remote POP3 host       "
			"         User name           "
			"Keep on server? "
			"Fetching inteval"
			"\n");
		color(DIM_WHITE);
		scr_printf(	"--- "
			"---------------------------- "
			"---------------------------- "
			"--------------- "
			"---------------- "
			"\n");
		for (i=0; i<num_recs; ++i) {
		color(DIM_WHITE);
		scr_printf("%3d ", i+1);

		extract_token(buf, recs[i], 1, '|', sizeof buf);
		color(BRIGHT_CYAN);
		scr_printf("%-28s ", buf);

		extract_token(buf, recs[i], 2, '|', sizeof buf);
		color(BRIGHT_MAGENTA);
		scr_printf("%-28s ", buf);

		color(BRIGHT_CYAN);
		scr_printf("%-15s ", (extract_int(recs[i], 4) ? "Yes" : "No") );
		color(BRIGHT_MAGENTA);
		scr_printf("%ld\n", extract_long(recs[i], 5) );
		color(DIM_WHITE);
		}

		ch = keymenu("", "<A>dd|<D>elete|<S>ave|<Q>uit");
		switch(ch) {
			case 'a':
				++num_recs;
				if (num_recs == 1) {
					recs = malloc(sizeof(char *));
				}
				else {
					recs = realloc(recs, (sizeof(char *)) * num_recs);
				}
				strcpy(buf, "pop3client|");
				newprompt("Enter host name: ", &buf[strlen(buf)], 28);
				strcat(buf, "|");
				newprompt("Enter user name: ", &buf[strlen(buf)], 28);
				strcat(buf, "|");
				newprompt("Enter password : ", &buf[strlen(buf)], 16);
				strcat(buf, "|");
				scr_printf("Keep messages on server instead of deleting them? ");
				sprintf(&buf[strlen(buf)], "%d", yesno());
				strcat(buf, "|");
				newprompt("Enter interval : ", &buf[strlen(buf)], 5);
				strcat(buf, "|");
				recs[num_recs-1] = strdup(buf);
				modified = 1;
				break;
			case 'd':
				i = intprompt("Delete which one",
					1, 1, num_recs) - 1;
				free(recs[i]);
				--num_recs;
				for (j=i; j<num_recs; ++j)
					recs[j] = recs[j+1];
				modified = 1;
				break;
			case 's':
				r = 1;
				for (i = 0; i < num_recs; ++i) {
					r += 1 + strlen(recs[i]);
				}
				listing = (char*) calloc(1, r);
				if (!listing) {
					scr_printf("Can't save config - out of memory!\n");
					logoff(ipc, 1);
				}
				if (num_recs) for (i = 0; i < num_recs; ++i) {
					strcat(listing, recs[i]);
					strcat(listing, "\n");
				}

				/* Retrieve all the *other* records for merging */
				r = CtdlIPCGetRoomNetworkConfig(ipc, &other_listing, buf);
				if (r / 100 == 1) {
					for (i=0; i<num_tokens(other_listing, '\n'); ++i) {
						extract_token(buf, other_listing, i, '\n', sizeof buf);
						if (strncasecmp(buf, "pop3client|", 11)) {
							listing = realloc(listing, strlen(listing) +
								strlen(buf) + 10);
							strcat(listing, buf);
							strcat(listing, "\n");
						}
					}
				}
				free(other_listing);
				r = CtdlIPCSetRoomNetworkConfig(ipc, listing, buf);
				free(listing);
				listing = NULL;

				if (r / 100 != 4) {
					scr_printf("%s\n", buf);
				} else {
					scr_printf("Wrote %d records.\n", num_recs);
					modified = 0;
				}
				quitting = 1;
				break;
			case 'q':
				quitting = !modified || boolprompt(
					"Quit without saving", 0);
				break;
			default:
				break;
		}
	} while (!quitting);

	if (recs != NULL) {
		for (i=0; i<num_recs; ++i) free(recs[i]);
		free(recs);
	}
}






/*
 * RSS feed retrieval client configuration
 */
void do_rssclient_configuration(CtdlIPC *ipc)
{
	char buf[SIZ];
	int num_recs = 0;
	char **recs = NULL;
	char ch;
	int i, j;
	int quitting = 0;
	int modified = 0;
	char *listing = NULL;
	char *other_listing = NULL;
	int r;
	char instr[SIZ];

	r = CtdlIPCGetRoomNetworkConfig(ipc, &listing, buf);
	if (r / 100 == 1) {
		while(listing && !IsEmptyStr(listing)) {
			extract_token(buf, listing, 0, '\n', sizeof buf);
			remove_token(listing, 0, '\n');
			extract_token(instr, buf, 0, '|', sizeof instr);
			if (!strcasecmp(instr, "rssclient")) {

				++num_recs;
				if (num_recs == 1) recs = malloc(sizeof(char *));
				else recs = realloc(recs, (sizeof(char *)) * num_recs);
				recs[num_recs-1] = malloc(SIZ);
				strcpy(recs[num_recs-1], buf);

			}
		}
	}
	if (listing) {
		free(listing);
		listing = NULL;
	}

	do {
		scr_printf("\n");
		color(BRIGHT_WHITE);
		scr_printf("### Feed URL\n");
		color(DIM_WHITE);
		scr_printf("--- "
			"---------------------------------------------------------------------------"
			"\n");
		
		for (i=0; i<num_recs; ++i) {
		color(DIM_WHITE);
		scr_printf("%3d ", i+1);

		extract_token(buf, recs[i], 1, '|', sizeof buf);
		color(BRIGHT_CYAN);
		scr_printf("%-75s\n", buf);

		color(DIM_WHITE);
		}

		ch = keymenu("", "<A>dd|<D>elete|<S>ave|<Q>uit");
		switch(ch) {
			case 'a':
				++num_recs;
				if (num_recs == 1) {
					recs = malloc(sizeof(char *));
				}
				else {
					recs = realloc(recs, (sizeof(char *)) * num_recs);
				}
				strcpy(buf, "rssclient|");
				newprompt("Enter feed URL: ", &buf[strlen(buf)], 75);
				strcat(buf, "|");
				recs[num_recs-1] = strdup(buf);
				modified = 1;
				break;
			case 'd':
				i = intprompt("Delete which one", 1, 1, num_recs) - 1;
				free(recs[i]);
				--num_recs;
				for (j=i; j<num_recs; ++j)
					recs[j] = recs[j+1];
				modified = 1;
				break;
			case 's':
				r = 1;
				for (i = 0; i < num_recs; ++i) {
					r += 1 + strlen(recs[i]);
				}
				listing = (char*) calloc(1, r);
				if (!listing) {
					scr_printf("Can't save config - out of memory!\n");
					logoff(ipc, 1);
				}
				if (num_recs) for (i = 0; i < num_recs; ++i) {
					strcat(listing, recs[i]);
					strcat(listing, "\n");
				}

				/* Retrieve all the *other* records for merging */
				r = CtdlIPCGetRoomNetworkConfig(ipc, &other_listing, buf);
				if (r / 100 == 1) {
					for (i=0; i<num_tokens(other_listing, '\n'); ++i) {
						extract_token(buf, other_listing, i, '\n', sizeof buf);
						if (strncasecmp(buf, "rssclient|", 10)) {
							listing = realloc(listing, strlen(listing) +
								strlen(buf) + 10);
							strcat(listing, buf);
							strcat(listing, "\n");
						}
					}
				}
				free(other_listing);
				r = CtdlIPCSetRoomNetworkConfig(ipc, listing, buf);
				free(listing);
				listing = NULL;

				if (r / 100 != 4) {
					scr_printf("%s\n", buf);
				} else {
					scr_printf("Wrote %d records.\n", num_recs);
					modified = 0;
				}
				quitting = 1;
				break;
			case 'q':
				quitting = !modified || boolprompt(
					"Quit without saving", 0);
				break;
			default:
				break;
		}
	} while (!quitting);

	if (recs != NULL) {
		for (i=0; i<num_recs; ++i) free(recs[i]);
		free(recs);
	}
}


