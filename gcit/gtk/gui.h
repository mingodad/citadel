int get_user_info(GtkWidget *w1, GtkWidget *wt);
void select_room(GtkWidget *widget, int row, int col, GdkEventButton *bevent);
void select_who(GtkWidget *widget, int row, int col, GdkEventButton *bevent);
void display_room_window(void);
void switchabout(void);
void display_who_window(void);
void do_post(GtkWidget *widget, GtkWidget *w);
void create_pager(GtkWidget *widget, GtkWidget *wdw);
int create_display_window(void);
