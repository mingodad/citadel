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
#include "tools.h"
#include "commands.h"

#define PWFILENAME "%s/.citadel.passwords"

void determine_pwfilename(char *pwfile) {
	struct passwd *p;

	p = getpwuid(getuid());
	if (p == NULL) strcpy(pwfile, "");
	sprintf(pwfile, PWFILENAME, p->pw_dir);
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
	char buf[256];
	char buf64[256];
	char hostbuf[256], portbuf[256], ubuf[256], pbuf[256];

	strcpy(username, "");
	strcpy(password, "");

	determine_pwfilename(pwfile);
	if (strlen(pwfile)==0) return;

	fp = fopen(pwfile, "r");
	if (fp == NULL) return;
	while (fgets(buf64, sizeof buf64, fp) != NULL) {
		decode_base64(buf, buf64);
		extract(hostbuf, buf, 0);
		extract(portbuf, buf, 1);
		extract(ubuf, buf, 2);
		extract(pbuf, buf, 3);

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
	char buf[256];
	char buf64[256];
	char hostbuf[256], portbuf[256], ubuf[256], pbuf[256];

	determine_pwfilename(pwfile);
	if (strlen(pwfile)==0) return;

	oldfp = fopen(pwfile, "r");
	if (oldfp == NULL) oldfp = fopen("/dev/null", "r");
	unlink(pwfile);
	fp = fopen(pwfile, "w");
	if (fp == NULL) fp = fopen("/dev/null", "w");
	while (fgets(buf64, sizeof buf64, oldfp) != NULL) {
		decode_base64(buf, buf64);
		extract(hostbuf, buf, 0);
		extract(portbuf, buf, 1);
		extract(ubuf, buf, 2);
		extract(pbuf, buf, 3);

		if ( (strcasecmp(hostbuf, host)) 
		   || (strcasecmp(portbuf, port)) ) {
			sprintf(buf, "%s|%s|%s|%s|",
				hostbuf, portbuf, ubuf, pbuf);
			encode_base64(buf64, buf);
			fprintf(fp, "%s\n", buf64);
		}
	}
	if (strlen(username) > 0) {
		sprintf(buf, "%s|%s|%s|%s|",
			host, port, username, password);
		encode_base64(buf64, buf);
		fprintf(fp, "%s\n", buf64);
	}
	fclose(oldfp);
	fclose(fp);
	chmod(pwfile, 0600);
}


/*
 * Set the password if the user wants to, clear it otherwise 
 */
void offer_to_remember_password(
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
