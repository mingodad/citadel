/*
 * serv_func.c
 *
 * Handles various types of data transfer operations with the Citadel service.
 *
 * $Id$
 */

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <limits.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include "webcit.h"
#include "webserver.h"

struct serv_info serv_info;

/*
 * get info about the server we've connected to
 */
void get_serv_info(char *browser_host, char *user_agent)
{
	char buf[SIZ];
	int a;

	/* Tell the server what kind of client is connecting */
	serv_printf("IDEN %d|%d|%d|%s|%s",
		DEVELOPER_ID,
		CLIENT_ID,
		CLIENT_VERSION,
		user_agent,
		browser_host
	);
	serv_gets(buf);

	/* Tell the server what kind of richtext we prefer */
	serv_puts("MSGP text/html|text/plain");
	serv_gets(buf);

	/* Now ask the server to tell us a little bit about itself... */
	serv_puts("INFO");
	serv_gets(buf);
	if (buf[0] != '1')
		return;

	a = 0;
	while (serv_gets(buf), strcmp(buf, "000")) {
		switch (a) {
		case 0:
			serv_info.serv_pid = atoi(buf);
			break;
		case 1:
			strcpy(serv_info.serv_nodename, buf);
			break;
		case 2:
			strcpy(serv_info.serv_humannode, buf);
			break;
		case 3:
			strcpy(serv_info.serv_fqdn, buf);
			break;
		case 4:
			strcpy(serv_info.serv_software, buf);
			break;
		case 5:
			serv_info.serv_rev_level = atoi(buf);
			break;
		case 6:
			strcpy(serv_info.serv_bbs_city, buf);
			break;
		case 7:
			strcpy(serv_info.serv_sysadm, buf);
			break;
		case 9:
			strcpy(serv_info.serv_moreprompt, buf);
			break;
		}
		++a;
	}
}



/* 
 * Function to spit out Citadel variformat text in HTML
 * If fp is non-null, it is considered to be the file handle to read the
 * text from.  Otherwise, text is read from the server.
 */
void fmout(FILE *fp, char *align)
{

	int intext = 0;
	int bq = 0;
	char buf[SIZ];

	wprintf("<DIV ALIGN=%s>\n", align);
	while (1) {
		if (fp == NULL)
			serv_gets(buf);
		if (fp != NULL) {
			if (fgets(buf, SIZ, fp) == NULL)
				strcpy(buf, "000");
			buf[strlen(buf) - 1] = 0;
		}
		if (!strcmp(buf, "000")) {
			if (bq == 1)
				wprintf("</I>");
			wprintf("</DIV><BR>\n");
			return;
		}
		if ((intext == 1) && (isspace(buf[0]))) {
			wprintf("<BR>");
		}
		intext = 1;

		/* Quoted text should be displayed in italics and in a
		 * different colour.  This code understands both Citadel/UX
		 * style " >" quotes and FordBoard-style " :-)" quotes.
		 */
		if ((bq == 0) &&
		    ((!strncmp(buf, " >", 2)) || (!strncmp(buf, " :-)", 4)))) {
			wprintf("<SPAN CLASS=\"pull_quote\">");
			bq = 1;
		} else if ((bq == 1) &&
		  (strncmp(buf, " >", 2)) && (strncmp(buf, " :-)", 4))) {
			wprintf("</SPAN>");
			bq = 0;
		}
		/* Activate embedded URL's */
		url(buf);

		escputs(buf);
		wprintf("\n");
	}
}




/*
 * Transmit message text (in memory) to the server.
 * If convert_to_html is set to 1, the message is converted into something
 * which kind of resembles HTML.
 */
void text_to_server(char *ptr, int convert_to_html)
{
	char buf[SIZ];
	int ch, a, pos;

	if (convert_to_html) {
		serv_puts("<HTML><BODY>");
	}

	pos = 0;
	strcpy(buf, "");

	while (ptr[pos] != 0) {
		ch = ptr[pos++];
		if (ch == 10) {
			while ( (isspace(buf[strlen(buf) - 1]))
			  && (strlen(buf) > 1) )
				buf[strlen(buf) - 1] = 0;
			serv_puts(buf);
			strcpy(buf, "");
			if (convert_to_html) {
				strcat(buf, "<BR>");
			}
			else {
				if (ptr[pos] != 0) strcat(buf, " ");
			}
		} else {
			a = strlen(buf);
			buf[a + 1] = 0;
			buf[a] = ch;
			if ((ch == 32) && (strlen(buf) > 200)) {
				buf[a] = 0;
				serv_puts(buf);
				strcpy(buf, "");
			}
			if (strlen(buf) > 250) {
				serv_puts(buf);
				strcpy(buf, "");
			}
		}
	}
	serv_puts(buf);

	if (convert_to_html) {
		serv_puts("</BODY></HTML>\n");
	}

}



/*
 * translate server message output to text
 * (used for editing room info files and such)
 */
void server_to_text()
{
	char buf[SIZ];

	int count = 0;

	while (serv_gets(buf), strcmp(buf, "000")) {
		if ((buf[0] == 32) && (count > 0)) {
			wprintf("\n");
		}
		wprintf("%s", buf);
		++count;
	}
}



/*
 * Read binary data from server into memory using a series of
 * server READ commands.
 */
void read_server_binary(char *buffer, size_t total_len) {
	char buf[SIZ];
	size_t bytes = 0;
	size_t thisblock = 0;

	memset(buffer, 0, total_len);
	while (bytes < total_len) {
		thisblock = 4095;
		if ((total_len - bytes) < thisblock) {
			thisblock = total_len - bytes;
			if (thisblock == 0) return;
		}
		serv_printf("READ %d|%d", (int)bytes, (int)thisblock);
		serv_gets(buf);
		if (buf[0] == '6') {
			thisblock = (size_t)atoi(&buf[4]);
			if (!WC->connected) return;
			serv_read(&buffer[bytes], thisblock);
			bytes += thisblock;
		}
		else {
			lprintf(3, "Error: %s\n", &buf[4]);
			return;
		}
	}
}


/*
 * Read text from server, appending to a string buffer until the
 * usual 000 terminator is found.  Caller is responsible for freeing
 * the returned pointer.
 */
char *read_server_text(void) {
	char *text = NULL;
	size_t bytes_allocated = 0;
	size_t bytes_read = 0;
	int linelen;
	char buf[SIZ];

	text = malloc(SIZ);
	if (text == NULL) {
		return(NULL);
	}
	strcpy(text, "");
	bytes_allocated = SIZ;

	while (serv_gets(buf), strcmp(buf, "000")) {
		linelen = strlen(buf);
		buf[linelen] = '\n';
		buf[linelen+1] = 0;
		++linelen;

		if ((bytes_read + linelen) >= (bytes_allocated - 2)) {
			bytes_allocated = 2 * bytes_allocated;
			text = realloc(text, bytes_allocated);
		}

		strcpy(&text[bytes_read], buf);
		bytes_read += linelen;
	}

	return(text);
}
