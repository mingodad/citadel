/*
 * Session layer proxy for Citadel
 * (c) 1998 by Art Cancro, All Rights Reserved, released under GNU GPL v2
 */

/*
 * NOTE: this isn't finished, so don't use it!!
 *
 */

#define CACHE_DIR	"/var/citadelproxy"

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include "citadel.h"

void logoff(int code) {
	exit(code);
	}


void do_msg0(char cmd[]) {
	long msgid;
	char filename[32];
	char temp[32];
	char buf[256];
	FILE *fp;

	msgid = atol(&cmd[5]);
	sprintf(filename, "%ld", msgid);

	/* If the message is cached, use the copy on disk */
	fp = fopen(filename, "r");
	if (fp != NULL) {
		printf("%d Cached message %ld:\n", LISTING_FOLLOWS, msgid);
		while (fgets(buf, 256, fp) != NULL) {
			buf[strlen(buf)-1]=0;
			printf("%s\n", buf);
			}
		fclose(fp);
		printf("000\n");
		fflush(stdout);
		}

	/* Otherwise, fetch the message from the server and cache it */
	else {
		sprintf(buf, "MSG0 %ld", msgid);
		serv_puts(buf);	
		serv_gets(buf);
		printf("%s\n", buf);
		fflush(stdout);
		if (buf[0] != '1') {
			return;
			}

		/* The message is written to a file with a temporary name, in
		 * order to avoid another user accidentally fetching a
		 * partially written message from the cache.
		 */
		sprintf(temp, "%ld.%d", msgid, getpid());
		fp = fopen(temp, "w");
		while (serv_gets(buf), strcmp(buf, "000")) {
			printf("%s\n", buf);
			fprintf(fp, "%s\n", buf);
			}
		printf("%s\n", buf);
		fflush(stdout);
		fclose(fp);

		/* Now that the message is complete, it can be renamed to the
		 * filename that the cache manager will recognize it with.
		 */
		link(temp, filename);
		unlink(temp);
		}

	}


void do_mainloop() {
	char cmd[256];
	char resp[256];
	char buf[4096];
	int bytes;

	while(1) {
		fflush(stdout);
		if (fgets(cmd, 256, stdin) == NULL) {
			serv_puts("QUIT");
			exit(1);
			}
		cmd[strlen(cmd)-1] = 0;

		/* QUIT commands are handled specially */
		if (!strncasecmp(cmd, "QUIT", 4)) {
			serv_puts("QUIT");
			printf("%d Proxy says: Bye!\n", OK);
			fflush(stdout);
			exit(0);
			}

		else if (!strncasecmp(cmd, "CHAT", 4)) {
			printf("%d Can't chat through the proxy ... yet.\n",
				ERROR);
			}

		else if (!strncasecmp(cmd, "MSG0", 4)) {
			do_msg0(cmd);
			}

		/* Other commands, just pass through. */
		else {
			
			serv_puts(cmd);
			serv_gets(resp);
			printf("%s\n", resp);
			fflush(stdout);

			/* Simple command-response... */
			if ( (resp[0]=='2')||(resp[0]=='3')||(resp[0]=='5') ) {
				}

			/* Textual input... */
			else if (resp[0] == '4') {
				do {
					if (fgets(buf, 256, stdin) == NULL) {
						exit(errno);
						}
					buf[strlen(buf)-1] = 0;
					serv_puts(buf);
					} while (strcmp(buf, "000"));
				}

			/* Textual output... */
			else if (resp[0] == '1') {
				do {
					serv_gets(buf);
					printf("%s\n", buf);
					} while (strcmp(buf, "000"));
				}

			/* Binary output... */
			else if (resp[0] == '6') {
				bytes = atol(&resp[4]);
				serv_read(buf, bytes);
				fwrite(buf, bytes, 1, stdout);
				fflush(stdout);
				}

			/* Binary input... */
			else if (resp[0] == '7') {
				bytes = atol(&resp[4]);
				fread(buf, bytes, 1, stdin);
				serv_write(buf, bytes);
				}

			/* chat... */
			else if (resp[0] == '8') {
				sleep(2);
				serv_puts("/quit");
				do {
					fgets(buf, 256, stdin);
					buf[strlen(buf)-1] = 0;
					serv_puts(buf);
					} while (strcmp(buf, "000"));
				}


			}
		}
	}



void main(int argc, char *argv[]) {
	char buf[256];

	/* Create the cache directory.  Ignore any error return, 'cuz that
	 * just means it's already there.  FIX... this really should check
	 * for that particular error.
	 */
	mkdir(CACHE_DIR, 0700);

	/* Now go there */
	if (chdir(CACHE_DIR) != 0) exit(errno);

	attach_to_server(argc, argv);

	serv_gets(buf);
	strcat(buf, " (VIA PROXY)");
	printf("%s\n", buf);
	fflush(stdout);
	if (buf[0] != '2') exit(0);

	do_mainloop();
	}
