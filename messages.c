#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "webcit.h"
#include "child.h"

char reply_to[512];
long msgarr[1024];

/*
 * Look for URL's embedded in a buffer and make them linkable.  We use a
 * target window in order to keep the BBS session in its own window.
 */
void url(buf)
char buf[]; {

	int pos;
	int start,end;
	char ench;
	char urlbuf[256];
	char outbuf[256];

	start = (-1);
	end = strlen(buf);
	ench = 0;

	for (pos=0; pos<strlen(buf); ++pos) {
		if (!strncasecmp(&buf[pos],"http://",7)) start = pos;
		if (!strncasecmp(&buf[pos],"ftp://",6)) start = pos;
		}

	if (start<0) return;

	if ((start>0)&&(buf[start-1]=='<')) ench = '>';
	if ((start>0)&&(buf[start-1]=='[')) ench = ']';
	if ((start>0)&&(buf[start-1]=='(')) ench = ')';
	if ((start>0)&&(buf[start-1]=='{')) ench = '}';

	for (pos=strlen(buf); pos>start; --pos) {
		if ((buf[pos]==' ')||(buf[pos]==ench)) end = pos;
		}

	strncpy(urlbuf,&buf[start],end-start);
	urlbuf[end-start] = 0;


	strncpy(outbuf,buf,start);
	sprintf(&outbuf[start],"%cA HREF=%c%s%c TARGET=%c%s%c%c%s%c/A%c", 
		LB,QU,urlbuf,QU,QU,TARGET,QU,RB,urlbuf,LB,RB);
	strcat(outbuf,&buf[end]);
	strcpy(buf,outbuf);
	}


void read_message(msgnum, oper)
long msgnum;
char *oper; {
	char buf[256];
	char m_subject[256];
	char from[256];
	long now;
	struct tm *tm;
	int format_type = 0;
	int nhdr = 0;
	int bq = 0;

	sprintf(buf,"MSG0 %ld",msgnum);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0]!='1') {
		wprintf("<STRONG>ERROR:</STRONG> %s<BR>\n",&buf[4]);
		return;
		}

	wprintf("<TABLE WIDTH=100% BORDER=0 CELLSPACING=0 CELLPADDING=0 BGCOLOR=000077><TR><TD>\n");
	wprintf("<FONT COLOR=\"FFFF00\"><B> ");
	strcpy(m_subject,"");

	strcpy(reply_to,"nobody...xxxxx");
	while(serv_gets(buf), strncasecmp(buf,"text",4)) {
		if (!strncasecmp(buf,"nhdr=yes",8)) nhdr=1;
		if (nhdr==1) buf[0]='_';
		if (!strncasecmp(buf,"type=",5))
			format_type=atoi(&buf[5]);
		if (!strncasecmp(buf,"from=",5)) {
			wprintf("from %s ",&buf[5]);
			strcpy(from,&buf[5]);
			}
		if (!strncasecmp(buf,"path=",5))
			strcpy(reply_to,&buf[5]);
		if (!strncasecmp(buf,"subj=",5))
			strcpy(m_subject,&buf[5]);
		if ((!strncasecmp(buf,"hnod=",5)) 
		   && (strcasecmp(&buf[5],serv_info.serv_humannode)))
			wprintf("(%s) ",&buf[5]);
		if ((!strncasecmp(buf,"room=",5))
		   && (strcasecmp(&buf[5],wc_roomname)))
			wprintf("in %s> ",&buf[5]);

		if (!strncasecmp(buf,"node=",5)) {
			if ( (room_flags&QR_NETWORK)
			   || ((strcasecmp(&buf[5],serv_info.serv_nodename)
   			   &&(strcasecmp(&buf[5],serv_info.serv_fqdn)))))
				{
				wprintf("@%s ",&buf[5]);
				}
			if ((!strcasecmp(&buf[5],serv_info.serv_nodename))
   			   ||(!strcasecmp(&buf[5],serv_info.serv_fqdn)))
				{
				strcpy(reply_to,from);
				}
			else if (haschar(&buf[5],'.')==0) {
				sprintf(reply_to,"%s @ %s",from,&buf[5]);
				}
			}

		if (!strncasecmp(buf,"rcpt=",5))
			wprintf("to %s ",&buf[5]);
		if (!strncasecmp(buf,"time=",5)) {
			now=atol(&buf[5]);
			tm=(struct tm *)localtime(&now);
			strcpy(buf,(char *)asctime(tm)); buf[strlen(buf)-1]=0;
			strcpy(&buf[16],&buf[19]);
			wprintf("%s ",&buf[4]);
			}
		}

	if (nhdr==1) wprintf("****");
	wprintf("</B></FONT></TD>");
	
	if (is_room_aide) {
		wprintf("<TD ALIGN=RIGHT NOWRAP><FONT COLOR=\"FFFF00\"><B>");

		wprintf("<A HREF=\"/confirm_move_msg");
		wprintf("&msgid=%ld", msgnum);
		wprintf("&referer=%s\">Move</A>", oper);

		wprintf("&nbsp;&nbsp;");

		wprintf("<A HREF=\"/confirm_delete_msg");
		wprintf("&msgid=%ld", msgnum);
		wprintf("&referer=%s\">Del</A>", oper);

		wprintf("</B></FONT></TD>");
		}
	
	wprintf("</TR></TABLE>\n");

	if (strlen(m_subject)>0) {
		wprintf("Subject: %s<BR>\n",m_subject);
		}

	if (format_type == 0) {
		fmout(NULL);
		}
	else {
		while(serv_gets(buf), strcmp(buf,"000")) {
			while ((strlen(buf)>0)&&(isspace(buf[strlen(buf)-1])))
				buf[strlen(buf)-1] = 0;
			if ((bq==0)&&
((!strncmp(buf,">",1))||(!strncmp(buf," >",2))||(!strncmp(buf," :-)",4)))) {
				wprintf("<FONT COLOR=\"000044\"><I>");
				bq = 1;
				}
			else if ((bq==1)&&
(strncmp(buf,">",1))&&(strncmp(buf," >",2))&&(strncmp(buf," :-)",4))) {
				wprintf("</FONT></I>");
				bq = 0;
				}
			wprintf("<TT>");
			url(buf);
			escputs(buf);
			wprintf("</TT><BR>\n");
			}
		}
		wprintf("<BR>");
	}



/* 
 * load message pointers from the server
 */
int load_msg_ptrs(servcmd)
char *servcmd; {
	char buf[256];
	int nummsgs;

	nummsgs = 0;
	serv_puts(servcmd);
	serv_gets(buf);
	if (buf[0]!='1') {
		wprintf("<EM>%s</EM><BR>\n",&buf[4]);
		return(nummsgs);
		}
	while (serv_gets(buf), strcmp(buf,"000")) {
		msgarr[nummsgs] = atol(buf);
		++nummsgs;
		}
	return(nummsgs);
	}


/*
 * command loop for reading messages
 */
void readloop(char *oper) {
	char cmd[256];
	int a;
	int nummsgs;

	printf("HTTP/1.0 200 OK\n");
	output_headers();
        wprintf("<HTML><HEAD><TITLE>Messages</TITLE>\n");
        wprintf("</HEAD><BODY BACKGROUND=\"/image&name=background\" TEXT=\"#000000\" LINK=\"#004400\">\n");

	wprintf("<CENTER><B>%s - ",wc_roomname);
	if (!strcmp(oper,"readnew")) {
		strcpy(cmd,"MSGS NEW");
		wprintf("new messages");
		}
	else if (!strcmp(oper,"readold")) {
		strcpy(cmd,"MSGS OLD");
		wprintf("old messages");
		}
	else {
		strcpy(cmd,"MSGS ALL");
		wprintf("all messages");
		}
	wprintf("</B></CENTER><BR>\n");

	nummsgs = load_msg_ptrs(cmd);
	if (nummsgs == 0) {
		if (!strcmp(oper,"readnew")) {
			wprintf("<EM>No new messages in this room.</EM>\n");
			}
		else if (!strcmp(oper,"readold")) {
			wprintf("<EM>No old messages in this room.</EM>\n");
			}
		else {
			wprintf("<EM>This room is empty.</EM>\n");
			}
		goto DONE;
		}

	for (a=0; a<nummsgs; ++a) {
		read_message(msgarr[a], oper);
		}

DONE:	wprintf("</BODY></HTML>\n");
	wDumpContent();
	}


