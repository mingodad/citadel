/*

A wrapper around the citadel_api and, to a lesser extent, transport.
Brian Costello
btx@calyx.net

*/

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "client_api.h"
#include "citadel_util.h"
#include "citadel_api.h"
#include "transport.h"

#ifndef HAVE_SNPRINTF
int snprintf (char *buf, size_t max, const char *fmt, ...);
#endif

/* 

get_form takes a formname, and returns the citadel form (MESG) in list

*/

int get_form(int sd, char *formname, citadel_list **list)
{
   int ret;
   if ((ret=cmd_mesg(sd, formname, list)) < 0)
   {
      fprintf(stderr, "Error in disp_form\n");
      return -1;
   }
   
   return 1;
}

int get_all_rooms(client_context *our_context, citadel_list **list)
{
   int ret;
   
   if ((ret = cmd_lkra(our_context->sd, list, -1)) < 0)
   {
      fprintf(stderr, "cmd_lkra failed in get_all_rooms.\n");
      return -1;
   }
   return 1;
}

int get_all_new_rooms(client_context *our_context, citadel_list **list)
{
   int ret;
   
   if ((ret = cmd_lkrn(our_context->sd, list, -1)) < 0)
   {
      fprintf(stderr, "cmd_lkrn failed in get_all_new_rooms.\n");
      return -1;
   }
   return 1;
}


/*

get_new_msg_list returns a list of the new message numbers in the current
room.

*/

int get_new_msg_list(client_context *our_context, citadel_list **list)
{
   if (cmd_msgs(our_context->sd, list, "NEW", 0)<0)
   {
      fprintf(stderr, "cmd_msgs failed in get_new_msg_list().\n");
      return -1;
   }
   
   return 1;
}

/*

get_last_msg_list returns a list of the last <num_last> message numbers from
the current room.

*/

int get_last_msg_list(client_context *our_context, int num_last, citadel_list **list)
{
   if (cmd_msgs(our_context->sd, list, "LAST", num_last)<0)
   {
      fprintf(stderr, "cmd_msgs failed in get_new_msg_list().\n");
      return -1;
   }
   
   return 1;
}

/*

get_msg_num returns a list containing the text of message number <msgnum>.
msgnum is determined by calling get_new_msg_list() or similar function.

*/

int get_msg_num(client_context *our_context, int msgnum, citadel_list **list)
{
   *list = NULL;
   
#ifdef DEBUG
   printf("get_msg_num - getting message #%d\n", msgnum);
#endif
   if (cmd_msg0(our_context->sd, list, msgnum, 0)<0)
   {
      fprintf(stderr, "cmd_msg0 failed in get_msg_num().\n");
      return -1;
   }
   
   return 1;
}

/*

goto_room goes to room <roomname> with a password <password>.  If password is
null, no password is transmitted.  If reset_msgs is set, the room's messages
are all marked read.  The parms from the goto command update our_context.

*/

int goto_room(client_context *our_context, char *roomname, char *password, citadel_parms *parms, int reset_msgs)
{
   citadel_parms *othparms;
   
   
   if (reset_msgs)
   {
      othparms = newparms();
   
      if (cmd_slrp(our_context->sd, 0, 1, othparms)<0)
      {
         fprintf(stderr, "cmd_slrp failed in goto_room.\n");
         return -1;
      }
      
      free_citadel_parms(&othparms);
   }
   
   if (cmd_goto(our_context->sd, roomname, password, parms)<0)
   {
      fprintf(stderr, "Unable to goto room %s!\n", roomname);
      return -1;
   }
   
   if (parms->argc < 9)
   {
      fprintf(stderr, "Only %d parms returned from GOTO!\n", parms->argc);
      return -1;
   }
   
   strcpy(our_context->roomname, parms->argv[0]);
   if (password)
      strcpy(our_context->roompass, password);
   else
      our_context->roompass[0] = '\0';
      
   our_context->unread_msg = atoi(parms->argv[1]);
   our_context->num_msg = atoi(parms->argv[2]);
   our_context->info_flag = atoi(parms->argv[3]);
   our_context->room_flags = atoi(parms->argv[4]);
   our_context->highest_msg_num = atoi(parms->argv[5]);
   our_context->highest_read_msg = atoi(parms->argv[6]);
   our_context->is_mail_room = atoi(parms->argv[7]);
   our_context->is_room_aide = atoi(parms->argv[8]);
   if (parms->argc > 9)
      our_context->new_mail_msgs = atoi(parms->argv[9]);
   if (parms->argc > 10)
      our_context->room_floor_no = atoi(parms->argv[10]);
      
   if (parms->line[3] == '*')
      our_context->message_waiting = 1;
   return 1;
}

/*

Sends filename <filename> over as a post

*/

int post_file(char *filename, client_context *our_context, citadel_parms *parms)
{
   int ret;
   
   ret = cmd_ent0(our_context->sd, 1, NULL, 0, 1, NULL, parms, filename);
   return(ret);
}

/*

client_connect sets up a citadel connection.  It connects, using the underlying
transport, inits the session, logs in, gets server information, gives client
information, returns the opening form in list, and finally goes to _BASEROOM_,
returning the parms from cmd_goto in parms.

*/
int client_connect(citadel_parms **parms, client_context *our_context, citadel_list **list)
{
   int sd;
   
   if ((sd = citadel_connect(our_context->host, our_context->port))<0)
   {
      fprintf(stderr, "Failed to connect to host %s.\n", our_context->host);
      return -1;
   }
   
   our_context->sd = sd;
   
   init_session(sd);
   
   if (cmd_user(sd, our_context->username)<0)
   {
      fprintf(stderr, "cmd_user failed in client_connect\n");
      return -1;
   }

   if (cmd_pass(sd, our_context->password, *parms)<0)
   {
      fprintf(stderr, "cmd_pass failed in client_connect\n");
      return -1;
   }

   if ((*parms)->argc < 6)
   {
      fprintf(stderr, "cmd_pass returned %d parms instead of 6 in client_connect()\n", (*parms)->argc);
      return -1;
   }

   our_context->connected = 1;
   strcpy(our_context->username, (*parms)->argv[0]);
   our_context->access_level = atoi((*parms)->argv[1]);
   our_context->times_called = atoi((*parms)->argv[2]);
   our_context->messages_posted = atoi((*parms)->argv[3]);
   our_context->flags = atoi((*parms)->argv[4]);
   our_context->user_number = atoi((*parms)->argv[5]);
   
   if (our_context->fake_host[0])
   {
      if (cmd_hchg(sd, our_context->fake_host) < 0)
      {
         fprintf(stderr, "cmd_hchg failed in client_connect()\n");
         return -1;
      }
   }

   if (our_context->fake_room[0])
   {
      if (cmd_rchg(sd, our_context->fake_room) < 0)
      {
         fprintf(stderr, "cmd_rchg failed in client_connect()\n");
         return -1;
      }
   }
   
   if (cmd_info(sd, list)<0)
   {
      fprintf(stderr, "cmd_info failed in client_connect()\n");
      return -1;
   }
   
/* @@ insert cmd_info code here! */   
   
   free_citadel_list(list);
   
   cmd_iden(sd, our_context->devid, our_context->cliid, our_context->verno, our_context->clientstr, our_context->hostname);

   reset_parms(parms);
   
   get_form(sd, "hello", list);
   
   goto_room(our_context, "_BASEROOM_", NULL, (*parms), 0);
   
   return sd;
}

int citadel_end_session(client_context *our_context)
{
   cmd_quit(our_context->sd);
   citadel_disconnect(our_context->sd);
   our_context->connected = 0;
   return 1;
}


/*

Returns a comma delimited description of each room's attributes:
 private
 guessname
 upload
 visdir
 anon2
 prefonly
*/ 

void get_flagbuf(int flags, char *flagbuf, int flaglen)
{
   int i;
   
   flagbuf[0] = '\0';
   snprintf(flagbuf, flaglen-1, "%s%s%s%s%s%s%s%s%s%s%s%s%s", (flags & QR_PERMANENT) ? "  Permanent": "",
   (flags & QR_PRIVATE) ? "  Private": "",(flags & QR_PASSWORDED) ? "  Passworded": "",
   (flags & QR_GUESSNAME) ? "  Guessname": "",(flags & QR_DIRECTORY) ? "  Directory": "", 
   (flags & QR_VISDIR) ? "  (Visible)": "",(flags & QR_UPLOAD) ? "  Upload": "",
   (flags & QR_DOWNLOAD) ? "  Download": "", (flags & QR_ANONONLY) ? "  Anonymous-only": "",
   (flags & QR_ANON2) ? "  Anonymous option": "",(flags & QR_NETWORK) ? "  Networked": "",
   (flags & QR_PREFONLY) ? "  Preferred-only": "",(flags & QR_READONLY) ? "  Read-only": "");
   flagbuf[flaglen-1] = '\0';
   
   i =2;
   while (i<strlen(flagbuf))
   {
      if (flagbuf[i] == ' ')
      {
         flagbuf[i] = ',';
         i++;
      }
      i++;
   }
}

int get_who_list(client_context *our_context, citadel_list **list)
{
   if (cmd_rwho(our_context->sd, list) < 0)
   {
      fprintf(stderr, "cmd_rwho failed in get_who_list().\n");
      return -1;
   }
   
   return 1;
}
                           
int send_page(client_context *our_context, char *pagewho, char *message)
{
   int ret;
   
   if ((!pagewho) || (!message))
      return -1;
      
   if ((ret = cmd_sexp(our_context->sd, pagewho, message)) < 0)
   {
      fprintf(stderr,"cmd_sexp failed in send_page.\n");
      return ret;
   }
   
   return 1;
}

int check_page(client_context *our_context, citadel_list **list)
{
   int ret;
   
   our_context->message_waiting = 0;
   if ((ret = cmd_pexp(our_context->sd, list)) < 0)
   {

   /* No error msg - it's normal to have no page message :) */

      return -1;
   }
   
   return 1;
}
