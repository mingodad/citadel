/*
 * More client-side support functions.
 * Unlike routines.c, some of these DO use global variables.
 *
 * Copyright (c) 1987-2016 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
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
#include "citadel_ipc.h"
#include "citadel_decls.h"
#include "routines2.h"
#include "routines.h"
#include "commands.h"
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


/*
int eopen(char *name, int mode)
{
	int ret;
	ret = open(name, mode);
	if (ret < 0) {
		scr_printf("Cannot open file '%s', mode=%d, errno=%d\n",
			name, mode, errno);
		interr(errno);
	}
	return (ret);
}
*/


int room_prompt(unsigned int qrflags)
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

void entregis(CtdlIPC *ipc)
{				/* register with name and address */

	char buf[SIZ];
	char tmpname[30];
	char tmpaddr[25];
	char tmpcity[15];
	char tmpstate[3];
	char tmpzip[11];
	char tmpphone[15];
	char tmpemail[SIZ];
	char tmpcountry[32];
	char diruser[256];
	char dirnode[256];
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

	r = CtdlIPCGetUserRegistration(ipc, NULL, &reg, buf);
	if (r / 100 == 1) {
		int a = 0;

		while (reg && !IsEmptyStr(reg)) {

			extract_token(buf, reg, 0, '\n', sizeof buf);
			remove_token(reg, 0, '\n');

			if (a == 2)
				safestrncpy(tmpname, buf, sizeof tmpname);
			else if (a == 3)
				safestrncpy(tmpaddr, buf, sizeof tmpaddr);
			else if (a == 4)
				safestrncpy(tmpcity, buf, sizeof tmpcity);
			else if (a == 5)
				safestrncpy(tmpstate, buf, sizeof tmpstate);
			else if (a == 6)
				safestrncpy(tmpzip, buf, sizeof tmpzip);
			else if (a == 7)
				safestrncpy(tmpphone, buf, sizeof tmpphone);
			else if (a == 9)
				safestrncpy(tmpemail, buf, sizeof tmpemail);
			else if (a == 10)
				safestrncpy(tmpcountry, buf, sizeof tmpcountry);
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
		safestrncpy(holdemail, tmpemail, sizeof holdemail);
		strprompt("Email address", tmpemail, 31);
		r = CtdlIPCDirectoryLookup(ipc, tmpemail, buf);
		if (r / 100 == 2) {
			extract_token(diruser, buf, 0, '@', sizeof diruser);
			extract_token(dirnode, buf, 1, '@', sizeof dirnode);
			striplt(diruser);
			striplt(dirnode);
			if ((strcasecmp(diruser, fullname))
			   || (strcasecmp(dirnode, ipc->ServInfo.nodename))) {
				scr_printf(
					"\nYou can't use %s as your address.\n",
					tmpemail);
				scr_printf(
					"It is already in use by %s @ %s.\n",
					diruser, dirnode);
				ok = 0;
				safestrncpy(tmpemail, holdemail, sizeof tmpemail);
			}
		}
	} while (ok == 0);

	/* now send the registration info back to the server */
	reg = (char *)realloc(reg, SIZ);
	if (reg) {
		sprintf(reg, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
			tmpname, tmpaddr, tmpcity, tmpstate,
			tmpzip, tmpphone, tmpemail, tmpcountry);
		r = CtdlIPCSetRegistration(ipc, reg, buf);
		if (r / 100 != 4)
			scr_printf("%s\n", buf);
		free(reg);
	}
	scr_printf("\n");
}

void updatels(CtdlIPC *ipc)
{				/* make all messages old in current room */
	char buf[256];
	int r;				/* IPC response code */

	r = CtdlIPCSetLastRead(ipc, (maxmsgnum > highest_msg_read) ?  maxmsgnum : highest_msg_read, buf);

	if (r / 100 != 2)
		scr_printf("%s\n", buf);
}

/*
 * only make messages old in this room that have been read
 */
void updatelsa(CtdlIPC *ipc)
{
	char buf[256];
	int r;				/* IPC response code */

	r = CtdlIPCSetLastRead(ipc, highest_msg_read, buf);
	if (r / 100 != 2)
		scr_printf("%s\n", &buf[4]);
}


/*
 * client-based uploads (for users with their own clientware)
 */
void cli_upload(CtdlIPC *ipc)
{
	char flnm[PATH_MAX];
	char desc[151];
	char buf[256];
	char tbuf[256];
	int r;		/* IPC response code */
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

	/* Keep generating filenames in hope of finding a unique one */
	a = 0;
	while (a < 10) {
		/* basename of filename */
		strcpy(tbuf, flnm);
		if (haschar(tbuf, '/'))
			extract_token(tbuf, flnm,
				num_tokens(tbuf, '/') - 1,
				'/', sizeof tbuf
			);
		/* filename.1, filename.2, etc */
		if (a > 0) {
			sprintf(&tbuf[strlen(tbuf)], ".%d", a);
		}
		/* Try upload */
		r = CtdlIPCFileUpload(ipc, tbuf, desc, flnm, progress, buf);
		if (r / 100 == 5 || r < 0)
			scr_printf("%s\n", buf);
		else
			break;
		++a;
	}
	if (a > 0) scr_printf("Saved as '%s'\n", tbuf);
}


/*
 * Function used for various image upload commands
 */
void cli_image_upload(CtdlIPC *ipc, char *keyname)
{
	char flnm[PATH_MAX];
	char buf[256];
	int r;

	/* Can we upload this image? */
	r = CtdlIPCImageUpload(ipc, 0, NULL, keyname, NULL, buf);
	if (r / 100 != 2) {
		scr_printf("%s\n", buf);
		return;
	}
	newprompt("Image file to be uploaded: ", flnm, 55);
	r = CtdlIPCImageUpload(ipc, 1, flnm, keyname, progress, buf);
	if (r / 100 == 5) {
		scr_printf("%s\n", buf);
	} else if (r < 0) {
		scr_printf("Cannot upload '%s': %s\n", flnm, strerror(errno));
	}
	/* else upload succeeded */
}


/*
 * protocol-based uploads (Xmodem, Ymodem, Zmodem)
 */
void upload(CtdlIPC *ipc, int c)
{				/* c = upload mode */
	char flnm[PATH_MAX];
	char desc[151];
	char buf[256];
	char tbuf[4096];
	int xfer_pid;
	int a, b;
	FILE *fp, *lsfp;
	int rv;

	if ((room_flags & QR_UPLOAD) == 0) {
		scr_printf("*** You cannot upload to this room.\n");
		return;
	}
	/* we don't need a filename when receiving batch y/z modem */
	if ((c == 2) || (c == 3))
		strcpy(flnm, "x");
	else
		newprompt("Enter filename: ", flnm, 15);

	for (a = 0; !IsEmptyStr(&flnm[a]); ++a)
		if ((flnm[a] == '/') || (flnm[a] == '\\') || (flnm[a] == '>')
		    || (flnm[a] == '?') || (flnm[a] == '*')
		    || (flnm[a] == ';') || (flnm[a] == '&'))
			flnm[a] = '_';

	/* create a temporary directory... */
	if (mkdir(tempdir, 0700) != 0) {
		scr_printf("*** Could not create temporary directory %s: %s\n",
		       tempdir, strerror(errno));
		return;
	}
	/* now do the transfer ... in a separate process */
	xfer_pid = fork();
	if (xfer_pid == 0) {
		rv = chdir(tempdir);
		if (rv < 0) {
			scr_printf("failed to change into %s Reason %s\nAborting now.\n", 
				   tempdir, 
				   strerror(errno));
			nukedir(tempdir);
			return;
		}
		switch (c) {
		case 0:
			stty_ctdl(0);
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
			stty_ctdl(3);
			execlp("rx", "rx", flnm, NULL);
			exit(1);
		case 2:
			stty_ctdl(3);
			execlp("rb", "rb", NULL);
			exit(1);
		case 3:
			stty_ctdl(3);
			execlp("rz", "rz", NULL);
			exit(1);
		}
	} else
		do {
			b = ka_wait(&a);
		} while ((b != xfer_pid) && (b != (-1)));
	stty_ctdl(0);

	if (a != 0) {
		scr_printf("\r*** Transfer unsuccessful.\n");
		nukedir(tempdir);
		return;
	}
	scr_printf("\r*** Transfer successful.\n");
	snprintf(buf, sizeof buf, "cd %s; ls", tempdir);
	lsfp = popen(buf, "r");
	if (lsfp != NULL) {
		while (fgets(flnm, sizeof flnm, lsfp) != NULL) {
			flnm[strlen(flnm) - 1] = 0;	/* chop newline */
			snprintf(buf, sizeof buf,
				 "Enter a short description of '%s':\n: ",
				 flnm);
			newprompt(buf, desc, 150);
			snprintf(buf, sizeof buf, "%s/%s", tempdir, flnm);
			CtdlIPCFileUpload(ipc, flnm, desc, buf, progress, tbuf);
			scr_printf("%s\n", tbuf);
		}
		pclose(lsfp);
	}
	nukedir(tempdir);
}

/* 
 * validate a user (returns 0 for successful validation, nonzero if quitting)
 */
int val_user(CtdlIPC *ipc, char *user, int do_validate)
{
	int a;
	char cmd[256];
	char buf[256];
	char *resp = NULL;
	int ax = 0;
	char answer[2];
	int r;				/* IPC response code */

	scr_printf("\n");
	r = CtdlIPCGetUserRegistration(ipc, user, &resp, cmd);
	if (r / 100 == 1) {
		a = 0;
		do {
			extract_token(buf, resp, 0, '\n', sizeof buf);
			remove_token(resp, 0, '\n');
			++a;
			if (a == 1)
				scr_printf("User #%s - %s  ", buf, cmd);
			if (a == 2)
				scr_printf("PW: %s\n", (IsEmptyStr(buf) ? "<NOT SET>" : "<SET>") );
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
		} while (!IsEmptyStr(resp));

/* TODODRW: discrepancy here. Parts of the code refer to axdefs[7] as the highest
 * but most of it limits it to axdefs[6].
 * Webcit limits to 6 as does the code here but there are 7 in axdefs.h
 */
		scr_printf("Current access level: %d (%s)\n", ax, axdefs[ax]);
	} else {
		scr_printf("%s\n%s\n", user, &cmd[4]);
	}
	if (resp) free(resp);

	if (do_validate) {
		/* now set the access level */
		while(1) {
			sprintf(answer, "%d", ax);
			strprompt("New access level (? for help, q to quit)",
				answer, 1);
			if ((answer[0] >= '0') && (answer[0] <= '6')) {
				ax = atoi(answer);
				r = CtdlIPCValidateUser(ipc, user, ax, cmd);
				if (r / 100 != 2)
				scr_printf("%s\n\n", cmd);
				return(0);
			}
			if (tolower(answer[0]) == 'q') {
				scr_printf("*** Aborted.\n\n");
				return(1);
			}
			if (answer[0] == '?') {
				scr_printf("Available access levels:\n");
				for (a=0; a<7; ++a) {
					scr_printf("%d - %s\n",
						a, axdefs[a]);
				}
			}
		}
	}
	return(0);
}


void validate(CtdlIPC *ipc)
{				/* validate new users */
	char cmd[256];
	char buf[256];
	int finished = 0;
	int r;				/* IPC response code */

	do {
		r = CtdlIPCNextUnvalidatedUser(ipc, cmd);
		if (r / 100 != 3)
			finished = 1;
		if (r / 100 == 2)
			scr_printf("%s\n", cmd);
		if (r / 100 == 3) {
			extract_token(buf, cmd, 0, '|', sizeof buf);
			if (val_user(ipc, buf, 1) != 0) finished = 1;
		}
	} while (finished == 0);
}

void subshell(void)
{
	int a, b;

	stty_ctdl(SB_RESTORE);
	a = fork();
	if (a == 0) {
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		execlp(getenv("SHELL"), getenv("SHELL"), NULL);
		scr_printf("Could not open a shell: %s\n", strerror(errno));
		exit(errno);
	}
	do {
		b = ka_wait(NULL);
	} while ((a != b) && (a != (-1)));
	stty_ctdl(0);
}

/*
 * <.A>ide <F>ile <D>elete command
 */
void deletefile(CtdlIPC *ipc)
{
	char filename[32];
	char buf[256];

	newprompt("Filename: ", filename, 31);
	if (IsEmptyStr(filename))
		return;
	CtdlIPCDeleteFile(ipc, filename, buf);
	scr_printf("%s\n", buf);
}


/*
 * <.A>ide <F>ile <M>ove command
 */
void movefile(CtdlIPC *ipc)
{
	char filename[64];
	char newroom[ROOMNAMELEN];
	char buf[256];

	newprompt("Filename: ", filename, 63);
	if (IsEmptyStr(filename))
		return;
	newprompt("Enter target room: ", newroom, ROOMNAMELEN - 1);
	CtdlIPCMoveFile(ipc, filename, newroom, buf);
	scr_printf("%s\n", buf);
}


/* 
 * list of users who have filled out a bio
 */
void list_bio(CtdlIPC *ipc)
{
	char buf[256];
	char *resp = NULL;
	int pos = 1;
	int r;			/* IPC response code */

	r = CtdlIPCListUsersWithBios(ipc, &resp, buf);
	if (r / 100 != 1) {
		scr_printf("%s\n", buf);
		return;
	}
	while (resp && !IsEmptyStr(resp)) {
		extract_token(buf, resp, 0, '\n', sizeof buf);
		remove_token(resp, 0, '\n');
		if ((pos + strlen(buf) + 5) > screenwidth) {
			scr_printf("\n");
			pos = 1;
		}
		scr_printf("%s, ", buf);
		pos = pos + strlen(buf) + 2;
	}
	scr_printf("%c%c  \n\n", 8, 8);
	if (resp) free(resp);
}


/*
 * read bio
 */
void read_bio(CtdlIPC *ipc)
{
	char who[256];
	char buf[256];
	char *resp = NULL;
	int r;			/* IPC response code */

	do {
		newprompt("Read bio for who ('?' for list) : ", who, 25);
		scr_printf("\n");
		if (!strcmp(who, "?"))
			list_bio(ipc);
	} while (!strcmp(who, "?"));

	r = CtdlIPCGetBio(ipc, who, &resp, buf);
	if (r / 100 != 1) {
		scr_printf("%s\n", buf);
		return;
	}
	while (!IsEmptyStr(resp)) {
		extract_token(buf, resp, 0, '\n', sizeof buf);
		remove_token(resp, 0, '\n');
		scr_printf("%s\n", buf);
	}
	if (resp) free(resp);
}



