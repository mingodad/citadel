/* $Id$ */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include "webcit.h"
#include "child.h"

struct serv_info serv_info;

/*
 * get info about the server we've connected to
 */
void get_serv_info(char *browser_host) {
	char buf[256];
	int a;

	serv_printf("IDEN %d|%d|%d|%s|%s",
		DEVELOPER_ID,
		CLIENT_ID,
		CLIENT_VERSION,
		SERVER,
		browser_host
		);
	serv_gets(buf);

	serv_puts("INFO");
	serv_gets(buf);
	if (buf[0]!='1') return;

	a = 0;
	while(serv_gets(buf), strcmp(buf,"000")) {
	    switch(a) {
		case 0:		serv_info.serv_pid = atoi(buf);
				break;
		case 1:		strcpy(serv_info.serv_nodename,buf);
				break;
		case 2:		strcpy(serv_info.serv_humannode,buf);
				break;
		case 3:		strcpy(serv_info.serv_fqdn,buf);
				break;
		case 4:		strcpy(serv_info.serv_software,buf);
				break;
		case 5:		serv_info.serv_rev_level = atoi(buf);
				break;
		case 6:		strcpy(serv_info.serv_bbs_city,buf);
				break;
		case 7:		strcpy(serv_info.serv_sysadm,buf);
				break;
		case 9:		strcpy(serv_info.serv_moreprompt,buf);
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
void fmout(FILE *fp)
{

	int intext = 0;
	int bq = 0;
	char buf[256];

	while(1) {
		if (fp==NULL) serv_gets(buf);
		if (fp!=NULL) {
			if (fgets(buf,256,fp)==NULL) strcpy(buf,"000");
			buf[strlen(buf)-1] = 0;
			}
		if (!strcmp(buf,"000")) {
			if (bq==1) wprintf("</I>");
			wprintf("<P>\n");
			return;
			}
		if ( (intext==1) && (isspace(buf[0])) ) {
			wprintf("<BR>");
			}
		intext = 1;

		/* Quoted text should be displayed in italics and in a
		 * different colour.  This code understands both Citadel/UX
		 * style " >" quotes and FordBoard-style " :-)" quotes.
		 */
		if ((bq==0)&&
		   ((!strncmp(buf," >",2))||(!strncmp(buf," :-)",4)))) {
			wprintf("<FONT COLOR=\"000044\"><I>");
			bq = 1;
			}
		else if ((bq==1)&&
		     (strncmp(buf," >",2))&&(strncmp(buf," :-)",4))) {
			wprintf("</FONT></I>");
			bq = 0;
			}

		/* Activate embedded URL's */
		url(buf);

		escputs(buf);
		wprintf("\n");
		}
	}






/*
 * transmit message text (in memory) to the server
 */
void text_to_server(char *ptr) {
	char buf[256];
	int ch,a,pos;

	pos = 0;
	
	strcpy(buf,"");
	while (ptr[pos]!=0) {
		ch = ptr[pos++];
		if (ch==10) {
			while (isspace(buf[strlen(buf)-1]))
				buf[strlen(buf)-1]=0;
			serv_puts(buf);
			strcpy(buf,"");
			}
		else {
			a = strlen(buf);
			buf[a+1] = 0;
			buf[a] = ch;
			if ((ch==32)&&(strlen(buf)>200)) {
				buf[a]=0;
				serv_puts(buf);
				strcpy(buf,"");
				}
			if (strlen(buf)>250) {
				serv_puts(buf);
				strcpy(buf,"");
				}
			}
		}
	serv_puts(buf);
	}






/*
 * translate server message output to text
 * (used for editing room info files and such)
 */
void server_to_text() {
	char buf[256]; 

	int count = 0;

	while (serv_gets(buf), strcmp(buf, "000") ) {
		if ( (buf[0] == 32) && (count > 0) ) {
			wprintf("\n");
			}
		wprintf("%s", buf);
		++count;
		}
	}




