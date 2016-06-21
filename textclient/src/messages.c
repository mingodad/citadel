/*
 * Text client functions for reading and writing of messages
 *
 * Copyright (c) 1987-2016 by the citadel.org team
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
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <sys/wait.h>
#include <sys/stat.h>

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

#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif

#include <stdarg.h>
#include <libcitadel.h>
#include "citadel_ipc.h"
#include "citadel_decls.h"
#include "messages.h"
#include "commands.h"
#include "tuiconfig.h"
#include "rooms.h"
#include "screen.h"

#define MAXWORDBUF SIZ
#define NO_REPLY_TO	"nobody ... xxxxxx"

char reply_to[SIZ];
char reply_subject[SIZ];
char reply_references[SIZ];
char reply_inreplyto[SIZ];

struct cittext {
	struct cittext *next;
	char text[MAXWORDBUF];
};

void stty_ctdl(int cmd);
int haschar(const char *st, int ch);
int file_checksum(char *filename);
void progress(CtdlIPC* ipc, unsigned long curr, unsigned long cmax);

unsigned long *msg_arr = NULL;
int msg_arr_size = 0;
int num_msgs;
extern char room_name[];
extern char tempdir[];
extern unsigned room_flags;
extern unsigned room_flags2;
extern int entmsg_ok;
extern long highest_msg_read;
extern char temp[];
extern char temp2[];
extern int screenwidth;
extern int screenheight;
extern long maxmsgnum;
extern char is_mail;
extern char is_aide;
extern char is_room_aide;
extern char fullname[];
extern char axlevel;
extern unsigned userflags;
extern char sigcaught;
extern char printcmd[];
extern int rc_allow_attachments;
extern int rc_display_message_numbers;
extern int rc_force_mail_prompts;
extern int editor_pid;
extern CtdlIPC *ipc_for_signal_handlers;	/* KLUDGE cover your eyes */
int num_urls = 0;
char urls[MAXURLS][SIZ];
char imagecmd[SIZ];
int has_images = 0;				/* Current msg has images */
struct parts *last_message_parts = NULL;	/* Parts from last msg */



void ka_sigcatch(int signum)
{
	alarm(S_KEEPALIVE);
	signal(SIGALRM, ka_sigcatch);
	CtdlIPCNoop(ipc_for_signal_handlers);
}


/*
 * server keep-alive version of wait() (needed for external editor)
 */
pid_t ka_wait(int *kstatus)
{
	pid_t p;

	alarm(S_KEEPALIVE);
	signal(SIGALRM, ka_sigcatch);
	do {
		errno = 0;
		p = wait(kstatus);
	} while (errno == EINTR);
	signal(SIGALRM, SIG_IGN);
	alarm(0);
	return (p);
}


/*
 * version of system() that uses ka_wait()
 */
int ka_system(char *shc)
{
	pid_t childpid;
	pid_t waitpid;
	int retcode;

	childpid = fork();
	if (childpid < 0) {
		color(BRIGHT_RED);
		perror("Cannot fork");
		color(DIM_WHITE);
		return ((pid_t) childpid);
	}

	if (childpid == 0) {
		execlp("/bin/sh", "sh", "-c", shc, NULL);
		exit(127);
	}

	if (childpid > 0) {
		do {
			waitpid = ka_wait(&retcode);
		} while (waitpid != childpid);
		return (retcode);
	}

	return (-1);
}



/*
 * add a newline to the buffer...
 */
void add_newline(struct cittext *textlist)
{
	struct cittext *ptr;

	ptr = textlist;
	while (ptr->next != NULL)
		ptr = ptr->next;

	while (ptr->text[strlen(ptr->text) - 1] == 32)
		ptr->text[strlen(ptr->text) - 1] = 0;
	/* strcat(ptr->text,"\n"); */

	ptr->next = (struct cittext *)
	    malloc(sizeof(struct cittext));
	ptr = ptr->next;
	ptr->next = NULL;
	strcpy(ptr->text, "");
}


/*
 * add a word to the buffer...
 */
void add_word(struct cittext *textlist, char *wordbuf)
{
	struct cittext *ptr;

	ptr = textlist;
	while (ptr->next != NULL)
		ptr = ptr->next;

	if (3 + strlen(ptr->text) + strlen(wordbuf) > screenwidth) {
		ptr->next = (struct cittext *)
		    malloc(sizeof(struct cittext));
		ptr = ptr->next;
		ptr->next = NULL;
		strcpy(ptr->text, "");
	}

	strcat(ptr->text, wordbuf);
	strcat(ptr->text, " ");
}


/*
 * begin editing of an opened file pointed to by fp
 */
void citedit(FILE *fp)
{
	int a, prev, finished, b, last_space;
	int appending = 0;
	struct cittext *textlist = NULL;
	struct cittext *ptr;
	char wordbuf[MAXWORDBUF];
	int rv = 0;

	/* first, load the text into the buffer */
	fseek(fp, 0L, 0);
	textlist = (struct cittext *) malloc(sizeof(struct cittext));
	textlist->next = NULL;
	strcpy(textlist->text, "");

	strcpy(wordbuf, "");
	prev = (-1);
	while (a = getc(fp), a >= 0) {
		appending = 1;
		if ((a == 32) || (a == 9) || (a == 13) || (a == 10)) {
			add_word(textlist, wordbuf);
			strcpy(wordbuf, "");
			if ((prev == 13) || (prev == 10)) {
				add_word(textlist, "\n");
				add_newline(textlist);
				add_word(textlist, "");
			}
		} else {
			wordbuf[strlen(wordbuf) + 1] = 0;
			wordbuf[strlen(wordbuf)] = a;
		}
		if (strlen(wordbuf) + 3 > screenwidth) {
			add_word(textlist, wordbuf);
			strcpy(wordbuf, "");
		}
		prev = a;
	}

	/* get text */
	finished = 0;
	prev = (appending ? 13 : (-1));
	strcpy(wordbuf, "");
	async_ka_start();
	do {
		a = inkey();
		if (a == 10)
			a = 13;
		if (a == 9)
			a = 32;
		if (a == 127)
			a = 8;

		if ((a != 32) && (prev == 13)) {
			add_word(textlist, "\n");
			scr_printf(" ");
		}

		if ((a == 32) && (prev == 13)) {
			add_word(textlist, "\n");
			add_newline(textlist);
		}

		if (a == 8) {
			if (!IsEmptyStr(wordbuf)) {
				wordbuf[strlen(wordbuf) - 1] = 0;
				scr_putc(8);
				scr_putc(32);
				scr_putc(8);
			}
		} else if (a == 23) {
			do {
				wordbuf[strlen(wordbuf) - 1] = 0;
				scr_putc(8);
				scr_putc(32);
				scr_putc(8);
			} while (!IsEmptyStr(wordbuf) && wordbuf[strlen(wordbuf) - 1] != ' ');
		} else if (a == 13) {
			scr_printf("\n");
			if (IsEmptyStr(wordbuf))
				finished = 1;
			else {
				for (b = 0; b < strlen(wordbuf); ++b)
					if (wordbuf[b] == 32) {
						wordbuf[b] = 0;
						add_word(textlist,
							 wordbuf);
						strcpy(wordbuf,
						       &wordbuf[b + 1]);
						b = 0;
					}
				add_word(textlist, wordbuf);
				strcpy(wordbuf, "");
			}
		} else {
			scr_putc(a);
			wordbuf[strlen(wordbuf) + 1] = 0;
			wordbuf[strlen(wordbuf)] = a;
		}
		if ((strlen(wordbuf) + 3) > screenwidth) {
			last_space = (-1);
			for (b = 0; b < strlen(wordbuf); ++b)
				if (wordbuf[b] == 32)
					last_space = b;
			if (last_space >= 0) {
				for (b = 0; b < strlen(wordbuf); ++b)
					if (wordbuf[b] == 32) {
						wordbuf[b] = 0;
						add_word(textlist,
							 wordbuf);
						strcpy(wordbuf,
						       &wordbuf[b + 1]);
						b = 0;
					}
				for (b = 0; b < strlen(wordbuf); ++b) {
					scr_putc(8);
					scr_putc(32);
					scr_putc(8);
				}
				scr_printf("\n%s", wordbuf);
			} else {
				add_word(textlist, wordbuf);
				strcpy(wordbuf, "");
				scr_printf("\n");
			}
		}
		prev = a;
	} while (finished == 0);
	async_ka_end();

	/* write the buffer back to disk */
	fseek(fp, 0L, 0);
	for (ptr = textlist; ptr != NULL; ptr = ptr->next) {
		fprintf(fp, "%s", ptr->text);
	}
	putc(10, fp);
	fflush(fp);
	rv = ftruncate(fileno(fp), ftell(fp));
	if (rv < 0)
		scr_printf("failed to set message buffer: %s\n", strerror(errno));

	
	/* and deallocate the memory we used */
	while (textlist != NULL) {
		ptr = textlist->next;
		free(textlist);
		textlist = ptr;
	}
}


/*
 * Free the struct parts
 */
void free_parts(struct parts *p)
{
	struct parts *a_part = p;

	while (a_part) {
		struct parts *q;

		q = a_part;
		a_part = a_part->next;
		free(q);
	}
}


/*
 * This is a mini RFC2047 decoder.
 * It only handles strings encoded from UTF-8 as Quoted-printable.
 */
void mini_2047_decode(char *s) {
	if (!s) return;

	char *qstart = strstr(s, "=?UTF-8?Q?");
	if (!qstart) return;

	char *qend = strstr(s, "?=");
	if (!qend) return;

	if (qend <= qstart) return;

	strcpy(qstart, &qstart[10]);
	qend -= 10;

	char *p = qstart;
	while (p < qend) {

		if (p[0] == '=') {

			char ch[3];
			ch[0] = p[1];
			ch[1] = p[2];
			ch[2] = p[3];
			int c;
			sscanf(ch, "%02x", &c);
			p[0] = c;
			strcpy(&p[1], &p[3]);
			qend -= 2;
		}

		if (p[0] == '_') {
			p[0] = ' ';
		}
		
		++p;
	}

	strcpy(qend, &qend[2]);
}

/*
 * Read a message from the server
 */
int read_message(CtdlIPC *ipc,
	long num,   /* message number */
	int pagin, /* 0 = normal read, 1 = read with pagination, 2 = header */
	FILE *dest) /* Destination file, NULL for screen */
{
	char buf[SIZ];
	char now[SIZ];
	int format_type = 0;
	int fr = 0;
	int nhdr = 0;
	struct ctdlipcmessage *message = NULL;
	int r;				/* IPC response code */
	char *converted_text = NULL;
	char *lineptr;
	char *nextline;
	char *searchptr;
	int i;
	char ch;
	int linelen;
	int final_line_is_blank = 0;

	has_images = 0;

	sigcaught = 0;
	stty_ctdl(1);

	strcpy(reply_to, NO_REPLY_TO);
	strcpy(reply_subject, "");
	strcpy(reply_references, "");
	strcpy(reply_inreplyto, "");

	r = CtdlIPCGetSingleMessage(ipc, num, (pagin == READ_HEADER ? 1 : 0), 4, &message, buf);
	if (r / 100 != 1) {
		scr_printf("*** msg #%ld: %d %s\n", num, r, buf);
		stty_ctdl(0);
		free(message->text);
		free_parts(message->attachments);
		free(message);
		return (0);
	}

	if (dest) {
		fprintf(dest, "\n ");
	} else {
		scr_printf("\n");
		if (pagin != 2)
			scr_printf(" ");
	}
	if (pagin == 1 && !dest) {
		color(BRIGHT_CYAN);
	}

	/* View headers only */
	if (pagin == 2) {
		scr_printf("nhdr=%s\nfrom=%s\ntype=%d\nmsgn=%s\n",
				message->nhdr ? "yes" : "no",
				message->author, message->type,
				message->msgid);
		if (!IsEmptyStr(message->subject)) {
			scr_printf("subj=%s\n", message->subject);
		}
		if (!IsEmptyStr(message->email)) {
			scr_printf("rfca=%s\n", message->email);
		}
		scr_printf("hnod=%s\nroom=%s\nnode=%s\ntime=%s",
				message->hnod, message->room,
				message->node, 
				asctime(localtime(&message->time)));
		if (!IsEmptyStr(message->recipient)) {
			scr_printf("rcpt=%s\n", message->recipient);
		}
		if (message->attachments) {
			struct parts *ptr;

			for (ptr = message->attachments; ptr; ptr = ptr->next) {
				scr_printf("part=%s|%s|%s|%s|%s|%ld\n",
					ptr->name, ptr->filename, ptr->number,
					ptr->disposition, ptr->mimetype,
					ptr->length
				);
			}
		}
		scr_printf("\n");
		stty_ctdl(0);
		free(message->text);
		free_parts(message->attachments);
		free(message);
		return (0);
	}

	if (rc_display_message_numbers) {
		if (dest) {
			fprintf(dest, "[#%s] ", message->msgid);
		} else {
			color(DIM_WHITE);
			scr_printf("[");
			color(BRIGHT_WHITE);
			scr_printf("#%s", message->msgid);
			color(DIM_WHITE);
			scr_printf("] ");
		}
	}
	if (nhdr == 1 && !is_room_aide) {
		if (dest) {
			fprintf(dest, " ****");
		} else {
			scr_printf(" ****");
		}
	} else {
		fmt_date(now, sizeof now, message->time, 0);
		if (dest) {
			fprintf(dest, "%s from %s ", now, message->author);
			if (!IsEmptyStr(message->email)) {
				fprintf(dest, "<%s> ", message->email);
			}
		} else {
			color(BRIGHT_CYAN);
			scr_printf("%s ", now);
			color(DIM_WHITE);
			scr_printf("from ");
			color(BRIGHT_CYAN);
			scr_printf("%s ", message->author);
			if (!IsEmptyStr(message->email)) {
				color(DIM_WHITE);
				scr_printf("<");
				color(BRIGHT_BLUE);
				scr_printf("%s", message->email);
					color(DIM_WHITE);
				scr_printf("> ");
			}
		}
		if (!IsEmptyStr(message->node)) {
			if ((room_flags & QR_NETWORK)
			    || ((strcasecmp(message->node, ipc->ServInfo.nodename)
			     && (strcasecmp(message->node, ipc->ServInfo.fqdn))))) {
				if (IsEmptyStr(message->email)) {
					if (dest) {
						fprintf(dest, "@%s ", message->node);
					} else {
						color(DIM_WHITE);
						scr_printf("@");
						color(BRIGHT_YELLOW);
						scr_printf("%s ", message->node);
					}
				}
			}
		}
		if (strcasecmp(message->hnod, ipc->ServInfo.humannode)
		    && (!IsEmptyStr(message->hnod)) && (IsEmptyStr(message->email))) {
			if (dest) {
				fprintf(dest, "(%s) ", message->hnod);
			} else {
				color(DIM_WHITE);
				scr_printf("(");
				color(BRIGHT_WHITE);
				scr_printf("%s", message->hnod);
				color(DIM_WHITE);
				scr_printf(") ");
			}
		}
		if (strcasecmp(message->room, room_name) && (IsEmptyStr(message->email))) {
			if (dest) {
				fprintf(dest, "in %s> ", message->room);
			} else {
				color(DIM_WHITE);
				scr_printf("in ");
				color(BRIGHT_MAGENTA);
				scr_printf("%s> ", message->room);
			}
		}
		if (!IsEmptyStr(message->recipient)) {
			if (dest) {
				fprintf(dest, "to %s ", message->recipient);
			} else {
				color(DIM_WHITE);
				scr_printf("to ");
				color(BRIGHT_CYAN);
				scr_printf("%s ", message->recipient);
			}
		}
	}
	
	if (dest) {
		fprintf(dest, "\n");
	} else {
		scr_printf("\n");
	}

	/* Set the reply-to address to an Internet e-mail address if possible
	 */
	if ((message->email != NULL) && (!IsEmptyStr(message->email))) {
		if (!IsEmptyStr(message->author)) {
			snprintf(reply_to, sizeof reply_to, "%s <%s>", message->author, message->email);
		}
		else {
			safestrncpy(reply_to, message->email, sizeof reply_to);
		}
	}

	/* But if we can't do that, set it to a Citadel address.
	 */
	if (!strcmp(reply_to, NO_REPLY_TO)) {
		snprintf(reply_to, sizeof(reply_to), "%s @ %s",
			 message->author, message->node);
	}

	if (message->msgid != NULL) {
		safestrncpy(reply_inreplyto, message->msgid, sizeof reply_inreplyto);
	}

	if (message->references != NULL) if (!IsEmptyStr(message->references)) {
		safestrncpy(reply_references, message->references, sizeof reply_references);
	}

	if (message->subject != NULL) {
		safestrncpy(reply_subject, message->subject, sizeof reply_subject);
		if (!IsEmptyStr(message->subject)) {
			if (dest) {
				fprintf(dest, "Subject: %s\n", message->subject);
			} else {
				color(DIM_WHITE);
				scr_printf("Subject: ");
				color(BRIGHT_CYAN);
				mini_2047_decode(message->subject);
				scr_printf("%s\n", message->subject);
			}
		}
	}

	if (pagin == 1 && !dest) {
		color(BRIGHT_WHITE);
	}

	/******* end of header output, start of message text output *******/

	/*
	 * Convert HTML to plain text, formatting for the actual width
	 * of the client screen.
	 */
	if (!strcasecmp(message->content_type, "text/html")) {
		converted_text = html_to_ascii(message->text, 0, screenwidth, 0);
		if (converted_text != NULL) {
			free(message->text);
			message->text = converted_text;
			format_type = 1;
		}
	}

	/* Text/plain is a different type */
	if (!strcasecmp(message->content_type, "text/plain")) {
		format_type = 1;
	}

	/* Extract URL's */
	static char *urlprefixes[] = {
		"http://",
		"https://",
		"ftp://"
	};
	int p = 0;
	num_urls = 0;	/* Start with a clean slate */
	for (p=0; p<(sizeof urlprefixes / sizeof(char *)); ++p) {
		searchptr = message->text;
		while ( (searchptr != NULL) && (num_urls < MAXURLS) ) {
			searchptr = strstr(searchptr, urlprefixes[p]);
			if (searchptr != NULL) {
				safestrncpy(urls[num_urls], searchptr, sizeof(urls[num_urls]));
				for (i = 0; i < strlen(urls[num_urls]); i++) {
					ch = urls[num_urls][i];
					if (ch == '>' || ch == '\"' || ch == ')' ||
					    ch == ' ' || ch == '\n') {
						urls[num_urls][i] = 0;
						break;
					}
				}
				num_urls++;
				++searchptr;
			}
		}
	}

	/*
	 * Here we go
	 */
	if (format_type == 0) {
		fr = fmout(screenwidth, NULL, message->text, dest, 1);
	} else {
		/* renderer for text/plain */

		lineptr = message->text;

		do {
			nextline = strchr(lineptr, '\n');
			if (nextline != NULL) {
				*nextline = 0;
				++nextline;
				if (*nextline == 0) nextline = NULL;
			}

			if (sigcaught == 0) {
				linelen = strlen(lineptr);
				if (linelen && (lineptr[linelen-1] == '\r')) {
					lineptr[--linelen] = 0;
				}
				if (dest) {
					fprintf(dest, "%s\n", lineptr);
				} else {
					scr_printf("%s\n", lineptr);
				}
			}
			if (lineptr[0] == 0) final_line_is_blank = 1;
			else final_line_is_blank = 0;
			lineptr = nextline;
		} while (nextline);
		fr = sigcaught;
	}
	if (!final_line_is_blank) {
		if (dest) {
			fprintf(dest, "\n");
		}
		else {
			scr_printf("\n");
			fr = sigcaught;		
		}
	}

	/* Enumerate any attachments */
	if ( (pagin == 1) && (message->attachments) ) {
		struct parts *ptr;

		for (ptr = message->attachments; ptr; ptr = ptr->next) {
			if ( (!strcasecmp(ptr->disposition, "attachment"))
			   || (!strcasecmp(ptr->disposition, "inline"))
			   || (!strcasecmp(ptr->disposition, ""))
			) {
				if ( (strcasecmp(ptr->number, message->mime_chosen))
				   && (!IsEmptyStr(ptr->mimetype))
				) {
					color(DIM_WHITE);
					scr_printf("Part ");
					color(BRIGHT_MAGENTA);
					scr_printf("%s", ptr->number);
					color(DIM_WHITE);
					scr_printf(": ");
					color(BRIGHT_CYAN);
					scr_printf("%s", ptr->filename);
					color(DIM_WHITE);
					scr_printf(" (%s, %ld bytes)\n", ptr->mimetype, ptr->length);
					if (!strncmp(ptr->mimetype, "image/", 6)) {
						has_images++;
					}
				}
			}
		}
	}

	/* Save the attachments info for later */
	last_message_parts = message->attachments;

	/* Now we're done */
	free(message->text);
	free(message);

	if (pagin == 1 && !dest)
		color(DIM_WHITE);
	stty_ctdl(0);
	return (fr);
}

/*
 * replace string function for the built-in editor
 */
void replace_string(char *filename, long int startpos)
{
	char buf[512];
	char srch_str[128];
	char rplc_str[128];
	FILE *fp;
	int a;
	long rpos, wpos;
	char *ptr;
	int substitutions = 0;
	long msglen = 0L;
	int rv;

	newprompt("Enter text to be replaced: ", srch_str, (sizeof(srch_str)-1) );
	if (IsEmptyStr(srch_str)) {
		return;
	}

	newprompt("Enter text to replace it with: ", rplc_str, (sizeof(rplc_str)-1) );

	fp = fopen(filename, "r+");
	if (fp == NULL) {
		return;
	}

	wpos = startpos;
	fseek(fp, startpos, 0);
	strcpy(buf, "");
	while (a = getc(fp), a > 0) {
		++msglen;
		buf[strlen(buf) + 1] = 0;
		buf[strlen(buf)] = a;
		if (strlen(buf) >= strlen(srch_str)) {
			ptr = (&buf[strlen(buf) - strlen(srch_str)]);
			if (!strncmp(ptr, srch_str, strlen(srch_str))) {
				strcpy(ptr, rplc_str);
				++substitutions;
			}
		}
		if (strlen(buf) > 384) {
			rpos = ftell(fp);
			fseek(fp, wpos, 0);
			rv = fwrite((char *) buf, 128, 1, fp);
			if (rv < 0) {
				scr_printf("failed to replace string: %s\n", strerror(errno));
				break; /*whoopsi! */
			}
			strcpy(buf, &buf[128]);
			wpos = ftell(fp);
			fseek(fp, rpos, 0);
		}
	}
	fseek(fp, wpos, 0);
	if (!IsEmptyStr(buf)) {
		rv = fwrite((char *) buf, strlen(buf), 1, fp);
	}
	wpos = ftell(fp);
	fclose(fp);
	rv = truncate(filename, wpos);
	scr_printf("<R>eplace made %d substitution(s).\n\n", substitutions);
}

/*
 * Function to begin composing a new message
 */
int client_make_message(CtdlIPC *ipc,
			char *filename,		/* temporary file name */
			char *recipient,	/* NULL if it's not mail */
			int is_anonymous,
			int format_type,
			int mode,
			char *subject, 		/* buffer to store subject line */
			int subject_required
) {
	FILE *fp;
	int a, b, e_ex_code;
	long beg;
	char datestr[SIZ];
	char header[SIZ];
	int cksum = 0;

	if ( (mode == 2) && (IsEmptyStr(editor_path)) ) {
		scr_printf("*** No editor available; using built-in editor.\n");
		mode = 0;
	}

	fmt_date(datestr, sizeof datestr, time(NULL), 0);
	header[0] = 0;

	if (room_flags & QR_ANONONLY && !recipient) {
		snprintf(header, sizeof header, " ****");
	}
	else {
		snprintf(header, sizeof header,
			" %s from %s",
			datestr,
			(is_anonymous ? "[anonymous]" : fullname)
			);
		if (!IsEmptyStr(recipient)) {
			size_t tmp = strlen(header);
			snprintf(&header[tmp], sizeof header - tmp,
				" to %s", recipient);
		}
	}
	scr_printf("%s\n", header);
	if (subject != NULL) if (!IsEmptyStr(subject)) {
		scr_printf("Subject: %s\n", subject);
	}
	
	if ( (subject_required) && (IsEmptyStr(subject)) ) {
		newprompt("Subject: ", subject, 70);
	}

	if (mode == 1) {
		scr_printf("(Press ctrl-d when finished)\n");
	}

	if (mode == 0) {
		fp = fopen(filename, "r");
		if (fp != NULL) {
			fmout(screenwidth, fp, NULL, NULL, 0);
			beg = ftell(fp);
			if (beg < 0)
				scr_printf("failed to get stream position %s\n", 
					   strerror(errno));
			fclose(fp);
		} else {
			fp = fopen(filename, "w");
			if (fp == NULL) {
				scr_printf("*** Error opening temp file!\n    %s: %s\n",
					filename, strerror(errno)
				);
			return(1);
			}
			fclose(fp);
		}
	}

ME1:	switch (mode) {

	case 0:
		fp = fopen(filename, "r+");
		if (fp == NULL) {
			scr_printf("*** Error opening temp file!\n    %s: %s\n",
				filename, strerror(errno)
			);
			return(1);
		}
		citedit(fp);
		fclose(fp);
		goto MECR;

	case 1:
		fp = fopen(filename, "a");
		if (fp == NULL) {
			scr_printf("*** Error opening temp file!\n"
				"    %s: %s\n",
				filename, strerror(errno));
			return(1);
		}
		do {
			a = inkey();
			if (a == 255)
				a = 32;
			if (a == 13)
				a = 10;
			if (a != 4) {
				putc(a, fp);
				scr_putc(a);
			}
			if (a == 10)
				scr_putc(10);
		} while (a != 4);
		fclose(fp);
		break;

	case 2:
	default:	/* allow 2+ modes */
		e_ex_code = 1;	/* start with a failed exit code */
		stty_ctdl(SB_RESTORE);
		editor_pid = fork();
		cksum = file_checksum(filename);
		if (editor_pid == 0) {
			char tmp[SIZ];

			chmod(filename, 0600);
			snprintf(tmp, sizeof tmp, "WINDOW_TITLE=%s", header);
			putenv(tmp);
			execlp(editor_path, editor_path, filename, NULL);
			exit(1);
		}
		if (editor_pid > 0)
			do {
				e_ex_code = 0;
				b = ka_wait(&e_ex_code);
			} while ((b != editor_pid) && (b >= 0));
		editor_pid = (-1);
		stty_ctdl(0);
		break;
	}

MECR:	if (mode >= 2) {
		if (file_checksum(filename) == cksum) {
			scr_printf("*** Aborted message.\n");
			e_ex_code = 1;
		}
		if (e_ex_code == 0) {
			goto MEFIN;
		}
		goto MEABT2;
	}

	b = keymenu("Entry command (? for options)",
		"<A>bort|"
		"<C>ontinue|"
		"<S>ave message|"
		"<P>rint formatted|"
		"add s<U>bject|"
		"<R>eplace string|"
		"<H>old message"
	);

	if (b == 'a') goto MEABT;
	if (b == 'c') goto ME1;
	if (b == 's') goto MEFIN;
	if (b == 'p') {
		scr_printf(" %s from %s", datestr, fullname);
		if (!IsEmptyStr(recipient)) {
			scr_printf(" to %s", recipient);
		}
		scr_printf("\n");
		if (subject != NULL) if (!IsEmptyStr(subject)) {
			scr_printf("Subject: %s\n", subject);
		}
		fp = fopen(filename, "r");
		if (fp != NULL) {
			fmout(screenwidth, fp, NULL, NULL, 0);
			beg = ftell(fp);
			if (beg < 0)
				scr_printf("failed to get stream position %s\n", 
					   strerror(errno));
			fclose(fp);
		}
		goto MECR;
	}
	if (b == 'r') {
		replace_string(filename, 0L);
		goto MECR;
	}
	if (b == 'h') {
		return (2);
	}
	if (b == 'u') {
		if (subject != NULL) {
			newprompt("Subject: ", subject, 70);
		}
		goto MECR;
	}

MEFIN:	return (0);

MEABT:	scr_printf("Are you sure? ");
	if (yesno() == 0) {
		goto ME1;
	}
MEABT2:	unlink(filename);
	return (2);
}


/*
 * Make sure there's room in msg_arr[] for at least one more.
 */
void check_msg_arr_size(void) {
	if ((num_msgs + 1) > msg_arr_size) {
		msg_arr_size += 512;
		msg_arr = realloc(msg_arr,
			((sizeof(long)) * msg_arr_size) );
	}
}


/*
 * break_big_lines()  -  break up lines that are >1024 characters
 *                       otherwise the server will truncate
 */
void break_big_lines(char *msg) {
	char *ptr;
	char *break_here;

	if (msg == NULL) {
		return;
	}

	ptr = msg;
	while (strlen(ptr) > 1000) {
		break_here = strchr(&ptr[900], ' ');
		if ((break_here == NULL) || (break_here > &ptr[999])) {
			break_here = &ptr[999];
		}
		*break_here = '\n';
		ptr = break_here++;
	}
}


/*
 * entmsg()  -  edit and create a message
 *              returns 0 if message was saved
 */
int entmsg(CtdlIPC *ipc,
		int is_reply,	/* nonzero if this was a <R>eply command */
		int c,		/* mode */
		int masquerade	/* prompt for a non-default display name? */
) {
	char buf[SIZ];
	int a, b;
	int need_recp = 0;
	int mode;
	long highmsg = 0L;
	FILE *fp;
	char subject[SIZ];
	struct ctdlipcmessage message;
	unsigned long *msgarr = NULL;
	int r;			/* IPC response code */
	int subject_required = 0;

	if (!entmsg_ok) {
		scr_printf("You may not enter messages in this type of room.\n");
		return(1);
	}

	if (c > 0) {
		mode = 1;
	}
	else {
		mode = 0;
	}

	strcpy(subject, "");

	/*
	 * First, check to see if we have permission to enter a message in
	 * this room.  The server will return an error code if we can't.
	 */
	strcpy(message.recipient, "");
	strcpy(message.author, "");
	strcpy(message.subject, "");
	strcpy(message.references, "");
	message.text = "";		/* point to "", changes later */
	message.anonymous = 0;
	message.type = mode;

	if (masquerade) {
		newprompt("Display name for this message: ", message.author, 40);
	}

	if (is_reply) {

		if (!IsEmptyStr(reply_subject)) {
			if (!strncasecmp(reply_subject,
			   "Re: ", 3)) {
				strcpy(message.subject, reply_subject);
			}
			else {
				snprintf(message.subject,
					sizeof message.subject,
					"Re: %s",
					reply_subject);
			}
		}

		/* Trim down excessively long lists of thread references.  We eliminate the
		 * second one in the list so that the thread root remains intact.
		 */
		int rrtok = num_tokens(reply_references, '|');
		int rrlen = strlen(reply_references);
		if ( ((rrtok >= 3) && (rrlen > 900)) || (rrtok > 10) ) {
			remove_token(reply_references, 1, '|');
		}

		snprintf(message.references, sizeof message.references, "%s%s%s",
			reply_references,
			(IsEmptyStr(reply_references) ? "" : "|"),
			reply_inreplyto
		);
	}

	r = CtdlIPCPostMessage(ipc, 0, &subject_required, &message, buf);

	if (r / 100 != 2 && r / 10 != 57) {
		scr_printf("%s\n", buf);
		return (1);
	}

	/* Error code 570 is special.  It means that we CAN enter a message
	 * in this room, but a recipient needs to be specified.
	 */
	need_recp = 0;
	if (r / 10 == 57) {
		need_recp = 1;
	}

	/* If the user is a dumbass, tell them how to type. */
	if ((userflags & US_EXPERT) == 0) {
		formout(ipc, "entermsg");
	}

	/* Handle the selection of a recipient, if necessary. */
	strcpy(buf, "");
	if (need_recp == 1) {
		if (axlevel >= AxProbU) {
			if (is_reply) {
				strcpy(buf, reply_to);
			} else {
				newprompt("Enter recipient: ", buf, SIZ-100);
				if (IsEmptyStr(buf)) {
					return (1);
				}
			}
		} else
			strcpy(buf, "sysop");
	}
	strcpy(message.recipient, buf);

	if (room_flags & QR_ANONOPT) {
		scr_printf("Anonymous (Y/N)? ");
		if (yesno() == 1)
			message.anonymous = 1;
	}

	/* If it's mail, we've got to check the validity of the recipient... */
	if (!IsEmptyStr(message.recipient)) {
		r = CtdlIPCPostMessage(ipc, 0, &subject_required,  &message, buf);
		if (r / 100 != 2) {
			scr_printf("%s\n", buf);
			return (1);
		}
	}

	/* Learn the number of the newest message in in the room, so we can
 	 * tell upon saving whether someone else has posted too.
 	 */
	num_msgs = 0;
	r = CtdlIPCGetMessages(ipc, LastMessages, 1, NULL, &msgarr, buf);
	if (r / 100 != 1) {
		scr_printf("%s\n", buf);
	} else {
		for (num_msgs = 0; msgarr[num_msgs]; num_msgs++)
			;
	}

	/* Now compose the message... */
	if (client_make_message(ipc, temp, message.recipient,
	   message.anonymous, 0, c, message.subject, subject_required) != 0) {
	    if (msgarr) free(msgarr);	
		return (2);
	}

	/* Reopen the temp file that was created, so we can send it */
	fp = fopen(temp, "r");

	if (!fp || !(message.text = load_message_from_file(fp))) {
		scr_printf("*** Internal error while trying to save message!\n"
			"%s: %s\n",
			temp, strerror(errno));
		unlink(temp);
		return(errno);
	}

	if (fp) fclose(fp);

	/* Break lines that are >1024 characters, otherwise the server
	 * will truncate them.
	 */
	break_big_lines(message.text);

	/* Transmit message to the server */
	r = CtdlIPCPostMessage(ipc, 1, NULL, &message, buf);
	if (r / 100 != 4) {
		scr_printf("%s\n", buf);
		return (1);
	}

	/* Yes, unlink it now, so it doesn't stick around if we crash */
	unlink(temp);

	if (num_msgs >= 1) highmsg = msgarr[num_msgs - 1];

	if (msgarr) free(msgarr);
	msgarr = NULL;
	r = CtdlIPCGetMessages(ipc, NewMessages, 0, NULL, &msgarr, buf);
	if (r / 100 != 1) {
		scr_printf("%s\n", buf);
	} else {
		for (num_msgs = 0; msgarr[num_msgs]; num_msgs++)
			;
	}

	/* get new highest message number in room to set lrp for goto... */
	maxmsgnum = msgarr[num_msgs - 1];

	/* now see if anyone else has posted in here */
	b = (-1);
	for (a = 0; a < num_msgs; ++a) {
		if (msgarr[a] > highmsg) {
			++b;
		}
	}
	if (msgarr) free(msgarr);
	msgarr = NULL;

	/* In the Mail> room, this algorithm always counts one message
	 * higher than in public rooms, so we decrement it by one.
	 */
	if (need_recp) {
		--b;
	}

	if (b == 1) {
		scr_printf("*** 1 additional message has been entered "
			"in this room by another user.\n");
	}
	else if (b > 1) {
		scr_printf("*** %d additional messages have been entered "
			"in this room by other users.\n", b);
	}
    free(message.text);

	return(0);
}

/*
 * Do editing on a quoted file
 */
void process_quote(void)
{
	FILE *qfile, *tfile;
	char buf[128];
	int line, qstart, qend;

	/* Unlink the second temp file as soon as it's opened, so it'll get
	 * deleted even if the program dies
	 */
	qfile = fopen(temp2, "r");
	unlink(temp2);

	/* Display the quotable text with line numbers added */
	line = 0;
	if (fgets(buf, 128, qfile) == NULL) {
		/* we're skipping a line here */
	}
	while (fgets(buf, 128, qfile) != NULL) {
		scr_printf("%3d %s", ++line, buf);
	}

	qstart = intprompt("Begin quoting at", 1, 1, line);
	qend = intprompt("  End quoting at", line, qstart, line);

	rewind(qfile);
	line = 0;
	if (fgets(buf, 128, qfile) == NULL) {
		/* we're skipping a line here */
	}
	tfile = fopen(temp, "w");
	while (fgets(buf, 128, qfile) != NULL) {
		if ((++line >= qstart) && (line <= qend))
			fprintf(tfile, " >%s", buf);
	}
	fprintf(tfile, " \n");
	fclose(qfile);
	fclose(tfile);
	chmod(temp, 0666);
}



/*
 * List the URL's which were embedded in the previous message
 */
void list_urls(CtdlIPC *ipc)
{
	int i;
	char cmd[SIZ];
	int rv;

	if (num_urls == 0) {
		scr_printf("There were no URL's in the previous message.\n\n");
		return;
	}

	for (i = 0; i < num_urls; ++i) {
		scr_printf("%3d %s\n", i + 1, urls[i]);
	}

	if ((i = num_urls) != 1)
		i = intprompt("Display which one", 1, 1, num_urls);

	snprintf(cmd, sizeof cmd, rc_url_cmd, urls[i - 1]);
	rv = system(cmd);
	if (rv != 0) 
		scr_printf("failed to '%s' by %d\n", cmd, rv);
	scr_printf("\n");
}


/*
 * Run image viewer in background
 */
int do_image_view(const char *filename)
{
	char cmd[SIZ];
	pid_t childpid;

	snprintf(cmd, sizeof cmd, imagecmd, filename);
	childpid = fork();
	if (childpid < 0) {
		unlink(filename);
		return childpid;
	}

	if (childpid == 0) {
		int retcode;
		pid_t grandchildpid;

		grandchildpid = fork();
		if (grandchildpid < 0) {
			return grandchildpid;
		}

		if (grandchildpid == 0) {
			int nullfd;
			int outfd = -1;
			int errfd = -1;

			nullfd = open("/dev/null", O_WRONLY);
			if (nullfd > -1) {
				dup2(1, outfd);
				dup2(2, errfd);
				dup2(nullfd, 1);
				dup2(nullfd, 2);
			}
			retcode = system(cmd);
			if (nullfd > -1) {
				dup2(outfd, 1);
				dup2(errfd, 2);
				close(nullfd);
			}
			unlink(filename);
			exit(retcode);
		}

		if (grandchildpid > 0) {
			exit(0);
		}
	}

	if (childpid > 0) {
		int retcode;

		waitpid(childpid, &retcode, 0);
		return retcode;
	}
	
	return -1;
}


/*
 * View an image attached to a message
 */
void image_view(CtdlIPC *ipc, unsigned long msg)
{
	struct parts *ptr = last_message_parts;
	char part[SIZ];
	int found = 0;

	/* Run through available parts */
	for (ptr = last_message_parts; ptr; ptr = ptr->next) {
		if ((!strcasecmp(ptr->disposition, "attachment")
		   || !strcasecmp(ptr->disposition, "inline"))
		   && !strncmp(ptr->mimetype, "image/", 6)) {
			found++;
			if (found == 1) {
				strcpy(part, ptr->number);
			}
		}
	}

	while (found > 0) {
		if (found > 1)
			strprompt("View which part (0 when done)", part, SIZ-1);
		found = -found;
		for (ptr = last_message_parts; ptr; ptr = ptr->next) {
			if ((!strcasecmp(ptr->disposition, "attachment")
			   || !strcasecmp(ptr->disposition, "inline"))
			   && !strncmp(ptr->mimetype, "image/", 6)
			   && !strcasecmp(ptr->number, part)) {
				char tmp[PATH_MAX];
				char buf[SIZ];
				void *file = NULL; /* The downloaded file */
				int r;
	
				/* view image */
				found = -found;
				r = CtdlIPCAttachmentDownload(ipc, msg, ptr->number, &file, progress, buf);
				if (r / 100 != 2) {
					scr_printf("%s\n", buf);
				} else {
					size_t len;
	
					len = (size_t)extract_long(buf, 0);
					progress(ipc, len, len);
					scr_flush();
					CtdlMakeTempFileName(tmp, sizeof tmp);
					strcat(tmp, ptr->filename);
					save_buffer(file, len, tmp);
					free(file);
					do_image_view(tmp);
				}
				break;
			}
		}
		if (found == 1)
			break;
	}
}
 

/*
 * Read the messages in the current room
 */
void readmsgs(CtdlIPC *ipc,
	enum MessageList c,		/* see listing in citadel_ipc.h */
	enum MessageDirection rdir,	/* 1=Forward (-1)=Reverse */
	int q		/* Number of msgs to read (if c==3) */
) {
	int a, e, f, g, start;
	int savedpos;
	int hold_sw = 0;
	char arcflag = 0;
	char quotflag = 0;
	int hold_color = 0;
	char prtfile[PATH_MAX];
	char pagin;
	char cmd[SIZ];
	char targ[ROOMNAMELEN];
	char filename[PATH_MAX];
	char save_to[PATH_MAX];
	void *attachment = NULL;	/* Downloaded attachment */
	FILE *dest = NULL;		/* Alternate destination other than screen */
	int r;				/* IPC response code */
	static int att_seq = 0;		/* Attachment download sequence number */
	int rv = 0;			/* silence the stupid warn_unused_result warnings */

	CtdlMakeTempFileName(prtfile, sizeof prtfile);

	if (msg_arr) {
		free(msg_arr);
		msg_arr = NULL;
	}
	r = CtdlIPCGetMessages(ipc, c, q, NULL, &msg_arr, cmd);
	if (r / 100 != 1) {
		scr_printf("%s\n", cmd);
	} else {
		for (num_msgs = 0; msg_arr[num_msgs]; num_msgs++)
			;
	}

	if (num_msgs == 0) {	/* TODO look at this later */
		if (c == LastMessages) return;
		scr_printf("*** There are no ");
		if (c == NewMessages) scr_printf("new ");
		if (c == OldMessages) scr_printf("old ");
		scr_printf("messages in this room.\n");
		return;
	}

	/* this loop cycles through each message... */
	start = ((rdir == 1) ? 0 : (num_msgs - 1));
	for (a = start; ((a < num_msgs) && (a >= 0)); a = a + rdir) {
		while (msg_arr[a] == 0L) {
			a = a + rdir;
			if ((a == num_msgs) || (a == (-1)))
				return;
		}

RAGAIN:		pagin = ((arcflag == 0)
			 && (quotflag == 0)
			 && (userflags & US_PAGINATOR)) ? 1 : 0;

		/* If we're doing a quote, set the screenwidth to 72 */
		if (quotflag) {
			hold_sw = screenwidth;
			screenwidth = 72;
		}

		/* If printing or archiving, set the screenwidth to 80 */
		if (arcflag) {
			hold_sw = screenwidth;
			screenwidth = 80;
		}

		/* clear parts list */
		free_parts(last_message_parts);
		last_message_parts = NULL;

		/* now read the message... */
		e = read_message(ipc, msg_arr[a], pagin, dest);

		/* ...and set the screenwidth back if we have to */
		if ((quotflag) || (arcflag)) {
			screenwidth = hold_sw;
		}
RMSGREAD:
		highest_msg_read = msg_arr[a];
		if (quotflag) {
			fclose(dest);
			dest = NULL;
			quotflag = 0;
			enable_color = hold_color;
			process_quote();
			e = 'r';
			goto DONE_QUOTING;
		}
		if (arcflag) {
			fclose(dest);
			dest = NULL;
			arcflag = 0;
			enable_color = hold_color;
			f = fork();
			if (f == 0) {
				if (freopen(prtfile, "r", stdin) == NULL) {
					/* we probably should handle the error condition here */
				}
				stty_ctdl(SB_RESTORE);
				ka_system(printcmd);
				stty_ctdl(SB_NO_INTR);
				unlink(prtfile);
				exit(0);
			}
			if (f > 0)
				do {
					g = wait(NULL);
				} while ((g != f) && (g >= 0));
			scr_printf("Message printed.\n");
		}
		if (e == SIGQUIT)
			return;
		if (((userflags & US_NOPROMPT) || (e == SIGINT))
			&& (((room_flags & QR_MAILBOX) == 0)
			|| (rc_force_mail_prompts == 0))) {
			e = 'n';
		} else {
			color(DIM_WHITE);
			scr_printf("(");
			color(BRIGHT_WHITE);
			scr_printf("%d", num_msgs - a - 1);
			color(DIM_WHITE);
			scr_printf(") ");

			keyopt("<B>ack <A>gain <R>eply reply<Q>uoted <N>ext <S>top ");
			if (rc_url_cmd[0] && num_urls)
				keyopt("<U>RLview ");
			if (has_images > 0 && !IsEmptyStr(imagecmd))
				keyopt("<I>mages ");
			keyopt("<?>help -> ");

			do {
				e = (inkey() & 127);
				e = tolower(e);
/* return key same as <N> */ if (e == 10)
					e = 'n';
/* space key same as <N> */ if (e == 32)
					e = 'n';
/* del/move for aides only */
				    if (  (!is_room_aide)
				       && ((room_flags & QR_MAILBOX) == 0)
				       && ((room_flags2 & QR2_COLLABDEL) == 0)
				       ) {
					if ((e == 'd') || (e == 'm'))
						e = 0;
				}
/* print only if available */
				if ((e == 'p') && (IsEmptyStr(printcmd)))
					e = 0;
/* can't file if not allowed */
				    if ((e == 'f')
					&& (rc_allow_attachments == 0))
					e = 0;
/* link only if browser avail*/
				    if ((e == 'u')
					&& (IsEmptyStr(rc_url_cmd)))
					e = 0;
				if ((e == 'i')
					&& (IsEmptyStr(imagecmd) || !has_images))
					e = 0;
			} while ((e != 'a') && (e != 'n') && (e != 's')
				 && (e != 'd') && (e != 'm') && (e != 'p')
				 && (e != 'q') && (e != 'b') && (e != 'h')
				 && (e != 'r') && (e != 'f') && (e != '?')
				 && (e != 'u') && (e != 'c') && (e != 'y')
				 && (e != 'i') && (e != 'o') );
			switch (e) {
			case 's':
				scr_printf("Stop");
				break;
			case 'a':
				scr_printf("Again");
				break;
			case 'd':
				scr_printf("Delete");
				break;
			case 'm':
				scr_printf("Move");
				break;
			case 'c':
				scr_printf("Copy");
				break;
			case 'n':
				scr_printf("Next");
				break;
			case 'p':
				scr_printf("Print");
				break;
			case 'q':
				scr_printf("reply Quoted");
				break;
			case 'b':
				scr_printf("Back");
				break;
			case 'h':
				scr_printf("Header");
				break;
			case 'r':
				scr_printf("Reply");
				break;
			case 'o':
				scr_printf("Open attachments");
				break;
			case 'f':
				scr_printf("File");
				break;
			case 'u':
				scr_printf("URL's");
				break;
			case 'y':
				scr_printf("mY next");
				break;
			case 'i':
				break;
			case '?':
				scr_printf("? <help>");
				break;
			}
			if (userflags & US_DISAPPEAR || e == 'i')
				scr_printf("\r%79s\r", "");
			else
				scr_printf("\n");
		}
DONE_QUOTING:	switch (e) {
		case '?':
			scr_printf("Options available here:\n"
				" ?  Help (prints this message)\n"
				" S  Stop reading immediately\n"
				" A  Again (repeats last message)\n"
				" N  Next (continue with next message)\n"
				" Y  My Next (continue with next message you authored)\n"
				" B  Back (go back to previous message)\n");
			if (  (is_room_aide)
			   || (room_flags & QR_MAILBOX)
			   || (room_flags2 & QR2_COLLABDEL)
			) {
				scr_printf(" D  Delete this message\n"
					" M  Move message to another room\n");
			}
			scr_printf(" C  Copy message to another room\n");
			if (!IsEmptyStr(printcmd))
				scr_printf(" P  Print this message\n");
			scr_printf(
				" Q  Reply to this message, quoting portions of it\n"
				" H  Headers (display message headers only)\n");
			if (is_mail)
				scr_printf(" R  Reply to this message\n");
			if (rc_allow_attachments) {
				scr_printf(" O  (Open attachments)\n");
				scr_printf(" F  (save attachments to a File)\n");
			}
			if (!IsEmptyStr(rc_url_cmd))
				scr_printf(" U  (list URL's for display)\n");
			if (!IsEmptyStr(imagecmd) && has_images > 0)
				scr_printf(" I  Image viewer\n");
			scr_printf("\n");
			goto RMSGREAD;
		case 'p':
			scr_flush();
			dest = fopen(prtfile, "w");
			arcflag = 1;
			hold_color = enable_color;
			enable_color = 0;
			goto RAGAIN;
		case 'q':
			scr_flush();
			dest = fopen(temp2, "w");
			quotflag = 1;
			hold_color = enable_color;
			enable_color = 0;
			goto RAGAIN;
		case 's':
			return;
		case 'a':
			goto RAGAIN;
		case 'b':
			a = a - (rdir * 2);
			break;
		case 'm':
		case 'c':
			newprompt("Enter target room: ",
				  targ, ROOMNAMELEN - 1);
			if (!IsEmptyStr(targ)) {
				r = CtdlIPCMoveMessage(ipc, (e == 'c' ? 1 : 0),
						       msg_arr[a], targ, cmd);
				scr_printf("%s\n", cmd);
				if (r / 100 == 2)
					msg_arr[a] = 0L;
			} else {
				goto RMSGREAD;
			}
			if (r / 100 != 2)	/* r will be init'ed, FIXME */
				goto RMSGREAD;	/* the logic here sucks */
			break;
		case 'o':
		case 'f':
			newprompt("Which section? ", filename, ((sizeof filename) - 1));
			r = CtdlIPCAttachmentDownload(ipc, msg_arr[a],
					filename, &attachment, progress, cmd);
			if (r / 100 != 2) {
				scr_printf("%s\n", cmd);
			} else {
				extract_token(filename, cmd, 2, '|', sizeof filename);
				/*
				 * Part 1 won't have a filename; use the
				 * subject of the message instead. IO
				 */
				if (IsEmptyStr(filename)) {
					strcpy(filename, reply_subject);
				}
				if (e == 'o') {		/* open attachment */
					mkdir(tempdir, 0700);
					snprintf(save_to, sizeof save_to, "%s/%04x.%s",
						tempdir,
						++att_seq,
						filename);
					save_buffer(attachment, extract_unsigned_long(cmd, 0), save_to);
					snprintf(cmd, sizeof cmd, rc_open_cmd, save_to);
					rv = system(cmd);
					if (rv != 0)
						scr_printf("failed to save %s Reason %d\n", cmd, rv);
				}
				else {			/* save attachment to disk */
					destination_directory(save_to, filename);
					save_buffer(attachment, extract_unsigned_long(cmd, 0), save_to);
				}
			}
			if (attachment) {
				free(attachment);
				attachment = NULL;
			}
			goto RMSGREAD;
		case 'd':
			scr_printf("*** Delete this message? ");
			if (yesno() == 1) {
				r = CtdlIPCDeleteMessage(ipc, msg_arr[a], cmd);
				scr_printf("%s\n", cmd);
				if (r / 100 == 2)
					msg_arr[a] = 0L;
			} else {
				goto RMSGREAD;
			}
			break;
		case 'h':
			read_message(ipc, msg_arr[a], READ_HEADER, NULL);
			goto RMSGREAD;
		case 'r':
			savedpos = num_msgs;
			entmsg(ipc, 1, ((userflags & US_EXTEDIT) ? 2 : 0), 0);
			num_msgs = savedpos;
			goto RMSGREAD;
		case 'u':
			list_urls(ipc);
			goto RMSGREAD;
		case 'i':
			image_view(ipc, msg_arr[a]);
			goto RMSGREAD;
	    case 'y':
          { /* hack hack hack */
            /* find the next message by me, stay here if we find nothing */
            int finda;
            int lasta = a;
            for (finda = (a + rdir); ((finda < num_msgs) && (finda >= 0)); finda += rdir)
              {
		/* This is repetitively dumb, but that's what computers are for.
		   We have to load up messages until we find one by us */
		char buf[SIZ];
		int founda = 0;
		struct ctdlipcmessage *msg = NULL;
                
		/* read the header so we can get 'from=' */
		r = CtdlIPCGetSingleMessage(ipc, msg_arr[finda], 1, 0, &msg, buf);
		if (!strncasecmp(msg->author, fullname, sizeof(fullname))) {
			a = lasta; /* meesa current */
			founda = 1;
		}

		free(msg);

		if (founda)
			break; /* for */
		lasta = finda; /* keep one behind or we skip on the reentrance to the for */
              } /* for */
          } /* case 'y' */
      } /* switch */
	}			/* end for loop */
}				/* end read routine */




/*
 * View and edit a system message
 */
void edit_system_message(CtdlIPC *ipc, char *which_message)
{
	char desc[SIZ];
	char read_cmd[SIZ];
	char write_cmd[SIZ];

	snprintf(desc, sizeof desc, "system message '%s'", which_message);
	snprintf(read_cmd, sizeof read_cmd, "MESG %s", which_message);
	snprintf(write_cmd, sizeof write_cmd, "EMSG %s", which_message);
	do_edit(ipc, desc, read_cmd, "NOOP", write_cmd);
}




/*
 * Loads the contents of a file into memory.  Caller must free the allocated
 * memory.
 */
char *load_message_from_file(FILE *src)
{
	size_t i;
	size_t got = 0;
	char *dest = NULL;

	fseek(src, 0, SEEK_END);
	i = ftell(src);
	rewind(src);

	dest = (char *)calloc(1, i + 1);
	if (!dest)
		return NULL;

	while (got < i) {
		size_t g;

		g = fread(dest + got, 1, i - got, src);
		got += g;
		if (g < i - got) {
			/* Interrupted system call, keep going */
			if (errno == EINTR)
				continue;
			/* At this point we have either EOF or error */
			i = got;
			break;
		}
		dest[i] = 0;
	}

	return dest;
}
