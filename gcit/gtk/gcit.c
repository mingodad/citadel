/*

gcit

Protected by the red, the black and the green with a key!

Brian Costello
btx@calyx.net

*/


#include "config.h"
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <gdk_imlib.h>
#include "client_api.h"
#include "citadel_util.h"
#include "gcit.h"
#include "gui.h"
#include "gtk_misc.h"

client_context our_context;
extern GtkWidget *page_user, *page_msg, *user_e, *pass_e, *host_e;
extern GtkWidget *user_info_window;
extern GtkWidget *maintext, *posttext, *chattext, *roomlist, *wholist, *pager;
extern GtkWidget *mainwindow, *roomwindow, *whowindow;
extern int changed_goto, next_goto_row;
char lasthost[256]=DEFAULT_HOST;
char lastusername[256]={0x00};
char lastpassword[256]={0x00};

void display_list(GtkWidget *text, citadel_list *list)
{
   citadel_list *t_list;
   
   for (t_list = list; t_list; t_list = t_list->next)
      print_gtk(text, "%s\n", t_list->listitem);
   
   return;
}

void display_msg_header(GtkWidget *text, int msgnum, long post_time, char *from, char *rcpt, char *roomname, char *nodeshort)
{
   if (roomname[0])
      print_gtk(text, "\nRoom: %s\n", roomname);
   print_gtk(text, " Msg: %d\n", msgnum);
   print_gtk(text, "From: %s", from);
   if (nodeshort[0])
      print_gtk(text, " (@%s)\n", nodeshort);
   else
      print_gtk(text, "\n");
   if (rcpt[0])
      print_gtk(text, "To: %30s\n", rcpt);
   else
      print_gtk(text, "\n");
}

void display_message(GtkWidget *text, citadel_list *list)
{
   citadel_list *t_list;
   int textfound=0;
   int msgn=0;
   char path[256];
   long post_time;
   char from[256];
   char rcpt[256];
   char roomname[256];
   char nodeshort[128];
   char nodelong[256];
   
   path[0] = '\0';
   post_time = 0;
   from[0] = '\0';
   rcpt[0] = '\0';
   roomname[0] = '\0';
   nodeshort[0] = '\0';
   nodelong[0] = '\0';
   
   t_list = list;
   
   print_gtk(text, "---------------------------------------------------------------------\n");
   while ((!textfound) && (t_list))
   {
#ifdef DEBUG
      printf("Looking at listitem - %s\n", t_list->listitem);
#endif
      if (!strncasecmp(t_list->listitem, "msgn=", 5))
         msgn = atol(t_list->listitem);
      if (!strncasecmp(t_list->listitem, "time=", 5))
         post_time = atol(t_list->listitem);
      if (!strncasecmp(t_list->listitem, "path=", 5))
      {
         strncpy(path, &t_list->listitem[5], sizeof(path)-1);
         path[sizeof(path)-1] = '\0';
      }
      if (!strncasecmp(t_list->listitem, "from=", 5))
      {
         strncpy(from, &t_list->listitem[5], sizeof(from)-1);
         from[sizeof(from)-1] = '\0';
      }
      if (!strncasecmp(t_list->listitem, "rcpt=", 5))
      {
         strncpy(rcpt, &t_list->listitem[5], sizeof(rcpt)-1);
         rcpt[sizeof(rcpt)-1] = '\0';
      }
      if (!strncasecmp(t_list->listitem, "room=", 5))
      {
         strncpy(roomname, &t_list->listitem[5], sizeof(roomname)-1);
         roomname[sizeof(roomname)-1] = '\0';
      }
      if (!strncasecmp(t_list->listitem, "node=", 5))
      {
         strncpy(nodeshort, &t_list->listitem[5], sizeof(nodeshort)-1);
         nodeshort[sizeof(nodeshort)-1] = '\0';
      }
      if (!strncasecmp(t_list->listitem, "hnod=", 5))
      {
         strncpy(nodelong, &t_list->listitem[5], sizeof(nodelong)-1);
         nodelong[sizeof(nodelong)-1] = '\0';
      }
      
      if (!strncasecmp(t_list->listitem, "text", 4))
      {
         textfound = 1;
      }
      
      t_list=t_list->next;
   }
   
   display_msg_header(text, msgn,post_time, from, rcpt, roomname, nodeshort);
   display_list(text, t_list);
   print_gtk(text, "---------------------------------------------------------------------\n");
}

void display_room_info(GtkWidget *text, client_context *our_context)
{
   print_gtk(text, "\nRoom: %s   -   (%d new of %d messages total)\n", our_context->roomname, our_context->unread_msg, our_context->num_msg);
   
}


/* 

get a list of the new messages in a room

*/

void get_room_new_msgs(void)
{
   if (our_context.room_msgs)
      free_citadel_list(&our_context.room_msgs);
   get_new_msg_list(&our_context, &our_context.room_msgs);
   our_context.next_msg_ptr = our_context.room_msgs;
}

void do_nextmsg(GtkWidget *wid, GtkWidget *w)
{
   citadel_list *msgtext;

   if (our_context.next_msg_ptr)
   {
      get_msg_num(&our_context, atoi(our_context.next_msg_ptr->listitem), &msgtext);
      our_context.next_msg_ptr = our_context.next_msg_ptr->next;
      display_message((GtkWidget *)maintext, msgtext);
      free_citadel_list(&msgtext);
   }
   else
      get_room_new_msgs();
}

/*

Get the list of the last num messages, and display the first.

*/

void get_room_last_n(int num)
{
   citadel_list *msgtext;

   if (our_context.room_msgs)
      free_citadel_list(&our_context.room_msgs);
   get_last_msg_list(&our_context, num, &our_context.room_msgs);
   our_context.next_msg_ptr = our_context.room_msgs;
   
   if (our_context.next_msg_ptr)
   {
      get_msg_num(&our_context, atoi(our_context.next_msg_ptr->listitem), &msgtext);
      our_context.next_msg_ptr = our_context.next_msg_ptr->next;
      display_message((GtkWidget *)maintext, msgtext);
      free_citadel_list(&msgtext);
   }
}

void do_goto(GtkWidget *widget, GtkWidget *w)
{
   citadel_parms *parms;
   char *text;
   
   parms = newparms();

   if (!changed_goto)
   {
      if ((!our_context.new_msg_rooms) || (!our_context.next_new_msg_room))
      {
         if (our_context.new_msg_rooms)
            free_citadel_list(&our_context.new_msg_rooms);
         get_all_new_rooms(&our_context, &our_context.new_msg_rooms);
         our_context.next_new_msg_room = our_context.new_msg_rooms;
         if (!our_context.next_new_msg_room)
            return;
         citadel_parseparms(our_context.next_new_msg_room->listitem, parms);
         if (our_context.new_msg_rooms)
         {
            next_goto_row = find_clist_row(roomlist, &text, parms->argv[0]);
         }
      }
      gtk_clist_select_row(GTK_CLIST(roomlist), next_goto_row, 0);
      our_context.next_new_msg_room = our_context.next_new_msg_room->next;
      if (our_context.next_new_msg_room)
      {
         next_goto_row = find_clist_row(roomlist, &text, parms->argv[0]);
      }
   }
   
   goto_room(&our_context, our_context.selected_room, NULL, parms, 1);
   display_room_info((GtkWidget *)maintext, &our_context);
   
/*   get_room_last_n(1); */
   get_room_new_msgs();
   free_citadel_parms(&parms);
   changed_goto = 0;
}

void do_connect(GtkWidget *widget, GtkWidget *text)
{
   citadel_parms *parms;
   citadel_list *list;
   
   strcpy(our_context.username, gtk_entry_get_text(GTK_ENTRY(user_e)));
   strcpy(our_context.password, gtk_entry_get_text(GTK_ENTRY(pass_e)));
   strcpy(our_context.host, gtk_entry_get_text(GTK_ENTRY(host_e)));
   strcpy(lastusername, our_context.username);
   strcpy(lastpassword, our_context.password);
   strcpy(lasthost, our_context.hostname);

   parms = newparms();
   
   if (client_connect(&parms, &our_context, &list) <0)
   {
      print_gtk(text, "Unable to connect to host %s port %d.\n", our_context.host, our_context.port);
      free_citadel_parms(&parms);
      return;
   }
   reset_parms(&parms);

   if (GTK_WIDGET_VISIBLE(user_info_window))
      gtk_widget_destroy(user_info_window);
   
   print_gtk(text, "Connected to %s port %d.\n", our_context.host, our_context.port);
      
   display_list(text, list);	/* Display the opening "hello" banner */
   free_citadel_list(&list); 
   
   display_room_info(text, &our_context);
   display_room_window();
   printf("@@ done display_room_window()\n");   
   display_who_window();
   
   get_room_new_msgs();

   gtk_timeout_add(30000, (GtkFunction)update_func, NULL);
   gtk_timeout_add(100000, (GtkFunction)get_room_msgs_func, (int)0);
}

void do_close(GtkWidget *widget, GtkWidget *w)
{
   if (our_context.connected)
   {
      print_gtk((GtkWidget *) maintext, "Disconnected from %s port %d.\n", our_context.host, our_context.port);
      citadel_end_session(&our_context);  
      if (roomwindow)
      {
         gtk_widget_destroy(roomwindow);
         roomlist = NULL;
      }
      if (whowindow)
      {
         gtk_widget_destroy(whowindow);
         wholist = NULL;
      }
   }
   return;
}


void do_posting(GtkWidget *widget, GtkWidget *t)
{
   int textlen, i;
   int fd, c;
   char ftemplate[256];
   citadel_parms *parms;
   
   parms = newparms();
   
   sprintf(ftemplate, "/tmp/citgtk_XXXXXX");
   fd = mkstemp(ftemplate);
   
   textlen = gtk_text_get_length(GTK_TEXT(t));
   for (i=0; i<textlen; i++)
   {
      c = GTK_TEXT_INDEX(GTK_TEXT(t), i);
      if (write(fd, &c, 1) < 0)
      {
         perror("Write temp post data");
         exit(1);
      }
   }
   close(fd);
   
   if (post_file(ftemplate, &our_context, parms)<0)
      print_gtk((GtkWidget *)maintext, "Unable to post the message!\n");
   else
      print_gtk((GtkWidget *)maintext, "Message posted.\n");
   
   get_room_new_msgs();
   free_citadel_parms(&parms);
   do_post(widget, t);
   return;
}


void do_send_page(GtkWidget *widget, GtkWidget *w)
{
   int ret;
   char *username=NULL;
   char *msg=NULL;
   char *sptr;
   char newmsg[78];
   int len, subamt;
   
   if (page_user)
      username = GTK_ENTRY(page_user)->text;
   if (page_msg)
      msg = GTK_ENTRY(page_msg)->text;
      
   if (msg)
      len = strlen(msg);
   else
      return;
   
   sptr = msg;
   while (len > 0)
   {
      if (len > 76)
         subamt = 76;
      else
         subamt = len;
      
      strncpy(newmsg, sptr, subamt);
      sptr += subamt;
      newmsg[subamt] = '\0';
      len -= subamt;
      if ((ret = send_page(&our_context, username, newmsg))<0)
      {
         fprintf(stderr, "Unable to send the page.\n");
         return;
      }
      
   }
   
   gtk_entry_set_text(GTK_ENTRY(page_msg), "");
   
   return;
}

void client_quit(GtkWidget *widget, GtkWidget *window)
{
   do_close(NULL, NULL);
   gtk_widget_destroy(window);
   gtk_main_quit();
}


int create_main_window(void)
{
   create_display_window();
   switchabout();
   return 1;
}

int find_clist_row(GtkWidget *list, char **buf, char *searchstr)
{
   int finished = -1;   
   int row;
   
   for (row=0;((row<GTK_CLIST(list)->rows) && (finished<0)); row++)
   {
      gtk_clist_get_text(GTK_CLIST(list), row, 0, buf);
      if (!strncmp((*buf), searchstr, strlen(searchstr)))
      {
         finished = row;
      }
   }
   
   return finished;
}

int get_room_msgs_func(int allrooms)
{
   citadel_list *list, *t_list;
   citadel_parms *parms;
   char *cptr;
   int r;
   
   if (!our_context.connected)
      return FALSE;

   parms = newparms();
   
   if (allrooms)
      get_all_rooms(&our_context, &list);
   else
      get_all_new_rooms(&our_context, &list);
   for (t_list = list; t_list; t_list=t_list->next)
   {
      if (citadel_parseparms(t_list->listitem, parms)<0)
      {
         fprintf(stderr, "Error: Citadel parseparms failed of %s.\n", t_list->listitem);
         return TRUE;
      }
      
      if ((r=find_clist_row(roomlist, &cptr, parms->argv[0]))<0)
      {
         fprintf(stderr, "Error- room %s not found!\n", parms->argv[0]);
         exit(1);
      }
      else
         printf("Room %s found on row #%d/%d\n", cptr, r, GTK_CLIST(roomlist)->rows);

      reset_parms(&parms);
      
      if (goto_room(&our_context, cptr, NULL, parms, 0)<0)
      {
         fprintf(stderr, "Error: Unable to go to room %s.\n", t_list->listitem);
         return TRUE;
      }

      printf("Making row %d/1 %s\n", r, parms->argv[1]);
      printf("Making row %d/2 %s\n", r, parms->argv[2]);
      gtk_clist_set_text(GTK_CLIST(roomlist), r, 1, parms->argv[1]);
      gtk_clist_set_text(GTK_CLIST(roomlist), r, 2, parms->argv[2]);
      reset_parms(&parms);
   }
   free_citadel_list(&list);
   reset_parms(&parms);

   if (goto_room(&our_context, our_context.selected_room, NULL, parms, 0)<0)
   {
      fprintf(stderr, "Error: Unable to go to room %s.\n", t_list->listitem);
      return TRUE;
   }
   free_citadel_parms(&parms);
   return TRUE;
}

int update_func(void)
{
   citadel_list *list;
   
   if (!our_context.connected)
      return FALSE;

   if (check_page(&our_context, &list)>0)
   {
      print_gtk(maintext, "Received page:\n");
      display_list(maintext, list);
      free_citadel_list(&list);
   }
   
   return TRUE;
      
      
}

int main(int argc, char **argv)
{
   char path[512];
   gtk_set_locale();
   gtk_init(&argc, &argv);
   gdk_imlib_init();
   
   snprintf(path, sizeof(path)-1, "%s/gcitrc", DATA_DIR);
   path[sizeof(path)-1] = '\0';
   gtk_rc_parse(path);
   
   bzero(&our_context, sizeof(our_context));
   
   create_main_window();
   
   gtk_main();
   
   return(0);
}


