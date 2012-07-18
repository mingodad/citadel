/*
 * Functions which allow the client to remember usernames and passwords for
 * various sites.
 *
 * Copyright (c) 1987-2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdio.h>
#include <libcitadel.h>
///#include "citadel.h"
#include "citadel_ipc.h"
#include "commands.h"
#include "client_passwords.h"

#define PWFILENAME "%s/.citadel.passwords"

void determine_pwfilename(char *pwfile, size_t n) {
	struct passwd *p;

	p = getpwuid(getuid());
	if (p == NULL) strcpy(pwfile, "");
	snprintf(pwfile, n, PWFILENAME, p->pw_dir);
}


/*
 * Check the password file for a host/port match; if found, stuff the user
 * name and password into the user/pass buffers
 */
void get_stored_password(
		char *host,
		char *port,
		char *username,
		char *password) {

	char pwfile[PATH_MAX];
	FILE *fp;
	char buf[SIZ];
	char buf64[SIZ];
	char hostbuf[256], portbuf[256], ubuf[256], pbuf[256];

	strcpy(username, "");
	strcpy(password, "");

	determine_pwfilename(pwfile, sizeof pwfile);
	if (IsEmptyStr(pwfile)) return;

	fp = fopen(pwfile, "r");
	if (fp == NULL) return;
	while (fgets(buf64, sizeof buf64, fp) != NULL) {
		CtdlDecodeBase64(buf, buf64, sizeof(buf64));
		extract_token(hostbuf, buf, 0, '|', sizeof hostbuf);
		extract_token(portbuf, buf, 1, '|', sizeof portbuf);
		extract_token(ubuf, buf, 2, '|', sizeof ubuf);
		extract_token(pbuf, buf, 3, '|', sizeof pbuf);

		if (!strcasecmp(hostbuf, host)) {
			if (!strcasecmp(portbuf, port)) {
				strcpy(username, ubuf);
				strcpy(password, pbuf);
			}
		}
	}
	fclose(fp);
}


/*
 * Set (or clear) stored passwords.
 */
void set_stored_password(
		char *host,
		char *port,
		char *username,
		char *password) {

	char pwfile[PATH_MAX];
	FILE *fp, *oldfp;
	char buf[SIZ];
	char buf64[SIZ];
	char hostbuf[256], portbuf[256], ubuf[256], pbuf[256];

	determine_pwfilename(pwfile, sizeof pwfile);
	if (IsEmptyStr(pwfile)) return;

	oldfp = fopen(pwfile, "r");
	if (oldfp == NULL) oldfp = fopen("/dev/null", "r");
	unlink(pwfile);
	fp = fopen(pwfile, "w");
	if (fp == NULL) fp = fopen("/dev/null", "w");
	while (fgets(buf64, sizeof buf64, oldfp) != NULL) {
		CtdlDecodeBase64(buf, buf64, sizeof(buf64));
		extract_token(hostbuf, buf, 0, '|', sizeof hostbuf);
		extract_token(portbuf, buf, 1, '|', sizeof portbuf);
		extract_token(ubuf, buf, 2, '|', sizeof ubuf);
		extract_token(pbuf, buf, 3, '|', sizeof pbuf);

		if ( (strcasecmp(hostbuf, host)) 
		   || (strcasecmp(portbuf, port)) ) {
			snprintf(buf, sizeof buf, "%s|%s|%s|%s|",
				hostbuf, portbuf, ubuf, pbuf);
			CtdlEncodeBase64(buf64, buf, strlen(buf), 0);
			fprintf(fp, "%s\n", buf64);
		}
	}
	if (!IsEmptyStr(username)) {
		snprintf(buf, sizeof buf, "%s|%s|%s|%s|",
			host, port, username, password);
		CtdlEncodeBase64(buf64, buf, strlen(buf), 0);
		fprintf(fp, "%s\n", buf64);
	}
	fclose(oldfp);
	fclose(fp);
	chmod(pwfile, 0600);
}


/*
 * Set the password if the user wants to, clear it otherwise 
 */
void offer_to_remember_password(CtdlIPC *ipc,
		char *host,
		char *port,
		char *username,
		char *password) {

	if (rc_remember_passwords) {
		if (boolprompt("Remember username/password for this site", 0)) {
			set_stored_password(host, port, username, password);
		}
		else {
			set_stored_password(host, port, "", "");
		}
	}
}
