/*

Citadel Api
Brian Costello
btx@calyx.net

For description of any of these commands, read session.txt, distributed with
the Citadel/UX source code.

*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include "client_api.h"
#include "transport.h"
#include "citadel_util.h"
#include "citadel_api.h"

int failnum(int i)
{
   if (i == OK)
      return(i);
   if (i>0)
      return(-1 * i);
   else
      return(i);
}

int send_cmd_parms_1(int sd, char *cmd, char *parm, citadel_parms *parms)
{
   int ret;
   
   if (parm)
   {
      parms->argc = 1;
      parms->argv[0] = parm;
   }

#ifdef DEBUG   
   printf("Sending parms\n");
#endif   
   ret = citadel_sendparms(sd, parms, cmd, 1);
   if (ret < 0)
      return ret;
      
   return(parms->return_code);
}

int send_single_cmd(int sd, char *cmd)
{
   int ret;
   citadel_parms *parms;
   
   if ((parms = newparms()) == NULL)
      return -1;
   
   ret = send_cmd_parms_1(sd, cmd, NULL, parms);
   free_citadel_parms(&parms);

   return ret;
}

int send_single_cmd_parm(int sd, char *cmd, char *parm)
{
   int ret;
   citadel_parms *parms;
   
   if ((parms = newparms()) == NULL)
      return -1;

   ret = send_cmd_parms_1(sd, cmd, parm, parms);
   free_citadel_parms(&parms);

   return ret;
}

int init_session(int sd)
{
   int ret;
   citadel_parms parms;
   
   ret = citadel_recvparms(sd, &parms);
   if (ret < 0)
      return(ret);
      
   return(parms.return_code);
}

int cmd_noop(int sd)
{
   return(send_single_cmd(sd, "NOOP"));
}

int cmd_quit(int sd)
{
   return(send_single_cmd(sd, "QUIT"));
}

int cmd_echo(int sd, char *echostr)
{
   return(send_single_cmd_parm(sd, "ECHO", echostr));
}

int cmd_lout(int sd)
{
   return(send_single_cmd(sd, "LOUT"));
}

int cmd_user(int sd, char *username)
{
   int ret;

   ret = send_single_cmd_parm(sd, "USER", username);
   if (ret != MORE_DATA)
      return(failnum(ret));
   else
      return 0;
}

int cmd_pass(int sd, char *password, citadel_parms *parms)
{
   int ret;
   
   if ((ret = send_cmd_parms_1(sd, "PASS", password, parms)) != OK)
      return(failnum(ret));
   else
      return 0;
}

int cmd_setp(int sd, char *password)
{
   int ret;
   
   if ((ret = send_single_cmd_parm(sd, "SETP", password)) != OK)
      return(failnum(ret));
   else
      return 0;
}


int cmd_last_room(int sd, char *cmd, citadel_list **list, int floorno)
{
   int ret;
   
   ret = send_single_cmd(sd, cmd);
   if (ret != LISTING_FOLLOWS)
   {
      return(failnum(ret));
   }
   
#ifdef DEBUG
   printf("Receiving listing...\n");
#endif   
   *list = NULL;
   return(failnum(citadel_receive_listing(sd, list)));
}

int cmd_lkrn(int sd, citadel_list **list, int floorno)
{
   return(cmd_last_room(sd, "LKRN", list, floorno));
}
int cmd_lkro(int sd, citadel_list **list, int floorno)
{
   return(cmd_last_room(sd, "LKRO", list, floorno));
}
int cmd_lzrm(int sd, citadel_list **list, int floorno)
{
   return(cmd_last_room(sd, "LZRM", list, floorno));
}
int cmd_lkra(int sd, citadel_list **list, int floorno)
{
   return(cmd_last_room(sd, "LKRA", list, floorno));
}
int cmd_lkms(int sd, citadel_list **list, int floorno)
{
   return(cmd_last_room(sd, "LKMS", list, floorno));
}

int cmd_getu(int sd, citadel_parms *parms)
{
   int ret;
   
   if ((ret = send_cmd_parms_1(sd, "GETU", NULL, parms)) != OK)
      return(failnum(ret));
   else
      return 0;
}

int cmd_setu(int sd, int width, int height, int option_bits)
{
   int ret;
   char strbuf[256];
   
   snprintf(strbuf,  sizeof(strbuf)-1, "SETU %d|%d|%d", width, height, option_bits);
   strbuf[sizeof(strbuf)-1] = '\0';
   
   if ((ret=send_single_cmd(sd, strbuf)) != OK)
      return(failnum(ret));
   else
      return 0;
}

int cmd_goto(int sd, char *roomname, char *password, citadel_parms *parms)
{
   int ret;
   char strbuf[256];
   
   if (password)
      snprintf(strbuf, sizeof(strbuf)-1, "GOTO %s|%s", roomname, password);
   else
      snprintf(strbuf, sizeof(strbuf)-1, "GOTO %s|", roomname);
      
   strbuf[sizeof(strbuf)-1] = '\0';
   
   if ((ret = send_cmd_parms_1(sd, strbuf, NULL, parms)) != OK)
      return(failnum(ret));
   else
      return 0;
}

int cmd_msgs(int sd, citadel_list **list, char *cmd, int number)
{
   int ret;
   char strbuf[CITADEL_STR_SIZE];
   
   if (number>0)
      snprintf(strbuf, CITADEL_STR_SIZE-1, "MSGS %s|%d", cmd, number);
   else
      snprintf(strbuf, CITADEL_STR_SIZE-1, "MSGS %s", cmd);
   
   strbuf[CITADEL_STR_SIZE-1] = '\0';
   
   if ((ret = send_single_cmd(sd, strbuf)) != LISTING_FOLLOWS)
      return(failnum(ret));
   
   *list = NULL;
   return(failnum(citadel_receive_listing(sd, list)));
}


int cmd_msg0(int sd, citadel_list **list, int msgnum, int header_only)
{
   int ret;
   char strbuf[CITADEL_STR_SIZE];
   
   snprintf(strbuf, CITADEL_STR_SIZE-1, "MSG0 %d|%d", msgnum, header_only);
   
   strbuf[CITADEL_STR_SIZE-1] = '\0';
   if ((ret = send_single_cmd(sd, strbuf)) != LISTING_FOLLOWS)
      return(failnum(ret));
   
   *list = NULL;
   return(failnum(citadel_receive_listing(sd, list)));
}

int cmd_whok(int sd, citadel_list **list)
{
   int ret;
   
   ret = send_single_cmd(sd, "WHOK");
   if (ret != LISTING_FOLLOWS)
   {
      return(failnum(ret));
   }
   
   *list = NULL;
   return(failnum(citadel_receive_listing(sd, list)));
}

int cmd_info(int sd, citadel_list **list)
{
   int ret;
   
   ret = send_single_cmd(sd, "INFO");
   if (ret != LISTING_FOLLOWS)
   {
      return(failnum(ret));
   }
   
   *list = NULL;
   return(failnum(citadel_receive_listing(sd, list)));
}

int cmd_rdir(int sd, citadel_parms *parms, citadel_list **list)
{
   int ret;
   
   ret = send_cmd_parms_1(sd, "RDIR", NULL, parms);
   if (ret != LISTING_FOLLOWS)
   {
      return(failnum(ret));
   }
   return(failnum(citadel_receive_listing(sd, list)));
}   

int cmd_slrp(int sd, int msgnum, int highest, citadel_parms *parms)
{
   int ret;
   char strbuf[32];
   
   if (!highest)
      snprintf(strbuf, CITADEL_STR_SIZE-1, "%d", msgnum);
   else
      strcpy(strbuf, "HIGHEST");
   
   if ((ret = send_cmd_parms_1(sd, "SLRP", strbuf, parms)) != OK)
      return(failnum(ret));
   else
      return(1);
}

int cmd_invt(int sd, char *username)
{
   int ret;
   char strbuf[CITADEL_STR_SIZE];
   
   snprintf(strbuf, CITADEL_STR_SIZE-1, "INVT %s", username);
   
   strbuf[CITADEL_STR_SIZE-1] = '\0';
   if ((ret = send_single_cmd(sd, strbuf)) != OK)
      return(failnum(ret));
   
   return(1);
}

int cmd_kick(int sd, char *username)
{
   int ret;
   char strbuf[CITADEL_STR_SIZE];
   
   snprintf(strbuf, CITADEL_STR_SIZE-1, "KICK %s", username);
   
   strbuf[CITADEL_STR_SIZE-1] = '\0';
   if ((ret = send_single_cmd(sd, strbuf)) != OK)
      return(failnum(ret));
   
   return(1);
}

int cmd_getr(int sd, citadel_parms *parms)
{
   int ret;
   
   if ((ret = send_cmd_parms_1(sd, "GETR", NULL, parms)) != OK)
      return(failnum(ret));
   else
      return(1);
}

int cmd_setr(int sd, char *roomname, char *password, char *directory, int flags, int bump, int floorno)
{
   int ret;
   char strbuf[CITADEL_STR_SIZE];
   
   snprintf(strbuf, CITADEL_STR_SIZE-1, "SETR %s|%s|%s|%d|%d|%d", roomname, (password) ? password : "",
                                         (directory) ? directory : "", flags, bump, floorno);
   
   strbuf[CITADEL_STR_SIZE-1] = '\0';
   if ((ret = send_single_cmd(sd, strbuf)) != OK)
      return(failnum(ret));
   return(1);
}

int cmd_geta(int sd, citadel_parms *parms)
{
   int ret;
   if ((ret = send_cmd_parms_1(sd, "GETA", NULL, parms)) != OK)
      return(failnum(ret));
   else
      return(1);
}

int cmd_seta(int sd, char *newaide)
{
   int ret;
   
   if ((ret = send_single_cmd_parm(sd, "SETA", newaide)) != OK)
      return(failnum(ret));
   else
      return(1);
}

int cmd_ent0(int sd, int postflag, char *recipient, int anonymous, int format, char *postname, citadel_parms *parms, char *local_filename)
{
   int ret;
   char strbuf[CITADEL_STR_SIZE];
   
   snprintf(strbuf, CITADEL_STR_SIZE-1, "ENT0 %d|%s|%d|%d|%s|", postflag, (recipient) ? recipient : "",
                                         anonymous, format, (postname) ? postname : "");
   
   strbuf[CITADEL_STR_SIZE-1] = '\0';
   if ((ret = send_cmd_parms_1(sd, strbuf, NULL, parms)) == OK)
      return(1);
   
   if (ret == SEND_LISTING)
      ret = citadel_send_listing_file(sd,local_filename);
   
   return(ret);
}

int cmd_rinf(int sd, citadel_list **list)
{
   int ret;
   
   ret = send_single_cmd(sd, "RINF");
   if (ret != LISTING_FOLLOWS)
   {
      return(failnum(ret));
   }
   return(failnum(citadel_receive_listing(sd, list)));
}


int cmd_mesg(int sd, char *msgname, citadel_list **list)
{
   int ret;
   
   ret = send_single_cmd_parm(sd, "MESG", msgname);
   if (ret != LISTING_FOLLOWS)
   {
      return(failnum(ret));
   }
   
   *list = NULL;
   return(failnum(citadel_receive_listing(sd, list)));
}

int cmd_rwho(int sd, citadel_list **list)
{
   int ret;
   
   ret = send_single_cmd(sd, "RWHO");
   if (ret != LISTING_FOLLOWS)
   {
      return(failnum(ret));
   }
   
   *list = NULL;
   return(failnum(citadel_receive_listing(sd, list)));
}

int cmd_iden(int sd, int devid, int cliid, int verno, char *clientstr, char *hostname)
{
   int ret;
   char strbuf[CITADEL_STR_SIZE];
   
   snprintf(strbuf, CITADEL_STR_SIZE-1, "IDEN %d|%d|%d|%s|%s|", devid, cliid, verno, 
                                         clientstr, hostname);
   
   strbuf[CITADEL_STR_SIZE-1] = '\0';
   if ((ret = send_single_cmd(sd, strbuf)) != OK)
      return(failnum(ret));
   
   return(1);
}


int cmd_sexp(int sd, char *username, char *msg)
{
   int ret;
   char strbuf[256];
   
   snprintf(strbuf, sizeof(strbuf)-1, "SEXP %s|%s", username, msg);
   strbuf[sizeof(strbuf)-1] = '\0';
   
   if ((ret = send_single_cmd(sd, strbuf)) != OK)
   {
      fprintf(stderr, "Command %s failed in cmd_sexp\n", strbuf);
      return(failnum(ret));
   }
   
   return 1;
}

int cmd_pexp(int sd, citadel_list **list)
{
   int ret;
   
   if ((ret = send_single_cmd(sd, "PEXP")) != LISTING_FOLLOWS)
   {
#ifdef DEBUG
      fprintf(stderr, "No express message waiting!  Returning %d\n", failnum(ret));
#endif   
      return(failnum(ret));
   }
   
   *list = NULL;
   return(failnum(citadel_receive_listing(sd, list)));
}

int cmd_hchg(int sd, char *hostname)
{
   int ret;
   
   if ((ret = send_single_cmd_parm(sd, "HCHG", hostname)) != OK)
   {
      fprintf(stderr, "cmd_hchg was unable to send the cmd HCHG %s.\n",hostname);
      return(failnum(ret));
   }
   
   return 1;
}

int cmd_rchg(int sd, char *roomname)
{
   int ret;
   
   if ((ret = send_single_cmd_parm(sd, "RCHG", roomname)) != OK)
   {
      fprintf(stderr, "cmd_rchg was unable to send the cmd RCHG %s.\n",roomname);
      return(failnum(ret));
   }
   
   return 1;
}
int cmd_uchg(int sd, char *username)
{
   int ret;
   
   if ((ret = send_single_cmd_parm(sd, "UCHG", username)) != OK)
   {
      fprintf(stderr, "cmd_uchg was unable to send the cmd UCHG %s.\n",username);
      return(failnum(ret));
   }
   
   return 1;
}
