/**
 ** libCxClient - Citadel/UX Extensible Client API
 ** Copyright (c) 2000, Flaming Sword Productions
 ** Copyright (c) 2001, The Citadel/UX Consortium
 ** All Rights Reserved
 **
 ** Module: messages.o
 ** Date: 2000-10-15
 ** Last Revision: 2000-10-15
 ** Description: Functions which manipulate (build) message lists.
 ** CVS: $Id$
 **/
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<stdarg.h>
#include	<CxClient.h>
#include	"autoconf.h"

/**
 ** CxMsInfo(): Retrieve message information for all of the message id's listed inside
 ** of a Message List.
 **/
CXLIST		CxMsInfo(int id, CXLIST msg_list) {
CXLIST		mp, messages = NULL;
char		buf[255], *from, *date, *subject;
int		rc;

	DPF((DFA,"Retreiving information for all messages in 0x%08x",msg_list));

	mp = msg_list;
	while ( mp ) {
		sprintf(buf,"MSG0 %s|1",mp->data);
		CxClSend(id, buf);
		rc = CxClRecv(id, buf);
		if( CHECKRC(rc, RC_LISTING)) {
			from = date = subject = 0;
			do {

				rc = CxClRecv(id, buf);
				if(rc && strstr(buf,"from=")) {
					DPF((DFA, "from: %s",buf));

					from = (char *)CxMalloc(strlen(buf+5)+1);
					strcpy(from, buf+5);

				} else if(rc && strstr(buf,"time=")) {

					DPF((DFA, "time: %s",buf));
					date = (char *)CxMalloc(strlen(buf+5)+1);
					strcpy(date, buf+5);
				}

			} while(rc<0);

			if((from && *from) && (date && *date)) {
				sprintf(buf,"%s|%s|%s",from,date,subject);
				DPF((DFA, "insert-> %s",buf));

				messages = (CXLIST) CxLlInsert(messages,buf);
				DPF((DFA, "Freeing memory (temp vars)"));
				if(subject) CxFree(subject);
				if(from) CxFree(from);
				if(date) CxFree(date);
			}
		}

		mp = mp->next;
	}

	return(messages);
}

/**
 ** CxMsList(): Retrieve a list of messages in the current room.
 **/
CXLIST		CxMsList(int id, int list_type, int number_messages) {
int		rc;
char		buf[255], *malleable;
CXLIST		msgs = NULL;

	DPF((DFA,"Retrieving list of messages from the server."));

	switch( list_type ) {
		case(2):
			// MSGS LAST|%d
			break;

		case(1):
			CxClSend( id, "MSGS NEW");
			break;

		default:
			CxClSend( id, "MSGS");
			break;
	}
	rc = CxClRecv( id, buf );

	if( CHECKRC(rc, RC_LISTING) ) {

		do {
			rc = CxClRecv(id, buf);

			if(rc) {
				malleable = (char *)CxMalloc(strlen(buf) + 1);
				strcpy(malleable,buf);
				DPF((DFA,"%s",malleable));

				msgs = (CXLIST) CxLlInsert(msgs,malleable);
				CxFree(malleable);
			}
		} while(rc < 0);

		return(msgs);
	} else {
		return(NULL);
	}
}

/**
 ** CxMsLoad(): Retrieve a message from the server.  Expects a MESSAGE_ID, 
 ** returns 0 on success, [err] on failure.
 **
 ** Argument:
 **	preserve_newlines: Preserve newline delimiters in body text?
 **
 ** CLIENT MUST free(toret.body) MANUALLY!!!!
 **/
int		CxMsLoad(int id, unsigned long int mid, int preserve_newlines, MESGINFO *toret) {
char		buf[255], *newline="\n";
int		rc, message_contents = 0, line_width;

	DPF((DFA,"Loading message \"%ld\"",mid));
	toret->message_id = 0;
	toret->author[0] = 0;
	toret->rcpt[0] = 0;
	toret->room[0] = 0;
	toret->subject[0] = 0;

	sprintf(buf,"MSG2 %ld",mid);
	CxClSend(id, buf);
	rc = CxClRecv(id, buf);
	if(CHECKRC(rc, RC_LISTING) ) {
		DPF((DFA,"RC_LISTING"));
		do {
			rc = CxClRecv(id, buf);
			if( rc ) {
				if(buf[strlen(buf)-1]=='\r') 
					buf[strlen(buf)-1]=0;

				DPF((DFA,"[%d] buf: %s", rc, buf));

				if(strstr(buf,"From:") && 
					!message_contents) {
					strcpy(toret->author, buf+5);

				} else if((strstr(buf,"To:") == (char *)&buf) && 
					!message_contents) {
					strcpy(toret->rcpt, buf+3);

				} else if(strstr(buf,"X-UIDL:")
					&& !message_contents) {
					DPF((DFA,"Message ID: %s",buf+7));
					toret->message_id = atoi(buf+7);

				} else if(strstr(buf,"X-Citadel-Room:")
					&& !message_contents) {
					DPF((DFA,"Room: %s",buf+15));
					strcpy(toret->room, buf+15);
					toret->room[
						strlen(toret->room)-1
					] = 0;

				} else if(strstr(buf,"Subject:") 
					&& !message_contents) {
					strcpy(toret->subject, buf+8);

				} else if(strstr(buf,"Path:") 
					&& !message_contents) {
					strcpy(toret->path, buf+5);

				} else if(strstr(buf,"Node:") 
					&& !message_contents) {
					strcpy(toret->node, buf+5);

				} else if((strstr(buf,"Date:") == (char *)&buf) 
					&& !message_contents) {
					strcpy(toret->date, buf+5);

				} else if((buf[0] == 0) || (buf[0] == '\r') || 
					message_contents) {
					message_contents = 1;

					/**
					 ** ugliness.  Load entire message.  Ick.
					 **/

					do {
						rc = CxClRecv(id, buf);
						if(rc) {
							DPF((DFA,"%s",buf));

							line_width = strlen(buf);

							/**
							 ** Start by stripping out the CR.
							 **/
							*(strchr(buf,'\r')) = 0;
							if(preserve_newlines)
								line_width+=strlen(newline);
							line_width++; /** Count NULL. **/

							if(!toret->body) {
								toret->body = (char *)CxMalloc(
									line_width
								);

								strcpy(toret->body, buf);

							} else {
								toret->body = (char *) realloc(
									toret->body, 
									strlen(toret->body)
									+ line_width 
								);

								strcat(toret->body, buf);
							}

							/**
							 ** If we are to preserve the newlines
							 ** present in the message, then append
							 ** a newline to the end of each line.
							 **/
							if(preserve_newlines)
								strcat(toret->body, newline);
						}
					} while(rc<0);

				}

			}
		} while(rc<0);
		DPF((DFA,"RC_LISTING completed."));
	}

	DPF((DFA,"[Return Data]"));
	DPF((DFA,"toret->message_id: %d\n",toret->message_id));
	DPF((DFA,"toret->author: %s\n",toret->author));
	DPF((DFA,"toret->room: %s\n",toret->room));
	DPF((DFA,"..."));

	/**
	 ** If this message has been loaded, we succeeded.
	 **/
	if(toret->message_id) {
		DPF((DFA,"Returning [SUCCESS]"));
		return(0);

	/**
	 ** Otherwise, we failed.
	 **/
	} else {
		DPF((DFA,"Returning [ENOMSG]"));
		return(1);
	}
}

/**
 ** CxMsSaveOk(): Verify that users can post to this room.  Returns 1 if posting is
 ** allowed, 0 if posting is not allowed.
 **/
int		CxMsSaveOk(int id, const char *username) {
int		rc;
char		buf[255];

	DPF((DFA,"Checking room for post permissions..."));
	sprintf(buf,"ENT0 0|%s|0|0",username);
	CxClSend(id, buf);
	rc = CxClRecv(id, buf);
	if(CHECKRC(rc, RC_OK) ) {
		DPF((DFA,"Ok for posting"));
		return(1);

	} else {
		DPF((DFA,"Not Ok for posting [%d]",rc));
		return(0);
	}

	return(999);
}

/**
 ** CxMsSave(): Save (post/send) a message to the server.  Expects a fully quantified
 ** MESGINFO struct.  Returns 0 on success, [err] on failure.
 ** [err]:
 **  1: No room specified
 **  2: Posting not allowed in this room.
 **  3: Message rejected for unknown reasons.
 **  ... tba
 **/
int		CxMsSave(int id, MESGINFO msg) {
int		rc;
char		buf[255];

	DPF((DFA,"Preparing to save message to server..."));

	if(!msg.room) {
		DPF((DFA,"Returning [ENOROOM]"));
		return(1);
	}

	DPF((DFA,"Checking for access..."));
	sprintf(buf,"ENT0 0|%s|0|0",msg.rcpt);
	CxClSend(id, buf);
	rc = CxClRecv(id, buf);
	DPF((DFA,"Server said [%s]",buf));

	if( CHECKRC(rc, RC_OK)) {
		DPF((DFA,"Permission to save"));

		sprintf(buf,"ENT0 1|%s|0|4|",msg.rcpt);
		CxClSend(id, buf);

		rc = CxClRecv(id, buf);
		if( CHECKRC(rc, RC_SENDLIST)) {
			DPF((DFA,"Sending message to server..."));
			sprintf(buf, "From: %s", msg.author);
			CxClSend(id, buf);
			sprintf(buf, "To: %s", msg.rcpt);
			CxClSend(id, buf);
			sprintf(buf, "X-Mailer: libCxClient %s", CXREVISION);
			CxClSend(id, buf);
			sprintf(buf, "Subject: %s", msg.subject);
			CxClSend(id, buf);
			CxClSend(id, "");
			CxClSend(id, msg.body);

			CxClSend(id, "000");
			DPF((DFA,"Done!"));

			DPF((DFA,"Server accepted message"));
			return(0);
		}

	} else {
		DPF((DFA,"No permission to save!"));
		return(2);
	}

	return(999);
}

/**
 ** CxMsMark(): Mark message(s) as read.
 **/
void		CxMsMark( int id, long unsigned int msgid ) {
char		buf[1024];
int		rc;

	DPF((DFA, "Marking message %s read.", msgid));

	if( msgid == MSGS_ALL ) {
		sprintf( buf, "SLRP highest" );

	} else {
		sprintf( buf, "SLRP %ld", msgid );
	}

	CxClSend( id, buf );
	rc = CxClRecv( id, buf );

	if( rc == RC_OK ) {
		DPF((DFA, "Done."));

	} else {
		DPF((DFA, "Failed."));
	}
}
