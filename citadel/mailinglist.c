/*
 * $Id$
 *
 * This program is designed to be a filter which allows two-way interaction
 * between a traditional e-mail mailing list and a Citadel room.
 *
 * It only handles the outbound side.  The inbound side is handled by the
 * Citadel e-mail gateway.  You should subscribe rooms to lists using the
 * "room_roomname@node.dom" type address.
 * 
 * Since some listprocs only accept postings from subscribed addresses, the
 * messages which this program converts will all appear to be from the room
 * address; however, the full name of the sender is set to the Citadel user
 * name of the real message author.
 * 
 * To prevent loops, this program only sends messages originating on the local
 * system.  Therefore it is not possible to carry a two-way mailing list room
 * on a Citadel network.
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include "citadel.h"

void LoadInternetConfig(void);
void get_config(void);
struct config config;

char ALIASES[128];
char CIT86NET[128];
char SENDMAIL[128];
char FALLBACK[128];
char GW_DOMAIN[128];
char TABLEFILE[128];
char OUTGOING_FQDN[128];
int RUN_NETPROC = 1;

/* 
 * Consult the mailinglists table to find out where we should send the
 * mailing list postings to.
 */
void xref(char *room, char *listaddr)
{				/* xref table */
    FILE *fp;
    int a;
    char buf[128];

    strcpy(listaddr, "");
    fp = fopen(TABLEFILE, "r");
    while (fgets(buf, 128, fp) != NULL) {
	buf[strlen(buf) - 1] = 0;
	for (a = 0; a < strlen(buf); ++a) {
	    if ((buf[a] == ',') && (!strcasecmp(&buf[a + 1], room))) {
		strcpy(listaddr, buf);
		listaddr[a] = 0;
	    }
	}
    }
    fclose(fp);
    return;
}


/*
 * The main loop.  We don't need any command-line parameters to this program.
 */
int main(void)
{

    char header[3];
    char fields[32][1024];
    int num_fields;
    int ch, p, a, i;
    int in_header;
    int is_good;
    char listaddr[512];
    char mailcmd[256];
    FILE *nm;
    char tempfile[64];

    get_config();
    LoadInternetConfig();
    strcpy(tempfile, tmpnam(NULL));

    while (1) {

	/* seek to the next message */
	is_good = 0;
	do {
	    if (feof(stdin)) {
		unlink(tempfile);
		exit(0);
	    }
	} while (getc(stdin) != 255);

	header[0] = 255;
	header[1] = getc(stdin);
	header[2] = getc(stdin);
	in_header = 1;
	num_fields = 0;

	do {
	    fields[num_fields][0] = getc(stdin);
	    if (fields[num_fields][0] != 'M') {
		p = 1;
		do {
		    ch = getc(stdin);
		    fields[num_fields][p++] = ch;
		} while ((ch != 0) && (!feof(stdin)));

		/**********************************************************/
		/* Only send messages which originated on the Citadel net */
		/* (In other words, from node names with no dots in them) */

		if (fields[num_fields][0] == 'N') {
		    is_good = 1;
		    for (i = 1; i < strlen(&fields[num_fields][1]); ++i) {
			if (fields[num_fields][i] == '.') {
			    is_good = 0;
			}
		    }
		}
		/**********************************************************/

		if (fields[num_fields][0] == 'O') {
		    xref(&fields[num_fields][1], listaddr);
		}
		if (fields[num_fields][0] != 'R')
		    ++num_fields;
	    } else {
		/* flush the message out to the next program */

		nm = fopen(tempfile, "wb");
		fprintf(nm, "%c%c%c",
			header[0], header[1], header[2]);
		for (a = 0; a < num_fields; ++a) {
		    fprintf(nm, "%s%c", &fields[a][0], 0);
		}
		fprintf(nm, "R%s%c", listaddr, 0);

		putc('M', nm);
		do {
		    ch = getc(stdin);
		    putc(ch, nm);
		} while ((ch != 0) && (!feof(stdin)));
		in_header = 0;
		fclose(nm);
		if (is_good) {
		    sprintf(mailcmd, "exec netmailer %s mlist", tempfile);
		    system(mailcmd);
		    is_good = 0;
		}
	    }
	} while (in_header);
    }
}
