#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include "citadel.h"

struct CtdlServerHandle {
	char ServerAddress[64];
	int ServerPort;
	char ipgmSecret[32];
	char UserName[32];
	char Password[32];
	char InitialRoom[32];
	int AssocClientSession;
	};

struct CtdlServerHandle CtdlMyHandle;


void logoff(exitcode) {
	exit(exitcode);
	}

/*
 * Programs linked against the Citadel server extension library need to
 * be called with the following arguments:
 * 0 - program name (as always)
 * 1 - server address (usually 127.0.0.1)
 * 2 - server port number
 * 3 - internal program secret
 * 4 - user name
 * 5 - user password
 * 6 - initial room
 * 7 - associated client session
 * 
 */

main(argc, argv)
int argc;
char *argv[]; {
	int a;
	char buf[256];

	/* We're really not interested in stdio */
	close(0);
	close(1);
	close(2);

	/* Bail out if someone tries to run this thing manually */
	if (argc < 3) exit(1);

	/* Zeroing out the server handle neatly sets the values of
	 * CtdlMyHandle to sane default values
	 */
	bzero(&CtdlMyHandle, sizeof(struct CtdlServerHandle));

	/* Now parse the command-line arguments fed to us by the server */
	for (a=0; a<argc; ++a) switch(a) {
		case 1:	strcpy(CtdlMyHandle.ServerAddress, argv[a]);
			break;
		case 2:	CtdlMyHandle.ServerPort = atoi(argv[a]);
			break;
		case 3:	strcpy(CtdlMyHandle.ipgmSecret, argv[a]);
			break;
		case 4:	strcpy(CtdlMyHandle.UserName, argv[a]);
			break;
		case 5:	strcpy(CtdlMyHandle.Password, argv[a]);
			break;
		case 6:	strcpy(CtdlMyHandle.InitialRoom, argv[a]);
			break;
		case 7:	CtdlMyHandle.AssocClientSession = atoi(argv[a]);
			break;
		}

	/* Connect to the server */
	argc = 3;
	attach_to_server(argc, argv);
	serv_gets(buf);
	if (buf[0] != '2') exit(1);

	/* Set up the server environment to our liking */

	sprintf(buf, "IDEN 0|5|006|CitadelAPI Client");
	serv_puts(buf);
	serv_gets(buf);

	if (strlen(CtdlMyHandle.ipgmSecret) > 0) {
		sprintf(buf, "IPGM %s", CtdlMyHandle.ipgmSecret);
		serv_puts(buf);
		serv_gets(buf);
		}

	if (strlen(CtdlMyHandle.UserName) > 0) {
		sprintf(buf, "USER %s", CtdlMyHandle.UserName);
		serv_puts(buf);
		serv_gets(buf);
		sprintf(buf, "PASS %s", CtdlMyHandle.Password);
		serv_puts(buf);
		serv_gets(buf);
		}

	sprintf(buf, "GOTO %s", CtdlMyHandle.InitialRoom);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '2') {
		serv_puts("GOTO _BASEROOM_");
		serv_gets(buf);
		}

	/* Now do the loop. */
	sleep(10);

	/* Clean up gracefully and exit. */
	serv_puts("QUIT");
	exit(0);
	}
