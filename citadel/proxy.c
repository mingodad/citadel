/*
 * Session layer proxy for Citadel
 * (c) 1998 by Art Cancro, All Rights Reserved, released under GNU GPL v2
 */

/*
 * NOTE: this isn't finished, so don't use it!!
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "citadel.h"


extern int serv_sock;

void logoff(int code) {
	exit(code);
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

		/* Other commands, just pass through. */
		else {
			serv_puts(cmd);
			serv_gets(resp);
			printf("%s\n", resp);

			/* Simple command-response... */
			if ( (resp[0]=='2')||(resp[0]=='3')||(resp[0]=='5') ) {
				}

			/* Textual output... */
			else if (resp[0] == '1') {
				do {
					serv_gets(buf);
					printf("%s\n", buf);
					} while (strcmp(buf, "000"));
				}

			/* Textual input... */
			else if (resp[0] == '1') {
				do {
					fgets(buf, 256, stdin);
					buf[strlen(buf)-1] = 0;
					serv_puts(buf);
					} while (strcmp(buf, "000"));
				}

			/* Binary output... */
			else if (resp[0] == '6') {
				bytes = atol(&resp[4]);
				serv_read(buf, bytes);
				fwrite(buf, bytes, 1, stdout);
				}

			/* Binary input... */
			else if (resp[0] == '7') {
				bytes = atol(&resp[4]);
				fread(buf, bytes, 1, stdin);
				serv_write(buf, bytes);
				}

			/* chat... */
			else if (resp[0] == '8') {
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
	
	attach_to_server(argc, argv);

	serv_gets(buf);
	strcat(buf, " (VIA PROXY)");
	printf("%s\n", buf);
	fflush(stdout);
	if (buf[0] != '2') exit(0);

	do_mainloop();
	}
