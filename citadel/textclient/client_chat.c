/*
 * front end for multiuser chat
 *
 * Copyright (c) 1987-2012 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
#include <libcitadel.h>
#include "citadel.h"
#include "citadel_ipc.h"
#include "client_chat.h"
#include "commands.h"
#include "routines.h"
#include "citadel_decls.h"
#include "rooms.h"
#include "messages.h"
#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif
#include "screen.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

extern char temp[];
char last_paged[SIZ] = "";

void chatmode(CtdlIPC *ipc)
{
	char wbuf[SIZ];
	char buf[SIZ];
	char response[SIZ];
	char c_user[SIZ];
	char c_text[SIZ];
	char last_user[SIZ];
	int send_complete_line;
	char ch;
	int a, pos;
	int seq = 0;

	fd_set rfds;
	struct timeval tv;
	int retval;

	CtdlIPC_chat_send(ipc, "RCHT enter");
	CtdlIPC_chat_recv(ipc, buf);
	if (buf[0] != '2') {
		scr_printf("%s\n", &buf[4]);
		return;
	}
	scr_printf("Entering chat mode (type /quit to exit)\n");

	strcpy(buf, "");
	strcpy(wbuf, "");
	strcpy(last_user, ""); 
	color(BRIGHT_YELLOW);
	scr_printf("\n");
	scr_printf("> ");
	send_complete_line = 0;

	while (1) {
		scr_flush();
		FD_ZERO(&rfds);
		FD_SET(0, &rfds);
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		retval = select(1, &rfds, NULL, NULL, &tv);

		if (retval < 0) {
			color(BRIGHT_WHITE);
			scr_printf("Server gone Exiting chat mode\n");
			scr_flush();
			return;
		}

		/* If there's data from the keyboard... */
		if (FD_ISSET(0, &rfds)) {
			ch = scr_getc(SCR_BLOCK);
			if ((ch == 10) || (ch == 13)) {
				send_complete_line = 1;
			} else if ((ch == 8) || (ch == 127)) {
				if (!IsEmptyStr(wbuf)) {
					wbuf[strlen(wbuf) - 1] = 0;
					scr_printf("%c %c", 8, 8);
				}
			} else {
				scr_putc(ch);
				wbuf[strlen(wbuf) + 1] = 0;
				wbuf[strlen(wbuf)] = ch;
			}
		}

		/* if the user hit return, send the line */
		if (send_complete_line) {

			if (!strcasecmp(wbuf, "/quit")) {
				CtdlIPC_chat_send(ipc, "RCHT exit");
				CtdlIPC_chat_recv(ipc, response);	/* don't care about the result */
				color(BRIGHT_WHITE);
				scr_printf("\rExiting chat mode\n");
				scr_flush();
				return;
			}

			CtdlIPC_chat_send(ipc, "RCHT send");
			CtdlIPC_chat_recv(ipc, response);
			if (response[0] == '4') {
				CtdlIPC_chat_send(ipc, wbuf);
				CtdlIPC_chat_send(ipc, "000");
			}
			strcpy(wbuf, "");
			send_complete_line = 0;
		}

		/* if it's time to word wrap, send a partial line */
		if (strlen(wbuf) >= (77 - strlen(fullname))) {
			pos = 0;
			for (a = 0; !IsEmptyStr(&wbuf[a]); ++a) {
				if (wbuf[a] == 32)
					pos = a;
			}
			if (pos == 0) {
				CtdlIPC_chat_send(ipc, "RCHT send");
				CtdlIPC_chat_recv(ipc, response);
				if (response[0] == '4') {
					CtdlIPC_chat_send(ipc, wbuf);
					CtdlIPC_chat_send(ipc, "000");
				}
				strcpy(wbuf, "");
				send_complete_line = 0;
			} else {
				wbuf[pos] = 0;
				CtdlIPC_chat_send(ipc, "RCHT send");
				CtdlIPC_chat_recv(ipc, response);
				if (response[0] == '4') {
					CtdlIPC_chat_send(ipc, wbuf);
					CtdlIPC_chat_send(ipc, "000");
				}
				strcpy(wbuf, &wbuf[pos + 1]);
			}
		}

		/* poll for incoming chat messages */
		snprintf(buf, sizeof buf, "RCHT poll|%d", seq);
		CtdlIPC_chat_send(ipc, buf);
		CtdlIPC_chat_recv(ipc, response);
	
		if (response[0] == '1') {
			seq = extract_int(&response[4], 0);
			extract_token(c_user, &response[4], 2, '|', sizeof c_user);
			while (CtdlIPC_chat_recv(ipc, c_text), strcmp(c_text, "000")) {
				scr_printf("\r%79s\r", "");
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
					size_t i = MIN(sizeof buf - 1, strlen(c_user) + 2);
					memset(buf, ' ', i);
					safestrncpy(&buf[i], c_text, sizeof buf - i);
				}
				while (strlen(buf) < 79) {
					strcat(buf, " ");
				}
				if (strcmp(c_user, last_user)) {
					scr_printf("\r%79s\n", "");
					strcpy(last_user, c_user);
				}
				scr_printf("\r%s\n", buf);
				scr_flush();
			}
		}
		color(BRIGHT_YELLOW);
		scr_printf("\r> %s", wbuf);
		scr_flush();
		strcpy(buf, "");
	}
}


/*
 * send an instant message
 */
void page_user(CtdlIPC *ipc)
{
	char buf[SIZ], touser[SIZ], msg[SIZ];
	FILE *pagefp;

	strcpy(touser, last_paged);
	strprompt("Page who", touser, 30);

	/* old server -- use inline paging */
	if (ipc->ServInfo.paging_level == 0) {
		newprompt("Message: ", msg, 69);
		snprintf(buf, sizeof buf, "SEXP %s|%s", touser, msg);
		CtdlIPC_chat_send(ipc, buf);
		CtdlIPC_chat_recv(ipc, buf);
		if (!strncmp(buf, "200", 3)) {
			strcpy(last_paged, touser);
		}
		scr_printf("%s\n", &buf[4]);
		return;
	}
	/* new server -- use extended paging */
	else if (ipc->ServInfo.paging_level >= 1) {
		snprintf(buf, sizeof buf, "SEXP %s||", touser);
		CtdlIPC_chat_send(ipc, buf);
		CtdlIPC_chat_recv(ipc, buf);
		if (buf[0] != '2') {
			scr_printf("%s\n", &buf[4]);
			return;
		}
		if (client_make_message(ipc, temp, touser, 0, 0, 0, NULL, 0) != 0) {
			scr_printf("No message sent.\n");
			return;
		}
		pagefp = fopen(temp, "r");
		unlink(temp);
		snprintf(buf, sizeof buf, "SEXP %s|-", touser);
		CtdlIPC_chat_send(ipc, buf);
		CtdlIPC_chat_recv(ipc, buf);
		if (buf[0] == '4') {
			strcpy(last_paged, touser);
			while (fgets(buf, sizeof buf, pagefp) != NULL) {
				buf[strlen(buf) - 1] = 0;
				CtdlIPC_chat_send(ipc, buf);
			}
			fclose(pagefp);
			CtdlIPC_chat_send(ipc, "000");
			scr_printf("Message sent.\n");
		} else {
			scr_printf("%s\n", &buf[4]);
		}
	}
}


void quiet_mode(CtdlIPC *ipc)
{
	static int quiet = 0;
	char cret[SIZ];
	int r;

	r = CtdlIPCEnableInstantMessageReceipt(ipc, !quiet, cret);
	if (r / 100 == 2) {
		quiet = !quiet;
		scr_printf("Quiet mode %sabled (%sother users may page you)\n",
				(quiet) ? "en" : "dis",
				(quiet) ? "no " : "");
	} else {
		scr_printf("Unable to change quiet mode: %s\n", cret);
	}
}


void stealth_mode(CtdlIPC *ipc)
{
	static int stealth = 0;
	char cret[SIZ];
	int r;

	r = CtdlIPCStealthMode(ipc, !stealth, cret);
	if (r / 100 == 2) {
		stealth = !stealth;
		scr_printf("Stealth mode %sabled (you are %s)\n",
				(stealth) ? "en" : "dis",
				(stealth) ? "invisible" : "listed as online");
	} else {
		scr_printf("Unable to change stealth mode: %s\n", cret);
	}
}
