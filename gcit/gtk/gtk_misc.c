/*

Misc. GTK display routines

*/

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <netinet/ip.h>
#include <gtk/gtk.h>
#include <stdarg.h>

GtkWidget *dialog_window = NULL;

void print_gtk(GtkWidget *text, char *str, ...)
{
   char buf[256];
   va_list va_args;

   if (!text)
      return;

   va_start(va_args, str);
   vsprintf(buf, str, va_args);
   
   if (text)
   {
      if (!GTK_IS_TEXT(text))
         printf("We have a non-text here - %s\n", buf);
      else
         gtk_text_insert(GTK_TEXT(text), NULL, NULL, NULL, buf, -1);
   }
   return;   
}

void gtk_killyesno()
{
   if (dialog_window)
   {
      gtk_widget_destroy(dialog_window);
      dialog_window = NULL;
   }
}

void gtk_yesno(char *msg, char *title, int def, void *yesproc, void *noproc)
{
   GtkWidget *button, *label;
   
   dialog_window = gtk_dialog_new();
   gtk_signal_connect (GTK_OBJECT(dialog_window), "destroy",
   		       GTK_SIGNAL_FUNC(gtk_widget_destroyed), &dialog_window);
   
   gtk_window_set_title(GTK_WINDOW(dialog_window), title);
   gtk_window_position(GTK_WINDOW(dialog_window), GTK_WIN_POS_MOUSE);
   
   label = gtk_label_new(msg);
   gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_window)->vbox), label, TRUE, TRUE, 0);
   gtk_widget_show(label);
   
   button = gtk_button_new_with_label("Yes");
   gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_window)->action_area), button,
                      TRUE, TRUE, 0);

   gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(yesproc), NULL);
   if (def)
      GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
   if (def == 1)
      gtk_widget_grab_default(button);
   gtk_widget_show(button);

   button = gtk_button_new_with_label("No");
   gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_window)->action_area), button,
                      TRUE, TRUE, 0);
   gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(noproc), NULL);
   if (def)
      GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
   if (def == 2)
      gtk_widget_grab_default(button);
   gtk_widget_show(button);
   gtk_widget_show(dialog_window);
}
