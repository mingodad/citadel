/*
 * Citadel/UX
 *
 * client_icq.c  --  manage Citadel ICQ configuration
 *                    (the "single process" version - no more fork() anymore)
 *
 * $Id$
 *
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdarg.h>
#include "citadel.h"
#include "client_chat.h"
#include "commands.h"
#include "routines.h"
#include "ipc.h"
#include "citadel_decls.h"
#include "tools.h"
#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

extern struct CtdlServInfo serv_info;
extern char temp[];
void getline(char *, int);

struct icq_contact {
	long uin;
	char name[256];
};


/* 
 * Do the Citadel ICQ client setup
 */
void setup_icq(void) {
	char uin[32];
	char pass[32];
	char buf[256];
	int i;
	
	struct icq_contact *contacts = NULL;
	int num_contacts = 0;	
	

        printf("Do you want to enter your ICQ uin and password? ");
        if (yesno()) {
		newprompt("     Enter your ICQ UIN: ", uin, 31);
		newprompt("Enter your ICQ password: ", pass, -31); 
		if (atol(uin)<=0L) {
			printf("You must enter a UIN.  "
				"Citadel ICQ configuration aborted.\n");
			return;
		}
		sprintf(buf, "CICQ login|%s|%s|", uin, pass);
		serv_puts(buf);
		serv_gets(buf);
		printf("%s\n", &buf[4]);
		if (buf[0] != '2') return;
	}

	printf("Do you want to edit your ICQ contact list? ");
        if (yesno()) {
		serv_puts("CICQ getcl");
		serv_gets(buf);
		if (buf[0]=='1') while (serv_gets(buf), strcmp(buf, "000")) {
			contacts = realloc(contacts,
				(++num_contacts * sizeof(struct icq_contact)));
			contacts[num_contacts-1].uin =
				extract_long(buf, 0);
			extract(contacts[num_contacts-1].name, buf, 1);
		}
		if (num_contacts) for (i=0; i<num_contacts; ++i) {
			color(BRIGHT_WHITE);
			printf("%10ld ", contacts[i].uin);
			color(DIM_WHITE);
			printf("(");
			color(BRIGHT_CYAN);
			printf("%32s", contacts[i].name);
			color(DIM_WHITE);
			printf(") ... ");
			color(BRIGHT_YELLOW);
			printf("Keep (yes/no) ? ");
			color(DIM_WHITE);
			if (!yesno()) contacts[i].uin = 0L;
		}

		printf("Enter the UIN's of any additional contacts you\n");
		printf("wish to add.  Enter a blank line when finished.\n");
		do {
			newprompt("> ", buf, 32);
			if (atol(buf) > 0L) {
				contacts = realloc(contacts,
					(++num_contacts * sizeof(struct icq_contact)));
				contacts[num_contacts-1].uin = atoi(buf);
			}
		} while (strlen(buf) > 0);

		serv_puts("CICQ putcl");
		serv_gets(buf);
		if (buf[0]=='4') {
			if (num_contacts) for (i=0; i<num_contacts; ++i) {
				sprintf(buf, "%ld", contacts[i].uin);
				serv_puts(buf);
			}
			if (num_contacts) free(contacts);
			serv_puts("000");
		}
	}
}
