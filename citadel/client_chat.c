/*
 * $Id$
 *
 * front end for chat mode
 * (the "single process" version - no more fork() anymore)
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

#include <sys/types.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <stdarg.h>
#include "citadel.h"
#include "client_chat.h"
#include "commands.h"
#include "routines.h"
#include "ipc.h"
#include "citadel_decls.h"
#include "tools.h"
#include "rooms.h"
#include "messages.h"
#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif
#include "screen.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

extern struct CtdlServInfo serv_info;
extern char temp[];
void getline(char *, int);

char last_paged[SIZ] = "";

void chatmode(void)
{
	char wbuf[SIZ];
	char buf[SIZ];
	char c_user[SIZ];
	char c_text[SIZ];
	char c_room[SIZ];
	char last_user[SIZ];
	int send_complete_line;
	int recv_complete_line;
	char ch;
	int a, pos;
	time_t last_transmit;

	fd_set rfds;
	struct timeval tv;
	int retval;

	serv_puts("CHAT");
	serv_gets(buf);
	if (buf[0] != '8') {
		scr_printf("%s\n", &buf[4]);
		return;
	}
	scr_printf("Entering chat mode (type /quit to exit, /help for other cmds)\n");
	set_keepalives(KA_NO);
	last_transmit = time(NULL);

	strcpy(buf, "");
	strcpy(wbuf, "");
	color(BRIGHT_YELLOW);
	sln_printf_if("\n");
	sln_printf("> ");
	send_complete_line = 0;
	recv_complete_line = 0;

	while (1) {
		sln_flush();
		FD_ZERO(&rfds);
		FD_SET(0, &rfds);
		FD_SET(getsockfd(), &rfds);
		tv.tv_sec = S_KEEPALIVE;
		tv.tv_usec = 0;
		retval = select(getsockfd() + 1, &rfds, NULL, NULL, &tv);

		if (FD_ISSET(getsockfd(), &rfds)) {
			ch = serv_getc();
			if (ch == 10) {
				recv_complete_line = 1;
				goto RCL;	/* ugly, but we've gotta get out! */
			} else {
				buf[strlen(buf) + 1] = 0;
				buf[strlen(buf)] = ch;
			}
			goto RCL;
		}
		if (FD_ISSET(0, &rfds)) {
			ch = scr_getc(SCR_BLOCK);
			if ((ch == 10) || (ch == 13)) {
				send_complete_line = 1;
			} else if ((ch == 8) || (ch == 127)) {
				if (strlen(wbuf) > 0) {
					wbuf[strlen(wbuf) - 1] = 0;
					sln_printf("%c %c", 8, 8);
				}
			} else {
				sln_putc(ch);
				wbuf[strlen(wbuf) + 1] = 0;
				wbuf[strlen(wbuf)] = ch;
			}
		}
		/* if the user hit return, send the line */
	      RCL:if (send_complete_line) {
			serv_puts(wbuf);
			last_transmit = time(NULL);
			strcpy(wbuf, "");
			send_complete_line = 0;
		}
		/* if it's time to word wrap, send a partial line */
		if (strlen(wbuf) >= (77 - strlen(fullname))) {
			pos = 0;
			for (a = 0; a < strlen(wbuf); ++a) {
				if (wbuf[a] == 32)
					pos = a;
			}
			if (pos == 0) {
				serv_puts(wbuf);
				last_transmit = time(NULL);
				strcpy(wbuf, "");
				send_complete_line = 0;
			} else {
				wbuf[pos] = 0;
				serv_puts(wbuf);
				last_transmit = time(NULL);
				strcpy(wbuf, &wbuf[pos + 1]);
			}
		}
		if (recv_complete_line) {
			sln_printf("\r%79s\r", "");
			if (!strcmp(buf, "000")) {
				color(BRIGHT_WHITE);
				sln_printf("\rExiting chat mode\n");
				sln_flush();
				set_keepalives(KA_YES);

				/* Some users complained about the client and server
				 * losing protocol synchronization when exiting chat.
				 * This little dialog forces everything to be
				 * hunky-dory.
				 */
				serv_puts("ECHO __ExitingChat__");
				do {
					serv_gets(buf);
				} while (strcmp(buf, "200 __ExitingChat__"));


				return;
			}
			if (num_parms(buf) >= 2) {
				extract(c_user, buf, 0);
				extract(c_text, buf, 1);
				if (num_parms(buf) > 2) {
					extract(c_room, buf, 2);
					scr_printf("Got room %s\n", c_room);
				}
				if (strcasecmp(c_text, "NOOP")) {
					if (!strcmp(c_user, fullname)) {
						color(BRIGHT_YELLOW);
					} else if (!strcmp(c_user, ":")) {
						color(BRIGHT_RED);
					} else {
						color(BRIGHT_GREEN);
					}
					if (strcmp(c_user, last_user)) {
						snprintf(buf, sizeof buf, "%s: %s", c_user, c_text);
					} else {
						size_t i = MIN(sizeof buf - 1,
						     strlen(c_user) + 2);

						memset(buf, ' ', i);
						safestrncpy(&buf[i], c_text,
							 sizeof buf - i);
					}
					while (strlen(buf) < 79)
						strcat(buf, " ");
					if (strcmp(c_user, last_user)) {
						sln_printf("\r%79s\n", "");
						strcpy(last_user, c_user);
					}
					scr_printf("\r%s\n", buf);
					scr_flush();
				}
			}
			color(BRIGHT_YELLOW);
			sln_printf("\r> %s", wbuf);
			sln_flush();
			recv_complete_line = 0;
			strcpy(buf, "");
		}

		/* If the user is sitting idle, send a half-keepalive to the
		 * server to prevent session timeout.
		 */
		if ((time(NULL) - last_transmit) >= S_KEEPALIVE) {
			serv_puts("NOOP");
			last_transmit = time(NULL);
		}

	}
}

/*
 * send an express message
 */
void page_user()
{
	char buf[SIZ], touser[SIZ], msg[SIZ];
	FILE *pagefp;

	strcpy(touser, last_paged);
	strprompt("Page who", touser, 30);

	/* old server -- use inline paging */
	if (serv_info.serv_paging_level == 0) {
		newprompt("Message:  ", msg, 69);
		snprintf(buf, sizeof buf, "SEXP %s|%s", touser, msg);
		serv_puts(buf);
		serv_gets(buf);
		if (!strncmp(buf, "200", 3)) {
			strcpy(last_paged, touser);
		}
		scr_printf("%s\n", &buf[4]);
		return;
	}
	/* new server -- use extended paging */
	else if (serv_info.serv_paging_level >= 1) {
		snprintf(buf, sizeof buf, "SEXP %s||", touser);
		serv_puts(buf);
		serv_gets(buf);
		if (buf[0] != '2') {
			scr_printf("%s\n", &buf[4]);
			return;
		}
		if (client_make_message(temp, touser, 0, 0, 0, NULL) != 0) {
			scr_printf("No message sent.\n");
			return;
		}
		pagefp = fopen(temp, "r");
		unlink(temp);
		snprintf(buf, sizeof buf, "SEXP %s|-", touser);
		serv_puts(buf);
		serv_gets(buf);
		if (buf[0] == '4') {
			strcpy(last_paged, touser);
			while (fgets(buf, sizeof buf, pagefp) != NULL) {
				buf[strlen(buf) - 1] = 0;
				serv_puts(buf);
			}
			fclose(pagefp);
			serv_puts("000");
			scr_printf("Message sent.\n");
		} else {
			scr_printf("%s\n", &buf[4]);
		}
	}
}




void quiet_mode(void)
{
	int qstate;
	char buf[SIZ];

	serv_puts("DEXP 2");
	serv_gets(buf);
	if (buf[0] != '2') {
		scr_printf("%s\n", &buf[4]);
		return;
	}
	qstate = atoi(&buf[4]);
	if (qstate == 0)
		qstate = 1;
	else
		qstate = 0;
	snprintf(buf, sizeof buf, "DEXP %d", qstate);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '2') {
		scr_printf("%s\n", &buf[4]);
		return;
	}
	qstate = atoi(&buf[4]);
	if (qstate) {
		scr_printf("Quiet mode enabled (no other users may page you)\n");
	} else {
		scr_printf("Quiet mode disabled (other users may page you)\n");
	}
}


void stealth_mode(void)
{
	int qstate;
	char buf[SIZ];

	serv_puts("STEL 2");
	serv_gets(buf);
	if (buf[0] != '2') {
		scr_printf("%s\n", &buf[4]);
		return;
	}
	qstate = atoi(&buf[4]);
	if (qstate == 0)
		qstate = 1;
	else
		qstate = 0;
	snprintf(buf, sizeof buf, "STEL %d", qstate);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '2') {
		scr_printf("%s\n", &buf[4]);
		return;
	}
	qstate = atoi(&buf[4]);
	if (qstate) {
		scr_printf("Stealth mode enabled (you are invisible)\n");
	} else {
		scr_printf("Stealth mode disabled (you are listed as online)\n");
	}
}
