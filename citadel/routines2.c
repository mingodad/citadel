/* $Id$
 *
 * More client-side support functions.
 * Unlike routines.c, some of these DO use global variables.
 *
 */

#include "sysdep.h"
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
#include <setjmp.h>
#include <errno.h>
#include <stdarg.h>
#include "citadel.h"
#include "citadel_decls.h"
#include "routines2.h"
#include "routines.h"
#include "commands.h"
#include "tools.h"
#include "citadel_ipc.h"
#include "messages.h"
#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif
#include "screen.h"
#include "client_crypto.h"

extern char temp[];
extern char tempdir[];
extern char *axdefs[7];
extern long highest_msg_read;
extern long maxmsgnum;
extern unsigned room_flags;
extern int screenwidth;


int eopen(char *name, int mode)
{
	int ret;
	ret = open(name, mode);
	if (ret < 0) {
		err_printf("Cannot open file '%s', mode=%d, errno=%d\n",
			name, mode, errno);
		interr(errno);
	}
	return (ret);
}


int room_prompt(int qrflags)
{				/* return proper room prompt character */
	int a;
	a = '>';
	if (qrflags & QR_DIRECTORY)
		a = ']';
	if ((a == ']') && (qrflags & QR_NETWORK))
		a = '}';
	if ((a == '>') && (qrflags & QR_NETWORK))
		a = ')';
	return (a);
}

void entregis(void)
{				/* register with name and address */

	char buf[SIZ];
	char tmpname[SIZ];
	char tmpaddr[SIZ];
	char tmpcity[SIZ];
	char tmpstate[SIZ];
	char tmpzip[SIZ];
	char tmpphone[SIZ];
	char tmpemail[SIZ];
	char tmpcountry[SIZ];
	char diruser[SIZ];
	char dirnode[SIZ];
	char holdemail[SIZ];
	char *reg = NULL;
	int ok = 0;
	int r;				/* IPC response code */

	strcpy(tmpname, "");
	strcpy(tmpaddr, "");
	strcpy(tmpcity, "");
	strcpy(tmpstate, "");
	strcpy(tmpzip, "");
	strcpy(tmpphone, "");
	strcpy(tmpemail, "");
	strcpy(tmpcountry, "");

	r = CtdlIPCGetUserRegistration(NULL, &reg, buf);
	if (r / 100 == 1) {
		int a = 0;

		while (reg && strlen(reg) > 0) {

			extract_token(buf, reg, 0, '\n');
			remove_token(reg, 0, '\n');

			if (a == 2)
				strcpy(tmpname, buf);
			else if (a == 3)
				strcpy(tmpaddr, buf);
			else if (a == 4)
				strcpy(tmpcity, buf);
			else if (a == 5)
				strcpy(tmpstate, buf);
			else if (a == 6)
				strcpy(tmpzip, buf);
			else if (a == 7)
				strcpy(tmpphone, buf);
			else if (a == 9)
				strcpy(tmpemail, buf);
			else if (a == 10)
				strcpy(tmpcountry, buf);
			++a;
		}
	}
	strprompt("REAL name", tmpname, 29);
	strprompt("Address", tmpaddr, 24);
	strprompt("City/town", tmpcity, 14);
	strprompt("State/province", tmpstate, 2);
	strprompt("ZIP/Postal Code", tmpzip, 10);
	strprompt("Country", tmpcountry, 31);
	strprompt("Telephone number", tmpphone, 14);

	do {
		ok = 1;
		strcpy(holdemail, tmpemail);
		strprompt("Email address", tmpemail, 31);
		r = CtdlIPCDirectoryLookup(tmpemail, buf);
		if (r / 100 == 2) {
			extract_token(diruser, buf, 0, '@');
			extract_token(dirnode, buf, 1, '@');
			striplt(diruser);
			striplt(dirnode);
			if ((strcasecmp(diruser, fullname))
			   || (strcasecmp(dirnode, serv_info.serv_nodename))) {
				scr_printf(
					"\nYou can't use %s as your address.\n",
					tmpemail);
				scr_printf(
					"It is already in use by %s @ %s.\n",
					diruser, dirnode);
				ok = 0;
				strcpy(tmpemail, holdemail);
			}
		}
	} while (ok == 0);

	/* now send the registration info back to the server */
	reg = (char *)realloc(reg, 4096);	/* Overkill? */
	if (reg) {
		sprintf(reg, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
			tmpname, tmpaddr, tmpcity, tmpstate,
			tmpzip, tmpphone, tmpemail, tmpcountry);
		r = CtdlIPCSetRegistration(reg, buf);
		if (r / 100 != 4)
			scr_printf("%s\n", buf);
		free(reg);
	}
	scr_printf("\n");
}

void updatels(void)
{				/* make all messages old in current room */
	char buf[SIZ];
	int r;				/* IPC response code */

	if (rc_alt_semantics) {
		if (maxmsgnum == highest_msg_read == 0) {
			/* err_printf("maxmsgnum == highest_msg_read == 0\n"); */
			return;
		}
		r = CtdlIPCSetLastRead((maxmsgnum > highest_msg_read) ?
				 maxmsgnum : highest_msg_read, buf);
	} else {
		r = CtdlIPCSetLastRead(0, buf);
	}
	if (r / 100 != 2)
		scr_printf("%s\n", buf);
}

/*
 * only make messages old in this room that have been read
 */
void updatelsa(void)
{
	char buf[SIZ];
	int r;				/* IPC response code */

	r = CtdlIPCSetLastRead(highest_msg_read, buf);
	if (r / 100 != 2)
		scr_printf("%s\n", &buf[4]);
}


/*
 * This routine completes a client upload
 */
void do_upload(int fd)
{
	char buf[SIZ];
	char tbuf[4096];
	long transmitted_bytes, total_bytes;
	int bytes_to_send;
	int bytes_expected;

	/* learn the size of the file */
	total_bytes = lseek(fd, 0L, 2);
	lseek(fd, 0L, 0);

	transmitted_bytes = 0L;
	progress(transmitted_bytes, total_bytes);
	do {
		bytes_to_send = read(fd, tbuf, 4096);
		if (bytes_to_send > 0) {
			snprintf(buf, sizeof buf, "WRIT %d", bytes_to_send);
			serv_puts(buf);
			serv_gets(buf);
			if (buf[0] == '7') {
				bytes_expected = atoi(&buf[4]);
				serv_write(tbuf, bytes_expected);
			} else {
				scr_printf("%s\n", &buf[4]);
			}
		}
		transmitted_bytes = transmitted_bytes + (long) bytes_to_send;
		progress(transmitted_bytes, total_bytes);
	} while (bytes_to_send > 0);

	/* close the upload file, locally and at the server */
	close(fd);
	serv_puts("UCLS 1");
	serv_gets(buf);
	scr_printf("%s\n", &buf[4]);
}


/*
 * client-based uploads (for users with their own clientware)
 */
void cli_upload(void)
{
	char flnm[SIZ];
	char desc[151];
	char buf[SIZ];
	char tbuf[SIZ];
	int a;
	int fd;

	if ((room_flags & QR_UPLOAD) == 0) {
		scr_printf("*** You cannot upload to this room.\n");
		return;
	}
	newprompt("File to be uploaded: ", flnm, 55);
	fd = open(flnm, O_RDONLY);
	if (fd < 0) {
		scr_printf("Cannot open '%s': %s\n", flnm, strerror(errno));
		return;
	}
	scr_printf("Enter a description of this file:\n");
	newprompt(": ", desc, 75);

	/* keep generating filenames in hope of finding a unique one */
	a = 0;
	do {
		if (a == 10)
			return;	/* fail if tried 10 times */
		strcpy(buf, flnm);
		while ((strlen(buf) > 0) && (haschar(buf, '/')))
			strcpy(buf, &buf[1]);
		if (a > 0) {
			size_t tmp = strlen(buf);
			snprintf(&buf[tmp], sizeof buf - tmp, "%d", a);
		}
		snprintf(tbuf, sizeof tbuf, "UOPN %s|%s", buf, desc);
		serv_puts(tbuf);
		serv_gets(buf);
		if (buf[0] != '2')
			scr_printf("%s\n", &buf[4]);
		++a;
	} while (buf[0] != '2');

	/* at this point we have an open upload file at the server */
	do_upload(fd);
}


/*
 * Function used for various image upload commands
 */
void cli_image_upload(char *keyname)
{
	char flnm[SIZ];
	char buf[SIZ];
	int fd;

	snprintf(buf, sizeof buf, "UIMG 0|%s", keyname);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '2') {
		scr_printf("%s\n", &buf[4]);
		return;
	}
	newprompt("Image file to be uploaded: ", flnm, 55);
	fd = open(flnm, O_RDONLY);
	if (fd < 0) {
		scr_printf("Cannot open '%s': %s\n", flnm, strerror(errno));
		return;
	}
	snprintf(buf, sizeof buf, "UIMG 1|%s", keyname);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '2') {
		scr_printf("%s\n", &buf[4]);
		return;
	}
	do_upload(fd);
}


/*
 * protocol-based uploads (Xmodem, Ymodem, Zmodem)
 */
void upload(int c)
{				/* c = upload mode */
	char flnm[SIZ];
	char desc[151];
	char buf[SIZ];
	char tbuf[4096];
	int xfer_pid;
	int a, b;
	FILE *fp, *lsfp;
	int fd;

	if ((room_flags & QR_UPLOAD) == 0) {
		scr_printf("*** You cannot upload to this room.\n");
		return;
	}
	/* we don't need a filename when receiving batch y/z modem */
	if ((c == 2) || (c == 3))
		strcpy(flnm, "x");
	else
		newprompt("Enter filename: ", flnm, 15);

	for (a = 0; a < strlen(flnm); ++a)
		if ((flnm[a] == '/') || (flnm[a] == '\\') || (flnm[a] == '>')
		    || (flnm[a] == '?') || (flnm[a] == '*')
		    || (flnm[a] == ';') || (flnm[a] == '&'))
			flnm[a] = '_';

	newprompt("Enter a short description of the file:\n: ", desc, 150);

	/* create a temporary directory... */
	if (mkdir(tempdir, 0700) != 0) {
		scr_printf("*** Could not create temporary directory %s: %s\n",
		       tempdir, strerror(errno));
		return;
	}
	/* now do the transfer ... in a separate process */
	xfer_pid = fork();
	if (xfer_pid == 0) {
		chdir(tempdir);
		switch (c) {
		case 0:
			sttybbs(0);
			scr_printf("Receiving %s - press Ctrl-D to end.\n", flnm);
			fp = fopen(flnm, "w");
			do {
				b = inkey();
				if (b == 13) {
					b = 10;
				}
				if (b != 4) {
					scr_printf("%c", b);
					putc(b, fp);
				}
			} while (b != 4);
			fclose(fp);
			exit(0);
		case 1:
			screen_reset();
			sttybbs(3);
			execlp("rx", "rx", flnm, NULL);
			exit(1);
		case 2:
			screen_reset();
			sttybbs(3);
			execlp("rb", "rb", NULL);
			exit(1);
		case 3:
			screen_reset();
			sttybbs(3);
			execlp("rz", "rz", NULL);
			exit(1);
		}
	} else
		do {
			b = ka_wait(&a);
		} while ((b != xfer_pid) && (b != (-1)));
	sttybbs(0);
	screen_set();

	if (a != 0) {
		scr_printf("\r*** Transfer unsuccessful.\n");
		nukedir(tempdir);
		return;
	}
	scr_printf("\r*** Transfer successful.  Sending file(s) to server...\n");
	snprintf(buf, sizeof buf, "cd %s; ls", tempdir);
	lsfp = popen(buf, "r");
	if (lsfp != NULL) {
		while (fgets(flnm, sizeof flnm, lsfp) != NULL) {
			flnm[strlen(flnm) - 1] = 0;
			snprintf(buf, sizeof buf, "%s/%s", tempdir, flnm);
			fd = open(buf, O_RDONLY);
			if (fd >= 0) {
				a = 0;
				do {
					snprintf(buf, sizeof buf, "UOPN %s|%s", flnm, desc);
					if (a > 0) {
						size_t tmp = strlen(buf);
						snprintf(&buf[tmp],
							sizeof buf - tmp,
							".%d", a);
					}
					++a;
					serv_puts(buf);
					serv_gets(buf);
				} while ((buf[0] != '2') && (a < 100));
				if (buf[0] == '2')
					do {
						a = read(fd, tbuf, 4096);
						if (a > 0) {
							snprintf(buf, sizeof buf, "WRIT %d", a);
							serv_puts(buf);
							serv_gets(buf);
							if (buf[0] == '7')
								serv_write(tbuf, a);
						}
					} while (a > 0);
				close(fd);
				serv_puts("UCLS 1");
				serv_gets(buf);
				scr_printf("%s\n", &buf[4]);
			}
		}
		pclose(lsfp);
	}
	nukedir(tempdir);
}

/* 
 * validate a user
 */
void val_user(char *user, int do_validate)
{
	int a;
	char cmd[SIZ];
	char buf[SIZ];
	int ax = 0;
	int r;				/* IPC response code */

	snprintf(cmd, sizeof cmd, "GREG %s", user);
	serv_puts(cmd);
	serv_gets(cmd);
	if (cmd[0] == '1') {
		a = 0;
		do {
			serv_gets(buf);
			++a;
			if (a == 1)
				scr_printf("User #%s - %s  ", buf, &cmd[4]);
			if (a == 2)
				scr_printf("PW: %s\n", buf);
			if (a == 3)
				scr_printf("%s\n", buf);
			if (a == 4)
				scr_printf("%s\n", buf);
			if (a == 5)
				scr_printf("%s, ", buf);
			if (a == 6)
				scr_printf("%s ", buf);
			if (a == 7)
				scr_printf("%s\n", buf);
			if (a == 8)
				scr_printf("%s\n", buf);
			if (a == 9)
				ax = atoi(buf);
			if (a == 10)
				scr_printf("%s\n", buf);
			if (a == 11)
				scr_printf("%s\n", buf);
		} while (strcmp(buf, "000"));
		scr_printf("Current access level: %d (%s)\n", ax, axdefs[ax]);
	} else {
		scr_printf("%-30s\n%s\n", user, &cmd[4]);
	}

	if (do_validate) {
		/* now set the access level */
		ax = intprompt("Access level", ax, 0, 6);
		r = CtdlIPCValidateUser(user, ax, cmd);
		if (r / 100 != 2)
			scr_printf("%s\n", cmd);
	}
	scr_printf("\n");
}


void validate(void)
{				/* validate new users */
	char cmd[SIZ];
	char buf[SIZ];
	int finished = 0;
	int r;				/* IPC response code */

	do {
		r = CtdlIPCNextUnvalidatedUser(cmd);
		if (r / 100 != 3)
			finished = 1;
		if (r / 100 == 2)
			scr_printf("%s\n", cmd);
		if (r / 100 == 3) {
			extract(buf, cmd, 0);
			val_user(buf, 1);
		}
	} while (finished == 0);
}

void subshell(void)
{
	int a, b;
	a = fork();
	if (a == 0) {
		screen_reset();
		sttybbs(SB_RESTORE);
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		execlp(getenv("SHELL"), getenv("SHELL"), NULL);
		err_printf("Could not open a shell: %s\n", strerror(errno));
		exit(errno);
	}
	do {
		b = ka_wait(NULL);
	} while ((a != b) && (a != (-1)));
	sttybbs(0);
	screen_set();
}

/*
 * <.A>ide <F>ile <D>elete command
 */
void deletefile(void)
{
	char filename[32];
	char buf[SIZ];

	newprompt("Filename: ", filename, 31);
	if (strlen(filename) == 0)
		return;
	CtdlIPCDeleteFile(filename, buf);
	err_printf("%s\n", buf);
}

/*
 * <.A>ide <F>ile <S>end command
 */
void netsendfile(void)
{
	char filename[32], destsys[20], buf[SIZ];

	newprompt("Filename: ", filename, 31);
	if (strlen(filename) == 0)
		return;
	newprompt("System to send to: ", destsys, 19);
	CtdlIPCNetSendFile(filename, destsys, buf);
	err_printf("%s\n", buf);
	return;
}

/*
 * <.A>ide <F>ile <M>ove command
 */
void movefile(void)
{
	char filename[64];
	char newroom[ROOMNAMELEN];
	char buf[SIZ];

	newprompt("Filename: ", filename, 63);
	if (strlen(filename) == 0)
		return;
	newprompt("Enter target room: ", newroom, ROOMNAMELEN - 1);
	CtdlIPCMoveFile(filename, newroom, buf);
	err_printf("%s\n", buf);
}


/* 
 * list of users who have filled out a bio
 */
void list_bio(void)
{
	char buf[SIZ];
	int pos = 1;

	serv_puts("LBIO");
	serv_gets(buf);
	if (buf[0] != '1') {
		pprintf("%s\n", &buf[4]);
		return;
	}
	while (serv_gets(buf), strcmp(buf, "000")) {
		if ((pos + strlen(buf) + 5) > screenwidth) {
			pprintf("\n");
			pos = 1;
		}
		pprintf("%s, ", buf);
		pos = pos + strlen(buf) + 2;
	}
	pprintf("%c%c  \n\n", 8, 8);
}


/*
 * read bio
 */
void read_bio(void)
{
	char who[SIZ];
	char buf[SIZ];

	do {
		newprompt("Read bio for who ('?' for list) : ", who, 25);
		pprintf("\n");
		if (!strcmp(who, "?"))
			list_bio();
	} while (!strcmp(who, "?"));
	snprintf(buf, sizeof buf, "RBIO %s", who);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '1') {
		pprintf("%s\n", &buf[4]);
		return;
	}
	while (serv_gets(buf), strcmp(buf, "000")) {
		pprintf("%s\n", buf);
	}
}


/* 
 * General system configuration command
 */
void do_system_configuration(void)
{
	char buf[SIZ];
	char sc[31][SIZ];
	int expire_mode = 0;
	int expire_value = 0;
	int a;
	int logpages = 0;

	/* Clear out the config buffers */
	memset(&sc[0][0], 0, sizeof(sc));

	/* Fetch the current config */
	serv_puts("CONF get");
	serv_gets(buf);
	if (buf[0] == '1') {
		a = 0;
		while (serv_gets(buf), strcmp(buf, "000")) {
			if (a < 31) {
				strcpy(&sc[a][0], buf);
			}
			++a;
		}
	}
	/* Fetch the expire policy (this will silently fail on old servers,
	 * resulting in "default" policy)
	 */
	serv_puts("GPEX site");
	serv_gets(buf);
	if (buf[0] == '2') {
		expire_mode = extract_int(&buf[4], 0);
		expire_value = extract_int(&buf[4], 1);
	}


	/* Identification parameters */

	strprompt("Node name", &sc[0][0], 15);
	strprompt("Fully qualified domain name", &sc[1][0], 63);
	strprompt("Human readable node name", &sc[2][0], 20);
	strprompt("Modem dialup number", &sc[3][0], 15);
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
	snprintf(sc[30], sizeof sc[29], "%d", (boolprompt(
	      "Allow system Aides access to user mailboxes",
						     atoi(&sc[30][0]))));

	if (strlen(&sc[18][0]) > 0) logpages = 1;
	else logpages = 0;
	logpages = boolprompt("Log all pages", logpages);
	if (logpages) {
		strprompt("Name of logging room", &sc[18][0], ROOMNAMELEN);
	}
	else {
		sc[18][0] = 0;
	}


	/* Server tuning */

	strprompt("Server connection idle timeout (in seconds)", &sc[5][0], 4);
	strprompt("Maximum concurrent sessions", &sc[14][0], 4);
	strprompt("Maximum message length", &sc[20][0], 20);
	strprompt("Minimum number of worker threads", &sc[21][0], 3);
	strprompt("Maximum number of worker threads", &sc[22][0], 3);

	/* no longer applicable ... deprecated
	strprompt("Server-to-server networking password", &sc[15][0], 19);
	*/

	strprompt("How often to run network jobs (in seconds)", &sc[28][0], 5);
	strprompt("SMTP server port (-1 to disable)", &sc[24][0], 5);
	strprompt("POP3 server port (-1 to disable)", &sc[23][0], 5);
	strprompt("IMAP server port (-1 to disable)", &sc[27][0], 5);

	/* Expiry settings */
	strprompt("Default user purge time (days)", &sc[16][0], 5);
	strprompt("Default room purge time (days)", &sc[17][0], 5);

	/* Angels and demons dancing in my head... */
	do {
		snprintf(buf, sizeof buf, "%d", expire_mode);
		strprompt("System default message expire policy (? for list)",
			  buf, 1);
		if (buf[0] == '?') {
			scr_printf("\n"
				"1. Never automatically expire messages\n"
				"2. Expire by message count\n"
				"3. Expire by message age\n");
		}
	} while ((buf[0] < 49) || (buf[0] > 51));
	expire_mode = buf[0] - 48;

	/* ...lunatics and monsters underneath my bed */
	if (expire_mode == 2) {
		snprintf(buf, sizeof buf, "%d", expire_value);
		strprompt("Keep how many messages online?", buf, 10);
		expire_value = atol(buf);
	}
	if (expire_mode == 3) {
		snprintf(buf, sizeof buf, "%d", expire_value);
		strprompt("Keep messages for how many days?", buf, 10);
		expire_value = atol(buf);
	}
	/* Save it */
	scr_printf("Save this configuration? ");
	if (yesno()) {
		serv_puts("CONF set");
		serv_gets(buf);
		if (buf[0] == '4') {
			for (a = 0; a < 31; ++a)
				serv_puts(&sc[a][0]);
			serv_puts("000");
		}
		snprintf(buf, sizeof buf, "SPEX site|%d|%d",
			 expire_mode, expire_value);
		serv_puts(buf);
		serv_gets(buf);
	}
}


/*
 * support function for do_internet_configuration()
 */
void get_inet_rec_type(char *buf) {
	int sel;

	keyopt(" <1> localhost      (Alias for this computer)\n");
	keyopt(" <2> gateway domain (Domain for all Citadel systems)\n");
	keyopt(" <3> smart-host     (Forward all outbound mail to this host)\n");
	keyopt(" <4> directory      (Consult the Global Address Book)\n");
	keyopt(" <5> SpamAssassin   (Address of SpamAssassin server)\n");
	sel = intprompt("Which one", 1, 1, 5);
	switch(sel) {
		case 1:	strcpy(buf, "localhost");
			return;
		case 2:	strcpy(buf, "gatewaydomain");
			return;
		case 3:	strcpy(buf, "smarthost");
			return;
		case 4:	strcpy(buf, "directory");
			return;
		case 5:	strcpy(buf, "spamassassin");
			return;
	}
}


/*
 * Internet mail configuration
 */
void do_internet_configuration(void) {
	char buf[SIZ];
	int num_recs = 0;
	char **recs = NULL;
	char ch;
	int badkey;
	int i, j;
	int quitting = 0;
	

	snprintf(buf, sizeof buf, "CONF getsys|%s", INTERNETCFG);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] == '1') while (serv_gets(buf), strcmp(buf, "000")) {
		++num_recs;
		if (num_recs == 1) recs = malloc(sizeof(char *));
		else recs = realloc(recs, (sizeof(char *)) * num_recs);
		recs[num_recs-1] = malloc(SIZ);
		strcpy(recs[num_recs-1], buf);
	}

	do {
		scr_printf("\n");
		color(BRIGHT_WHITE);
		scr_printf("###                    Host or domain                     Record type      \n");
		color(DIM_WHITE);
		scr_printf("--- -------------------------------------------------- --------------------\n");
		for (i=0; i<num_recs; ++i) {
		color(DIM_WHITE);
		scr_printf("%3d ", i+1);
		extract(buf, recs[i], 0);
		color(BRIGHT_CYAN);
		scr_printf("%-50s ", buf);
		extract(buf, recs[i], 1);
		color(BRIGHT_MAGENTA);
		scr_printf("%-20s\n", buf);
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
				newprompt("Enter host name: ",
					buf, 50);
				strcat(buf, "|");
				get_inet_rec_type(&buf[strlen(buf)]);
				recs[num_recs-1] = strdup(buf);
				break;
			case 'd':
				i = intprompt("Delete which one",
					1, 1, num_recs) - 1;
				free(recs[i]);
				--num_recs;
				for (j=i; j<num_recs; ++j)
					recs[j] = recs[j+1];
				break;
			case 's':
				snprintf(buf, sizeof buf, "CONF putsys|%s",
					INTERNETCFG);
				serv_puts(buf);
				serv_gets(buf);
				if (buf[0] == '4') {
					for (i=0; i<num_recs; ++i) {
						serv_puts(recs[i]);
					}
					serv_puts("000");
				}
				else {
					scr_printf("%s\n", &buf[4]);
				}
				quitting = 1;
				break;
			case 'q':
				quitting = boolprompt(
					"Quit without saving", 0);
				break;
			default:
				badkey = 1;
		}
	} while (quitting == 0);

	if (recs != NULL) {
		for (i=0; i<num_recs; ++i) free(recs[i]);
		free(recs);
	}
}



/*
 * Edit network configuration for room sharing, mailing lists, etc.
 */
void network_config_management(char *entrytype, char *comment) {
	char filename[PATH_MAX];
	char changefile[PATH_MAX];
	int e_ex_code;
	pid_t editor_pid;
	int cksum;
	int b, i;
	char buf[SIZ];
	char instr[SIZ];
	char addr[SIZ];
	FILE *tempfp;
	FILE *changefp;

	if (strlen(editor_path) == 0) {
		scr_printf("You must have an external editor configured in"
			" order to use this function.\n");
		return;
	}

	snprintf(filename, sizeof filename, "%s.listedit", tmpnam(NULL));
	snprintf(changefile, sizeof changefile, "%s.listedit", tmpnam(NULL));

	tempfp = fopen(filename, "w");
	if (tempfp == NULL) {
		err_printf("Cannot open %s: %s\n", filename, strerror(errno));
		return;
	}

	fprintf(tempfp, "# Configuration for room: %s\n", room_name);
	fprintf(tempfp, "# %s\n", comment);
	fprintf(tempfp, "# Specify one per line.\n"
			"\n\n");

	serv_puts("GNET");
	serv_gets(buf);
	if (buf[0] == '1') {
		while(serv_gets(buf), strcmp(buf, "000")) {
			extract(instr, buf, 0);
			if (!strcasecmp(instr, entrytype)) {
				extract(addr, buf, 1);
				fprintf(tempfp, "%s\n", addr);
			}
		}
	}
	fclose(tempfp);

	e_ex_code = 1;	/* start with a failed exit code */
	editor_pid = fork();
	cksum = file_checksum(filename);
	if (editor_pid == 0) {
		chmod(filename, 0600);
		screen_reset();
		sttybbs(SB_RESTORE);
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
	sttybbs(0);
	screen_set();
	}

	if (file_checksum(filename) == cksum) {
		err_printf("*** Not saving changes.\n");
		e_ex_code = 1;
	}

	if (e_ex_code == 0) { 		/* Save changes */
		changefp = fopen(changefile, "w");
		serv_puts("GNET");
		serv_gets(buf);
		if (buf[0] == '1') {
			while(serv_gets(buf), strcmp(buf, "000")) {
				extract(instr, buf, 0);
				if (strcasecmp(instr, entrytype)) {
					fprintf(changefp, "%s\n", buf);
				}
			}
		}
		tempfp = fopen(filename, "r");
		while (fgets(buf, sizeof buf, tempfp) != NULL) {
			for (i=0; i<strlen(buf); ++i) {
				if (buf[i] == '#') buf[i] = 0;
			}
			striplt(buf);
			if (strlen(buf) > 0) {
				fprintf(changefp, "%s|%s\n", entrytype, buf);
			}
		}
		fclose(tempfp);
		fclose(changefp);

		/* now write it to the server... */
		serv_puts("SNET");
		serv_gets(buf);
		if (buf[0] == '4') {
			changefp = fopen(changefile, "r");
			if (changefp != NULL) {
				while (fgets(buf, sizeof buf,
				       changefp) != NULL) {
					buf[strlen(buf) - 1] = 0;
					serv_puts(buf);
				}
				fclose(changefp);
			}
			serv_puts("000");
		}
	}

	unlink(filename);		/* Delete the temporary files */
	unlink(changefile);
}


/*
 * IGnet node configuration
 */
void do_ignet_configuration(void) {
	char buf[SIZ];
	int num_recs = 0;
	char **recs = NULL;
	char ch;
	int badkey;
	int i, j;
	int quitting = 0;
	

	snprintf(buf, sizeof buf, "CONF getsys|%s", IGNETCFG);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] == '1') while (serv_gets(buf), strcmp(buf, "000")) {
		++num_recs;
		if (num_recs == 1) recs = malloc(sizeof(char *));
		else recs = realloc(recs, (sizeof(char *)) * num_recs);
		recs[num_recs-1] = malloc(SIZ);
		strcpy(recs[num_recs-1], buf);
	}

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
		extract(buf, recs[i], 0);
		color(BRIGHT_CYAN);
		scr_printf("%-16s ", buf);
		extract(buf, recs[i], 1);
		color(BRIGHT_MAGENTA);
		scr_printf("%-18s ", buf);
		extract(buf, recs[i], 2);
		color(BRIGHT_CYAN);
		scr_printf("%-32s ", buf);
		extract(buf, recs[i], 3);
		color(BRIGHT_MAGENTA);
		scr_printf("%-3s\n", buf);
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
				break;
			case 'd':
				i = intprompt("Delete which one",
					1, 1, num_recs) - 1;
				free(recs[i]);
				--num_recs;
				for (j=i; j<num_recs; ++j)
					recs[j] = recs[j+1];
				break;
			case 's':
				snprintf(buf, sizeof buf, "CONF putsys|%s", IGNETCFG);
				serv_puts(buf);
				serv_gets(buf);
				if (buf[0] == '4') {
					for (i=0; i<num_recs; ++i) {
						serv_puts(recs[i]);
					}
					serv_puts("000");
				}
				else {
					scr_printf("%s\n", &buf[4]);
				}
				quitting = 1;
				break;
			case 'q':
				quitting = boolprompt(
					"Quit without saving", 0);
				break;
			default:
				badkey = 1;
		}
	} while (quitting == 0);

	if (recs != NULL) {
		for (i=0; i<num_recs; ++i) free(recs[i]);
		free(recs);
	}
}

/*
 * Filter list configuration
 */
void do_filterlist_configuration(void) {
	char buf[SIZ];
	int num_recs = 0;
	char **recs = NULL;
	char ch;
	int badkey;
	int i, j;
	int quitting = 0;
	

	snprintf(buf, sizeof buf, "CONF getsys|%s", FILTERLIST);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] == '1') while (serv_gets(buf), strcmp(buf, "000")) {
		++num_recs;
		if (num_recs == 1) recs = malloc(sizeof(char *));
		else recs = realloc(recs, (sizeof(char *)) * num_recs);
		recs[num_recs-1] = malloc(SIZ);
		strcpy(recs[num_recs-1], buf);
	}

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
		extract(buf, recs[i], 0);
		color(BRIGHT_CYAN);
		scr_printf("%-28s ", buf);
		extract(buf, recs[i], 1);
		color(BRIGHT_MAGENTA);
		scr_printf("%-28s ", buf);
		extract(buf, recs[i], 2);
		color(BRIGHT_CYAN);
		scr_printf("%-16s\n", buf);
		extract(buf, recs[i], 3);
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
				break;
			case 'd':
				i = intprompt("Delete which one",
					1, 1, num_recs) - 1;
				free(recs[i]);
				--num_recs;
				for (j=i; j<num_recs; ++j)
					recs[j] = recs[j+1];
				break;
			case 's':
				snprintf(buf, sizeof buf, "CONF putsys|%s", FILTERLIST);
				serv_puts(buf);
				serv_gets(buf);
				if (buf[0] == '4') {
					for (i=0; i<num_recs; ++i) {
						serv_puts(recs[i]);
					}
					serv_puts("000");
				}
				else {
					scr_printf("%s\n", &buf[4]);
				}
				quitting = 1;
				break;
			case 'q':
				quitting = boolprompt(
					"Quit without saving", 0);
				break;
			default:
				badkey = 1;
		}
	} while (quitting == 0);

	if (recs != NULL) {
		for (i=0; i<num_recs; ++i) free(recs[i]);
		free(recs);
	}
}


