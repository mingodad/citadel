/*

gcit.h
the header file for the Gtk+ client frontend
btx@calyx.net

*/


/* User definables */

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_HOST_REVEAL "shaq.eop.gov"
#define DEFAULT_ROOM_REVEAL "secret room"

/* Not user definables */
#define CITADEL_VERSION "Gtk Citadel Client v0.1"
#define CITADEL_GTK_CLIID	1
#define CITADEL_GTK_VERNO	1

void display_room_window();
void display_who_window();
void do_post(GtkWidget *, GtkWidget *);
void create_pager(GtkWidget *, GtkWidget *);
int update_func();
void do_connect(GtkWidget *, GtkWidget *);
void get_room_new_msgs();
int get_room_msgs_func();
void do_goto(GtkWidget *, GtkWidget *);
void do_posting(GtkWidget *, GtkWidget *);
void do_send_page(GtkWidget *, GtkWidget *);
void do_nextmsg(GtkWidget *, GtkWidget *);
void do_close(GtkWidget *, GtkWidget *);
void client_quit(GtkWidget *, GtkWidget *);
int find_clist_row(GtkWidget *, char **, char *);
