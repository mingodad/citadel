/*
 * Citadel/UX
 *
 * client_chat.c  --  front end for chat mode
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
#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

#define MIN(a, b) ((a) < (b) ? (a) : (b))

extern struct CtdlServInfo serv_info;
extern char temp[];
void citedit(FILE *fp, long int base_pos);
void getline(char *, int);

void chatmode(void) {
	char wbuf[256];
	char buf[256];
	char c_user[256];
	char c_text[256];
	char c_room[256];
	char last_user[256];
	int send_complete_line;
	int recv_complete_line;
	char ch;
	int a,pos;
	
	fd_set rfds;
	struct timeval tv;
	int retval;

	serv_puts("CHAT");
	serv_gets(buf);
	if (buf[0]!='8') {
		printf("%s\n",&buf[4]);
		return;
		}

	printf("Entering chat mode (type /quit to exit, /help for other cmds)\n");
	set_keepalives(KA_CHAT);

	strcpy(buf,"");
	strcpy(wbuf,"");
	color(BRIGHT_YELLOW);
	printf("> ");
	send_complete_line = 0;
	recv_complete_line = 0;

	while(1) {
	    fflush(stdout);
	    FD_ZERO(&rfds);
	    FD_SET(0,&rfds);
	    FD_SET(getsockfd(),&rfds);
	    tv.tv_sec = S_KEEPALIVE;
	    tv.tv_usec = 0;
	    retval = select(getsockfd()+1, &rfds, NULL, NULL, &tv);

	    if (FD_ISSET(getsockfd(), &rfds)) {
		ch = serv_getc();
		if (ch == 10) {
			recv_complete_line = 1;
			goto RCL; /* ugly, but we've gotta get out! */
			}
		else {
			buf[strlen(buf) + 1] = 0;
			buf[strlen(buf)] = ch;
			}
		goto RCL;
		}

	    if (FD_ISSET(0, &rfds)) {
		ch = inkey();
		if ((ch == 10) || (ch == 13)) {
			send_complete_line = 1;
			}
		else if ((ch == 8) || (ch == 127)) {
			if (strlen(wbuf) > 0) {
				wbuf[strlen(wbuf)-1] = 0;
				printf("%c %c",8,8);
				}
			}
		else {
			putc(ch,stdout);
			wbuf[strlen(wbuf) + 1] = 0;
			wbuf[strlen(wbuf)] = ch;
			}
		}


	/* if the user hit return, send the line */
RCL:	    if (send_complete_line) {
		serv_puts(wbuf);
		strcpy(wbuf,"");
		send_complete_line = 0;
		}

	/* if it's time to word wrap, send a partial line */
	    if ( strlen(wbuf) >= (77-strlen(fullname)) ) {
		pos = 0;
		for (a=0; a<strlen(wbuf); ++a) {
			if (wbuf[a] == 32) pos = a;
			}
		if (pos == 0) {
			serv_puts(wbuf);
			strcpy(wbuf, "");
			send_complete_line = 0;
			}
		else {
			wbuf[pos] = 0;
			serv_puts(wbuf);
			strcpy(wbuf,&wbuf[pos+1]);
			}
		}

	    if (recv_complete_line) {	
		printf("\r%79s\r","");
		if (!strcmp(buf,"000")) {
			color(BRIGHT_WHITE);
			printf("Exiting chat mode\n");

			fflush(stdout);
			set_keepalives(KA_YES);
			return;
			}
		if (num_parms(buf)>=2) {
			extract(c_user,buf,0);
			extract(c_text,buf,1);
			if (num_parms(buf)>2)
			{
   			   extract(c_room,buf,2);
   			   printf("Got room %s\n", c_room);
   			}
   			   
			if (strucmp(c_text,"NOOP")) {
				if (!strcmp(c_user, fullname)) {
					color(BRIGHT_YELLOW);
					}
				else if (!strcmp(c_user,":")) {
					color(BRIGHT_RED);
					}
				else {
					color(BRIGHT_GREEN);
					}
				if (strcmp(c_user,last_user)) {
					snprintf(buf,sizeof buf,"%s: %s",c_user,c_text);
					}
				else {
					size_t i = MIN(sizeof buf - 1,
						       strlen(c_user) + 2);

					memset(buf, ' ', i);
					safestrncpy(&buf[i], c_text,
						    sizeof buf - i);
					}
				while (strlen(buf)<79) strcat(buf," ");
				if (strcmp(c_user,last_user)) {
					printf("\r%79s\n","");
					strcpy(last_user,c_user);
					}
				printf("\r%s\n",buf);
				fflush(stdout);
				}
			}
		color(BRIGHT_YELLOW);
		printf("> %s",wbuf);
		recv_complete_line = 0;
		strcpy(buf,"");
		}
	    }
	}

/*
 * send an express message
 */
void page_user() {
	static char last_paged[32] = "";
	char buf[256], touser[256], msg[256];
	FILE *pagefp;

	strcpy(touser, last_paged);
	strprompt("Page who", touser, 30);

	/* old server -- use inline paging */
	if (serv_info.serv_paging_level == 0) {
		newprompt("Message:  ", msg, 69);
		snprintf(buf,sizeof buf,"SEXP %s|%s",touser,msg);
		serv_puts(buf);
		serv_gets(buf);
		if (!strncmp(buf, "200", 3)) {
	   		strcpy(last_paged, touser);
			}
		printf("%s\n", &buf[4]);
		return;
	}

	/* new server -- use extended paging */ 
	else if (serv_info.serv_paging_level >= 1) {
		snprintf(buf, sizeof buf, "SEXP %s||", touser);
		serv_puts(buf);
		serv_gets(buf);
		if (buf[0] != '2') {
			printf("%s\n", &buf[4]);
			return;
			}

		printf("Type message to send.  Enter a blank line when finished.\n");
		pagefp = fopen(temp, "w+");
		unlink(temp);
		citedit(pagefp, 0L);
		fseek(pagefp, 0L, SEEK_END);
		if ( ftell(pagefp) > 2)  {
			rewind(pagefp);
			snprintf(buf, sizeof buf, "SEXP %s|-", touser);
			serv_puts(buf);
			serv_gets(buf);
			if (buf[0]=='4') {
	   			strcpy(last_paged, touser);
				while (fgets(buf, 256, pagefp) != NULL) {
					buf[strlen(buf)-1] = 0;
					serv_puts(buf);
					}
				fclose(pagefp);
				serv_puts("000");
				printf("Message sent.\n");
			}
			else {
				printf("%s\n", &buf[4]);
			}
		} 
		else {
			printf("No message sent.\n");
		}
	}
}



