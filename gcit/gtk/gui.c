/*

GUI ugliness offloaded here!

*/

#include "config.h"
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
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
#include "gtk_misc.h"

GtkWidget *page_user, *page_msg, *user_e, *pass_e, *host_e;
GtkWidget *user_info_window = NULL;
GtkWidget *maintext, *posttext, *chattext, *roomlist, *wholist, *pager;
GtkWidget *mainwindow, *roomwindow, *whowindow;
GtkWidget *aboutwindow = NULL;
GdkPixmap *pixmap;
GtkWidget *image_drawing;
extern client_context our_context;
extern char lasthost[];
extern char lastusername[];
extern char lastpassword[];

int changed_goto=0;
int next_goto_row;
int cur_goto_row;

int get_user_info(GtkWidget *w1, GtkWidget *wt)
{
   GtkWidget *box1, *box2;
   GtkWidget *separator;
   GtkWidget *button;
   GtkWidget *label;
   struct utsname utsname;


   if (!user_info_window)
   {
      our_context.port = 504;
      our_context.devid = CITADEL_API_DEVID;
      our_context.cliid = CITADEL_GTK_CLIID;
      our_context.verno = CITADEL_GTK_VERNO;
      strcpy(our_context.fake_host, DEFAULT_HOST_REVEAL);
      strcpy(our_context.fake_room, DEFAULT_ROOM_REVEAL);
      strcpy(our_context.clientstr, CITADEL_VERSION);
      if (uname(&utsname) < 0)
         strcpy(our_context.hostname, "unknown");
      else
         strncpy(our_context.hostname, utsname.nodename, sizeof(our_context.hostname));
      user_info_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_signal_connect (GTK_OBJECT (user_info_window), "destroy",
                          GTK_SIGNAL_FUNC(gtk_widget_destroyed),
                          &user_info_window);
      gtk_window_set_title (GTK_WINDOW (user_info_window), "Please login");
      gtk_widget_set_uposition(user_info_window, 401, 0);
      gtk_container_border_width (GTK_CONTAINER (user_info_window), 0);
      
      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (user_info_window), box1);
      gtk_widget_show (box1);
      
      box2 = gtk_vbox_new (FALSE, 5);
      gtk_container_border_width (GTK_CONTAINER (box2), 5);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);
      
      label = gtk_label_new("Host:");
      gtk_box_pack_start(GTK_BOX (box2), label, TRUE, TRUE, 0);
      gtk_widget_show (label);
      
      host_e = gtk_entry_new ();
      gtk_entry_set_text (GTK_ENTRY (host_e), lasthost);
      gtk_editable_select_region (GTK_EDITABLE (host_e), 0, -1);
      gtk_box_pack_start (GTK_BOX (box2), host_e, TRUE, TRUE, 0);
      gtk_widget_show (host_e);
      
      separator = gtk_hseparator_new();
      gtk_box_pack_start(GTK_BOX(box2), separator, TRUE, TRUE, 0);
      gtk_widget_show(separator);
      
      label = gtk_label_new("Username:");
      gtk_box_pack_start(GTK_BOX (box2), label, TRUE, TRUE, 0);
      gtk_widget_show (label);
      
      user_e = gtk_entry_new ();
      gtk_entry_set_text (GTK_ENTRY (user_e), lastusername);
      gtk_box_pack_start (GTK_BOX (box2), user_e, TRUE, TRUE, 0);
      gtk_widget_show (user_e);
      
      label = gtk_label_new("Password:");
      gtk_box_pack_start(GTK_BOX (box2), label, TRUE, TRUE, 0);
      gtk_widget_show (label);
      
      pass_e = gtk_entry_new ();
      gtk_entry_set_text (GTK_ENTRY (pass_e), lastpassword);
      gtk_box_pack_start (GTK_BOX (box2), pass_e, TRUE, TRUE, 0);
      gtk_entry_set_visibility(GTK_ENTRY(pass_e), FALSE);
      gtk_widget_show (pass_e);

      if (gtk_entry_get_text(GTK_ENTRY(user_e)))
         gtk_entry_select_region(GTK_ENTRY(user_e), 0, strlen(gtk_entry_get_text(GTK_ENTRY(user_e)))-1);

      separator = gtk_hseparator_new();
      gtk_box_pack_start(GTK_BOX(box2), separator, TRUE, TRUE, 0);
      gtk_widget_show(separator);
      
      box2 = gtk_hbox_new(FALSE, 10);
      gtk_container_border_width(GTK_CONTAINER(box2), 5);
      gtk_box_pack_start(GTK_BOX(box1), box2, TRUE, TRUE, 0);
      gtk_widget_show(box2);
      
      button = gtk_button_new_with_label("Ok");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                                 GTK_SIGNAL_FUNC(do_connect),
                                 GTK_OBJECT (maintext));
      GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default(button);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_button_new_with_label("Cancel");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                                 GTK_SIGNAL_FUNC(gtk_widget_destroy),
                                 GTK_OBJECT (user_info_window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);
      
   }

   if (!GTK_WIDGET_VISIBLE(user_info_window))
      gtk_widget_show(user_info_window);
   else
      gtk_widget_destroy(user_info_window);
   return 1;
}


void select_room(GtkWidget *widget, int row, int col, GdkEventButton *bevent)
{
   char *text;
   
   gtk_clist_get_text(GTK_CLIST(widget), row, 0, &text);
   strcpy(our_context.selected_room, text);
   changed_goto = 1;
   return;
}

void select_who(GtkWidget *widget, int row, int col, GdkEventButton *bevent)
{
   char *text;
   
   gtk_clist_get_text(GTK_CLIST(widget), row, 0, &text);
   strcpy(our_context.selected_who, text);
   return;
}

void display_room_window()
{

#define ROOM_COLS 4
  
   static char *titles[ROOM_COLS] =
   { 
      "Room Name",
      "New Messages",
      "Total Messages",
      "Flags"
   };
                                    
   char text[ROOM_COLS][128];
   char *texts[ROOM_COLS];
   GtkWidget *box1;
   GtkWidget *box2;
   GtkWidget *clist;
   GtkWidget *button;
   GtkWidget *separator;
   citadel_list *list, *t_list;
   citadel_parms *parms;
   char flagbuf[127];
           
   if (!roomwindow)
   {
      roomwindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_signal_connect (GTK_OBJECT (roomwindow), "destroy",
                          GTK_SIGNAL_FUNC(gtk_widget_destroyed),
                          &roomwindow);
      gtk_window_set_title (GTK_WINDOW (roomwindow), "Rooms");
      gtk_widget_set_uposition(roomwindow, 0, 420);
      gtk_container_border_width (GTK_CONTAINER (roomwindow), 0);
      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (roomwindow), box1);
      gtk_widget_show (box1);
      
      gtk_widget_set_usize(roomwindow, 380, 250);
      box2 = gtk_vbox_new (FALSE, 5);
      gtk_container_border_width (GTK_CONTAINER (box2), 5);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);
      clist = gtk_clist_new_with_titles (ROOM_COLS, titles);
      gtk_clist_set_row_height (GTK_CLIST (clist), 20);
      gtk_signal_connect (GTK_OBJECT (clist),
                          "select_row",
                          (GtkSignalFunc) select_room,
                          NULL);
      gtk_clist_set_column_width (GTK_CLIST (clist), 0, 100);
      gtk_clist_set_selection_mode (GTK_CLIST (clist), GTK_SELECTION_BROWSE);
      gtk_clist_set_policy (GTK_CLIST (clist),
                            GTK_POLICY_AUTOMATIC,
                            GTK_POLICY_AUTOMATIC);
      gtk_container_border_width (GTK_CONTAINER (clist), 5);
      gtk_box_pack_start (GTK_BOX (box2), clist, TRUE, TRUE, 0);
      
      gtk_clist_set_column_width (GTK_CLIST (clist), 0, 90);
      gtk_clist_set_column_width (GTK_CLIST (clist), 1, 90);
      gtk_clist_set_column_width (GTK_CLIST (clist), 2, 90);
      gtk_clist_set_column_width (GTK_CLIST (clist), 3, 150);
     
      gtk_widget_show (clist);
      
      roomlist = clist;
      
      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);
      box2 = gtk_hbox_new (FALSE, 5);
      gtk_container_border_width (GTK_CONTAINER (box2), 5);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);
      
      button = gtk_button_new_with_label ("Goto");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                                 GTK_SIGNAL_FUNC(do_goto),
                                 NULL);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Update");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                                 GTK_SIGNAL_FUNC(get_room_msgs_func),
                                 (void *)1);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
      
      parms = newparms();
      get_all_rooms(&our_context, &list);
      texts[0] = text[0];
      texts[1] = text[1];
      texts[2] = text[2];
      texts[3] = text[3];
      
      for (t_list = list; (t_list); t_list = t_list->next)
      {
         if ((!t_list->listitem) || (!t_list->listitem[0]))
            continue;
/*         printf("Working with listitem=%s\n", t_list->listitem); */
         citadel_parseparms(t_list->listitem, parms);
/*         printf("There are %d args.\n", parms->argc); */
         strcpy(text[0], parms->argv[0]);
         get_flagbuf(atoi(parms->argv[1]), flagbuf, sizeof(flagbuf));
         strncpy(text[3], flagbuf, sizeof(text[3])-1);
         reset_parms(&parms);
         strcpy(text[1], "0");
         strcpy(text[2], "");
         gtk_clist_append (GTK_CLIST (clist), texts);
      }
      
      free_citadel_list(&list);
      free_citadel_parms(&parms);
      changed_goto = 0;
   }
   if (!GTK_WIDGET_VISIBLE (roomwindow))
   {
      gtk_widget_show (roomwindow);
      return;
   }
   else
   {
      gtk_widget_destroy (roomwindow);
      roomlist = NULL;
   }
}



GdkPixmap *make_pixmap_from_filename(char *fn, GtkWidget *image_drawing, int *w, int *h)
{
   GdkImlibImage *im;
   GdkPixmap *pmap;

   im = gdk_imlib_load_image(fn);
   *w = im->rgb_width;
   *h = im->rgb_height;
   gdk_imlib_render(im, *w, *h);
   pmap = gdk_imlib_move_image(im);
   gdk_imlib_kill_image(im);
   return pmap;
}

void draw_img(GtkWidget *widget, GdkEventConfigure *event)
{
   if (!image_drawing)
   {
      fprintf(stderr, "No image drawing!\n");
      return;
   }
   if (!image_drawing->window)
   {
      fprintf(stderr, "No image drawing window!\n");
      return;
   }
   if (!pixmap)
   {
      fprintf(stderr, "No pixmap!\n");
      return;
   }
   
   gdk_window_set_back_pixmap(image_drawing->window, pixmap, FALSE);
   gdk_window_clear(image_drawing->window);
   gdk_flush();
}

void button_win(GtkWidget *widget, GdkEventButton *event)
{
   gtk_widget_destroy(aboutwindow);
}

void switchabout(void)
{
   char fn[256];
   int w, h;
   
   if (!aboutwindow)
   {
      aboutwindow= gtk_window_new(GTK_WINDOW_TOPLEVEL);
      gtk_widget_set_name(aboutwindow, "About");
      gtk_widget_set_uposition(aboutwindow, 400,400);  

      gtk_window_set_policy (GTK_WINDOW(aboutwindow), TRUE, TRUE, FALSE);
         
      gtk_signal_connect (GTK_OBJECT (aboutwindow), "destroy",
                          GTK_SIGNAL_FUNC(gtk_widget_destroyed), &aboutwindow);
      gtk_window_set_title (GTK_WINDOW (aboutwindow), "About");
      gtk_container_border_width (GTK_CONTAINER (aboutwindow), 0);
      
      image_drawing = gtk_drawing_area_new();
      sprintf(fn, "%s/gcit.gif", Q_PIXMAP_DIR);
      pixmap = make_pixmap_from_filename(fn, image_drawing, &w, &h);
      gtk_drawing_area_size(GTK_DRAWING_AREA(image_drawing),w, h);
      gtk_signal_connect(GTK_OBJECT(image_drawing),"configure_event",GTK_SIGNAL_FUNC(draw_img), NULL);
      gtk_signal_connect(GTK_OBJECT(image_drawing),"button_press_event",GTK_SIGNAL_FUNC(button_win), NULL);
      gtk_widget_set_events(image_drawing,GDK_BUTTON_PRESS_MASK);
      gtk_container_add(GTK_CONTAINER(aboutwindow),image_drawing);
      gtk_widget_show(image_drawing);
   }
   if (!GTK_WIDGET_VISIBLE(aboutwindow))
      gtk_widget_show(aboutwindow);
   else
      gtk_widget_destroy(aboutwindow);
      
   return;

}

void display_who_window()
{
#define ROOM_COLS 4
  
   static char *titles[ROOM_COLS] =
   { 
      "User Name",
      "Room",
      "Host",
      "Flags"
   };
                                    
   char text[ROOM_COLS][50];
   char *texts[ROOM_COLS];
   GtkWidget *box1;
   GtkWidget *box2;
   GtkWidget *clist;
   GtkWidget *button;
   GtkWidget *separator;
   citadel_list *list, *t_list;
   citadel_parms *parms;
           
   if (!whowindow)
   {
      whowindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_signal_connect (GTK_OBJECT (whowindow), "destroy",
                          GTK_SIGNAL_FUNC(gtk_widget_destroyed),
                          &whowindow);
      gtk_window_set_title (GTK_WINDOW (whowindow), "Who");
      gtk_widget_set_uposition(whowindow, 385, 420);
      gtk_container_border_width (GTK_CONTAINER (whowindow), 0);
      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (whowindow), box1);
      gtk_widget_show (box1);
      
      gtk_widget_set_usize(whowindow, 380, 250);
      box2 = gtk_vbox_new (FALSE, 5);
      gtk_container_border_width (GTK_CONTAINER (box2), 5);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);
      clist = gtk_clist_new_with_titles (ROOM_COLS, titles);
      gtk_clist_set_row_height (GTK_CLIST (clist), 20);
      gtk_signal_connect (GTK_OBJECT (clist),
                          "select_row",
                          (GtkSignalFunc) select_who,
                          NULL);
      gtk_clist_set_column_width (GTK_CLIST (clist), 0, 100);
      gtk_clist_set_selection_mode (GTK_CLIST (clist), GTK_SELECTION_BROWSE);
      gtk_clist_set_policy (GTK_CLIST (clist),
                            GTK_POLICY_AUTOMATIC,
                            GTK_POLICY_AUTOMATIC);
      gtk_container_border_width (GTK_CONTAINER (clist), 5);
      gtk_box_pack_start (GTK_BOX (box2), clist, TRUE, TRUE, 0);
      
      gtk_clist_set_column_width (GTK_CLIST (clist), 0, 90);
      gtk_clist_set_column_width (GTK_CLIST (clist), 1, 90);
      gtk_clist_set_column_width (GTK_CLIST (clist), 2, 90);
      gtk_clist_set_column_width (GTK_CLIST (clist), 3, 150);
      
      gtk_widget_show (clist);
      
      wholist = clist;
      
      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);
      box2 = gtk_hbox_new (FALSE, 5);
      gtk_container_border_width (GTK_CONTAINER (box2), 5);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);
      
      button = gtk_button_new_with_label ("Page");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                                 GTK_SIGNAL_FUNC(create_pager),
                                 NULL);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Update");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                                 GTK_SIGNAL_FUNC(do_goto),
                                 NULL);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
      
      texts[0] = text[0];
      texts[1] = text[1];
      texts[2] = text[2];
      texts[3] = text[3];
      
      parms = newparms();

      get_who_list(&our_context, &list);
      for (t_list = list; t_list; t_list = t_list->next)
      {
         citadel_parseparms(t_list->listitem, parms);
         strcpy(text[0], parms->argv[1]);
         citadel_parseparms(t_list->listitem, parms);
         strcpy(text[1], parms->argv[2]);
         citadel_parseparms(t_list->listitem, parms);
         strcpy(text[2], parms->argv[3]);
         citadel_parseparms(t_list->listitem, parms);
         if (parms->argv[7])
            strcpy(text[3], parms->argv[7]);
         else
            strcpy(text[3], "N/A");
         gtk_clist_append (GTK_CLIST (clist), texts);
         reset_parms(&parms);
      }
      
      free_citadel_list(&list);
      free_citadel_parms(&parms);
   }
   if (!GTK_WIDGET_VISIBLE (whowindow))
      gtk_widget_show (whowindow);
   else
   {
      gtk_widget_destroy (whowindow);
      wholist = NULL;
   }
}


void do_post(GtkWidget *widget, GtkWidget *w)
{
   static GtkWidget *window = NULL;
   GtkWidget *box1, *box2, *button, *table, *separator, *hscrollbar;
   GtkWidget *vscrollbar, *text;
   
   if (!window)
   {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_widget_set_name (window, "text window");
      gtk_widget_set_usize (window, 400, 400);
      gtk_widget_set_uposition(window, 505, 0);
      gtk_window_set_policy (GTK_WINDOW(window), TRUE, TRUE, FALSE);
      gtk_signal_connect (GTK_OBJECT (window), "destroy",
                          GTK_SIGNAL_FUNC(gtk_widget_destroyed),
                          &window);
   
      gtk_window_set_title (GTK_WINDOW (window), "Post a message");
      gtk_container_border_width (GTK_CONTAINER (window), 0);
      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);
      box2 = gtk_vbox_new (FALSE, 5);
      gtk_container_border_width (GTK_CONTAINER (box2), 5);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);
      
      table = gtk_table_new (2, 2, FALSE);
      gtk_table_set_row_spacing (GTK_TABLE (table), 0, 2);
      gtk_table_set_col_spacing (GTK_TABLE (table), 0, 2);
      gtk_box_pack_start (GTK_BOX (box2), table, TRUE, TRUE, 0);
      gtk_widget_show (table);
   
      text = gtk_text_new (NULL, NULL);
      gtk_text_set_editable (GTK_TEXT (text), TRUE);
      gtk_table_attach (GTK_TABLE (table), text, 0, 1, 0, 1,
                        GTK_EXPAND | GTK_SHRINK | GTK_FILL,
                        GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);
      gtk_text_set_word_wrap(GTK_TEXT(text), TRUE);
      gtk_widget_show (text);
      
      posttext = (void *)text;
   
      hscrollbar = gtk_hscrollbar_new (GTK_TEXT (text)->hadj);
      gtk_table_attach (GTK_TABLE (table), hscrollbar, 0, 1, 1, 2,
                        GTK_EXPAND | GTK_FILL | GTK_SHRINK, GTK_FILL, 0, 0);
   
      gtk_widget_show (hscrollbar);
      vscrollbar = gtk_vscrollbar_new (GTK_TEXT (text)->vadj);
      gtk_table_attach (GTK_TABLE (table), vscrollbar, 1, 2, 0, 1,
                        GTK_FILL, GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);
      gtk_widget_show (vscrollbar);
      gtk_widget_realize (text);
      
      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);
   
      box2 = gtk_hbox_new (FALSE, 5);
      gtk_container_border_width (GTK_CONTAINER (box2), 5);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);


      button = gtk_button_new_with_label ("Post");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                                 GTK_SIGNAL_FUNC(do_posting),
                                 GTK_OBJECT(text));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
      
      button = gtk_button_new_with_label ("Abort");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                                 GTK_SIGNAL_FUNC(gtk_widget_destroy),
                                 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);

   }
   
   if (!GTK_WIDGET_VISIBLE(window))
      gtk_widget_show(window);
   else
   {
      posttext = NULL;
      gtk_widget_destroy(window);
   }
      
   return;
   
}

void create_pager(GtkWidget *widget, GtkWidget *wdw)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *separator;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "Pager");
      gtk_container_border_width (GTK_CONTAINER (window), 0);


      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);


      box2 = gtk_vbox_new (FALSE, 5);
      gtk_container_border_width (GTK_CONTAINER (box2), 5);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);

      label = gtk_label_new("Page who:");
      gtk_box_pack_start(GTK_BOX(box2), label, TRUE, TRUE, 0);
      gtk_widget_show(label);

      page_user = gtk_entry_new ();
      if (our_context.selected_who[0])
         gtk_entry_set_text (GTK_ENTRY (page_user), our_context.selected_who);
      else
         gtk_entry_set_text (GTK_ENTRY (page_user), "");
      gtk_editable_select_region (GTK_EDITABLE (page_user), 0, -1);
      gtk_box_pack_start (GTK_BOX (box2), page_user, TRUE, TRUE, 0);
      gtk_widget_show (page_user);

      label = gtk_label_new("Message:");
      gtk_box_pack_start(GTK_BOX(box2), label, TRUE, TRUE, 0);
      gtk_widget_show(label);

      page_msg = gtk_entry_new ();
      gtk_entry_set_text (GTK_ENTRY (page_msg), "");
      gtk_editable_select_region (GTK_EDITABLE (page_msg), 0, -1);
      gtk_box_pack_start (GTK_BOX (box2), page_msg, TRUE, TRUE, 0);
      gtk_widget_show (page_msg);

      box2 = gtk_hbox_new (FALSE, 5);
      gtk_container_border_width (GTK_CONTAINER (box2), 5);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);

      button = gtk_button_new_with_label ("Send");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(do_send_page),
				 NULL);
				 
      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);
      
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
      
      button = gtk_button_new_with_label ("Close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);
      
      pager = window;
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
  {
    gtk_widget_destroy (window);
    pager = NULL;
  }

}

int create_display_window(void)
{
   static GtkWidget *window = NULL;
   GtkWidget *box1, *box2, *button, *table, *separator, *hscrollbar;
   GtkWidget *vscrollbar, *text;
   
   if (!window)
   {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_widget_set_name (window, "text window");
      gtk_widget_set_usize (window, 500, 400);
      gtk_widget_set_uposition(window, 0, 0);
      gtk_window_set_policy (GTK_WINDOW(window), TRUE, TRUE, FALSE);
      gtk_signal_connect (GTK_OBJECT (window), "destroy",
                          GTK_SIGNAL_FUNC(gtk_widget_destroyed),
                          &window);
   
      gtk_window_set_title (GTK_WINDOW (window), CITADEL_VERSION);
      gtk_container_border_width (GTK_CONTAINER (window), 0);
      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);
      box2 = gtk_vbox_new (FALSE, 5);
      gtk_container_border_width (GTK_CONTAINER (box2), 5);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);
      
      table = gtk_table_new (2, 2, FALSE);
      gtk_table_set_row_spacing (GTK_TABLE (table), 0, 2);
      gtk_table_set_col_spacing (GTK_TABLE (table), 0, 2);
      gtk_box_pack_start (GTK_BOX (box2), table, TRUE, TRUE, 0);
      gtk_widget_show (table);
   
      text = gtk_text_new (NULL, NULL);
      gtk_text_set_editable (GTK_TEXT (text), FALSE);
      gtk_table_attach (GTK_TABLE (table), text, 0, 1, 0, 1,
                        GTK_EXPAND | GTK_SHRINK | GTK_FILL,
                        GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);
      gtk_widget_show (text);
   
      maintext = (void *)text;
      
      hscrollbar = gtk_hscrollbar_new (GTK_TEXT (text)->hadj);
      gtk_table_attach (GTK_TABLE (table), hscrollbar, 0, 1, 1, 2,
                        GTK_EXPAND | GTK_FILL | GTK_SHRINK, GTK_FILL, 0, 0);
   
      gtk_widget_show (hscrollbar);
      vscrollbar = gtk_vscrollbar_new (GTK_TEXT (text)->vadj);
      gtk_table_attach (GTK_TABLE (table), vscrollbar, 1, 2, 0, 1,
                        GTK_FILL, GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);
      gtk_widget_show (vscrollbar);
      gtk_widget_realize (text);
      
      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);
   
      box2 = gtk_hbox_new (FALSE, 5);
      gtk_container_border_width (GTK_CONTAINER (box2), 5);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);

      button = gtk_button_new_with_label ("Connect");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                                 GTK_SIGNAL_FUNC(get_user_info),
                                 GTK_OBJECT (text));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);
      
      button = gtk_button_new_with_label ("Next");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                                 GTK_SIGNAL_FUNC(do_nextmsg), 
                                 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Post");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                                 GTK_SIGNAL_FUNC(do_post),
                                 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                                 GTK_SIGNAL_FUNC(do_close),
                                 NULL);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);
      
      button = gtk_button_new_with_label ("Quit");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                                 GTK_SIGNAL_FUNC(client_quit),
                                 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
   }
   
   if (!GTK_WIDGET_VISIBLE(window))
   {
      gtk_widget_show(window);
      mainwindow = window;
   }
   else
   {
      gtk_widget_destroy(window);
      maintext = NULL;
      mainwindow = NULL;
   }
      
   return(1);
}
