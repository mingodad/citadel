#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <string.h>
#include <pthread.h>
#include "citadel.h"
#include "server.h"
#include <syslog.h>
#ifdef NEED_SELECT_H
#include <sys/select.h>
#endif
#include "proto.h"

extern struct config config;
extern struct CitContext *ContextList;

struct ChatLine *ChatQueue = NULL;
int ChatLastMsg = 0;


void allwrite(char *cmdbuf, int flag, char *roomname, char *username)
{	
	FILE *fp;
	char bcast[256];
	char *un;
	struct ChatLine *clptr, *clnew;
	time_t now;

	if (CC->fake_username[0])
	   un = CC->fake_username;
	else
	   un = CC->usersupp.fullname;
	if (flag == 1) 
	{
		sprintf(bcast,":|<%s %s>",un,cmdbuf);
	}
	else
	if (flag == 0)
	{
		sprintf(bcast,"%s|%s",un,cmdbuf);
	}
	else
	if (flag == 2)
	{
		sprintf(bcast,":|<%s whispers %s>", un, cmdbuf);
	}
	if (strucmp(cmdbuf,"NOOP")) {
		fp = fopen(CHATLOG,"a");
		fprintf(fp,"%s\n",bcast);
		fclose(fp);
		}

	clnew = (struct ChatLine *) malloc(sizeof(struct ChatLine));
	bzero(clnew, sizeof(struct ChatLine));
	if (clnew == NULL) {
		fprintf(stderr, "citserver: cannot alloc chat line: %s\n",
			strerror(errno));
		return;
		}

	time(&now);
	clnew->next = NULL;
	clnew->chat_time = now;
	strncpy(clnew->chat_room, roomname, 19);
	if (username)
  	   strncpy(clnew->chat_username, username, 31); 
  	else
  	   clnew->chat_username[0] = '\0';
	strcpy(clnew->chat_text, bcast);

	/* Here's the critical section.
	 * First, add the new message to the queue...
	 */
	begin_critical_section(S_CHATQUEUE);
	++ChatLastMsg;
	clnew->chat_seq = ChatLastMsg;
	if (ChatQueue == NULL) {
		ChatQueue = clnew;
		}
	else {
		for (clptr=ChatQueue; clptr->next != NULL; clptr=clptr->next) ;;
		clptr->next = clnew;
		}

	/* Then, before releasing the lock, free the expired messages */
	while(1) {
		if (ChatQueue == NULL) goto DONE_FREEING;
		if ( (now - ChatQueue->chat_time) < 120L ) goto DONE_FREEING;
		clptr = ChatQueue;
		ChatQueue = ChatQueue->next;
		free(clptr);
		}
DONE_FREEING:	end_critical_section(S_CHATQUEUE);
	}

/*
 * List users in chat.  Setting allflag to 1 also lists users elsewhere.
 */
void do_chat_listing(int allflag)
{
	struct CitContext *ccptr;

	cprintf(":|\n:| Users currently in chat:\n");
	for (ccptr = ContextList; ccptr != NULL; ccptr = ccptr->next) {
		if ( (!strucmp(ccptr->cs_room, "<chat>"))
		   && ((ccptr->cs_flags & CS_STEALTH) == 0)) {
			cprintf(":| %-25s <%s>\n", (ccptr->fake_username[0]) ? ccptr->fake_username : ccptr->curr_user, ccptr->chat_room);
			}
		}

	if (allflag == 1) 
	{
		cprintf(":|\n:| Users not in chat:\n");
		for (ccptr = ContextList; ccptr != NULL; ccptr = ccptr->next) 
		{
			if ( (strucmp(ccptr->cs_room, "<chat>"))
			   && ((ccptr->cs_flags & CS_STEALTH) == 0)) 
			{
				cprintf(":| %-25s <%s>:\n", (ccptr->fake_username[0]) ? ccptr->fake_username : ccptr->curr_user, ccptr->cs_room);
			}
		}
	}
	
	cprintf(":|\n");
	}


void cmd_chat(char *argbuf)
{
	char cmdbuf[256];
	char *un;
	char *strptr1;
	char hold_cs_room[20];
	int MyLastMsg, ThisLastMsg;
	struct ChatLine *clptr;
	int retval;

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	strcpy(CC->chat_room, "Main room");

	strcpy(hold_cs_room,CC->cs_room);
	CC->cs_flags = CC->cs_flags | CS_CHAT;
	set_wtmpsupp("<chat>");
	cprintf("%d Entering chat mode (type '/help' for available commands)\n",
		START_CHAT_MODE);

	MyLastMsg = ChatLastMsg;

	if ((CC->cs_flags & CS_STEALTH) == 0) {
		allwrite("<entering chat>",0, CC->chat_room, NULL);
		}

	strcpy(cmdbuf, "");

	while(1) {
		cmdbuf[strlen(cmdbuf) + 1] = 0;
		retval = client_read_to(&cmdbuf[strlen(cmdbuf)], 1, 2);

		/* if we have a complete line, do send processing */
		if (strlen(cmdbuf) > 0) if (cmdbuf[strlen(cmdbuf)-1] == 10) {
			cmdbuf[strlen(cmdbuf) - 1] = 0;
			time(&CC->lastcmd);
			time(&CC->lastidle);

			if ( (!strucmp(cmdbuf,"exit"))
		     	||(!strucmp(cmdbuf,"/exit"))
		     	||(!strucmp(cmdbuf,"quit"))
		     	||(!strucmp(cmdbuf,"logout"))
		     	||(!strucmp(cmdbuf,"logoff"))
		     	||(!strucmp(cmdbuf,"/q"))
		     	||(!strucmp(cmdbuf,".q"))
		     	||(!strucmp(cmdbuf,"/quit"))
				) strcpy(cmdbuf,"000");
	
			if (!strcmp(cmdbuf,"000")) {
				if ((CC->cs_flags & CS_STEALTH) == 0) {
					allwrite("<exiting chat>",0, CC->chat_room, NULL);
					}
				sleep(1);
				cprintf("000\n");
				CC->cs_flags = CC->cs_flags - CS_CHAT;
				set_wtmpsupp(hold_cs_room);
				return;
				}
	
			if ((!strucmp(cmdbuf,"/help"))
		   	||(!strucmp(cmdbuf,"help"))
		   	||(!strucmp(cmdbuf,"/?"))
		   	||(!strucmp(cmdbuf,"?"))) {
				cprintf(":|\n");
				cprintf(":|Available commands: \n");
				cprintf(":|/help   (prints this message) \n");
				cprintf(":|/who    (list users currently in chat) \n");
				cprintf(":|/whobbs (list users in chat -and- elsewhere) \n");
				cprintf(":|/me     ('action' line, ala irc) \n");
				cprintf(":|/msg    (send private message, ala irc) \n");
				cprintf(":|/join   (join new room) \n"); 
				cprintf(":|/quit   (return to the BBS) \n");
				cprintf(":|\n");
				}
			if (!strucmp(cmdbuf,"/who")) {
				do_chat_listing(0);
				}
			if (!strucmp(cmdbuf,"/whobbs")) {
				do_chat_listing(1);
				}
			if (!struncmp(cmdbuf,"/me ",4)) {
				allwrite(&cmdbuf[4],1, CC->chat_room, NULL);
				}
				
			if (!struncmp(cmdbuf,"/msg ", 5))
			{
			   strptr1 = strtok(cmdbuf, " ");
			   if (strptr1)
			   {
 			      strptr1 = strtok(NULL, " ");
 			      if (strptr1)
 			      {
			         allwrite(&strptr1[strlen(strptr1)+1], 2, CC->chat_room, CC->curr_user);
			         if (struncmp(CC->curr_user, strptr1, strlen(CC->curr_user)))
  			            allwrite(&strptr1[strlen(strptr1)+1], 2, CC->chat_room, strptr1);
			      }
 			   }
 			   cprintf("\n");
 			}

			if (!struncmp(cmdbuf,"/join ", 6))
			{
	          	   allwrite("<changing rooms>",0, CC->chat_room, NULL);
			   if (!cmdbuf[6])
			      strcpy(CC->chat_room, "Main room");
			   else
			   {
   			      strncpy(CC->chat_room, &cmdbuf[6], 20);
			   }
		           allwrite("<joining room>",0, CC->chat_room, NULL);
		           cprintf("\n");
		           
			}
			if ((cmdbuf[0]!='/')&&(strlen(cmdbuf)>0)) {
				allwrite(cmdbuf,0, CC->chat_room, NULL);
				}

			strcpy(cmdbuf, "");

			}
	
		/* now check the queue for new incoming stuff */
		
		if (CC->fake_username[0])
		   un = CC->fake_username;
		else
		   un = CC->curr_user;
		if (ChatLastMsg > MyLastMsg) {
			ThisLastMsg = ChatLastMsg;
			for (clptr=ChatQueue; clptr!=NULL; clptr=clptr->next) 
			{
				if (
				(clptr->chat_seq > MyLastMsg) && 
				(!struncmp(CC->chat_room, clptr->chat_room, 20)) &&
				((!clptr->chat_username[0]) || (!struncmp(un, clptr->chat_username, 32)))
				)
				{
					cprintf("%s\n", clptr->chat_text);
				}
			}
			MyLastMsg = ThisLastMsg;
			}

		}
	}


/*
 * poll for express messages
 */
void cmd_pexp(void) {
	struct ExpressMessage *emptr;

	if (CC->FirstExpressMessage == NULL) {
		cprintf("%d No express messages waiting.\n",ERROR);
		return;
		}

	cprintf("%d Express msgs:\n",LISTING_FOLLOWS);

	while (CC->FirstExpressMessage != NULL) {
		cprintf("%s", CC->FirstExpressMessage->em_text);
		begin_critical_section(S_SESSION_TABLE);
		emptr = CC->FirstExpressMessage;
		CC->FirstExpressMessage = CC->FirstExpressMessage->next;
		free(emptr);
		end_critical_section(S_SESSION_TABLE);
		}
	cprintf("000\n");
	}

/*
 * returns an asterisk if there are any express messages waiting,
 * space otherwise.
 */
char check_express(void) {
	if (CC->FirstExpressMessage == NULL) {
		return(' ');
		}
	else {
		return('*');
		}
	}


/*
 * send express messages  <bc>
 */
void cmd_sexp(char *argbuf)
{
	char x_user[256];
	char x_msg[256];
	int message_sent = 0;
	struct CitContext *ccptr;
	struct ExpressMessage *emptr, *emnew;
	char *lun;		/* <bc> */

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if (num_parms(argbuf)!=2) {
		cprintf("%d usage error\n",ERROR);
		return;	
		}

   	if (CC->fake_username[0])
   	   lun = CC->fake_username;
   	else
   	   lun = CC->usersupp.fullname;

	extract(x_user,argbuf,0);

	if (!strcmp(x_user, "."))
	{
	   strcpy(x_user, CC->last_pager);
   	}
	extract(x_msg,argbuf,1);
	
	if (!x_user[0])
   	{
   	   cprintf("%d You were not previously paged.\n", ERROR);
   	   return;
   	}

	if ( (!strucmp(x_user, "broadcast")) && (CC->usersupp.axlevel < 6) ) {
		cprintf("%d Higher access required to send a broadcast.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	/* find the target user's context and append the message */
	begin_critical_section(S_SESSION_TABLE);
	for (ccptr = ContextList; ccptr != NULL; ccptr = ccptr->next) {
	   	char *un;
	   	
	    	if (ccptr->fake_username[0])		/* <bc> */
    		   un = ccptr->fake_username;
	    	else
    		   un = ccptr->usersupp.fullname;
	   	   
		if ( (!strucmp(un, x_user))
		   || (!strucmp(x_user, "broadcast")) ) {
			strcpy(ccptr->last_pager, CC->curr_user);
			emnew = (struct ExpressMessage *)
				malloc(sizeof(struct ExpressMessage));
			emnew->next = NULL;
			sprintf(emnew->em_text, "%s from %s:\n %s\n",
				( (!strucmp(x_user, "broadcast")) ? "Broadcast message" : "Message" ),
				lun, x_msg);

			if (ccptr->FirstExpressMessage == NULL) {
				ccptr->FirstExpressMessage = emnew;
				}
			else {
				emptr = ccptr->FirstExpressMessage;
				while (emptr->next != NULL) {
					emptr = emptr->next;
					}
				emptr->next = emnew;
				}

			++message_sent;
			}
		}
	end_critical_section(S_SESSION_TABLE);

	if (message_sent > 0) {
		cprintf("%d Message sent.\n",OK);
		}
	else {
		cprintf("%d No user '%s' logged in.\n",ERROR,x_user);
		}
	}
