/* $Id$ */
void listzrooms(void);
void readinfo(void);
void forget(void);
void entroom(void);
void killroom(void);
void invite(void);
void kickout(void);
void editthisroom(void);
void roomdir(void);
void download(int proto);
void ungoto(void);
void whoknows(void);
void enterinfo(void);
void knrooms(int kn_floor_mode);
void load_floorlist(void);
void create_floor(void);
void edit_floor(void);
void kill_floor(void);
void enter_bio(void);
void download_to_local_disk(char *, long);
void hit_any_key(void);


/* 
 * This struct holds a list of rooms for client display.
 * (oooh, a tree!)
 */
struct roomlisting {
        struct roomlisting *lnext;
	struct roomlisting *rnext;
        char rlname[ROOMNAMELEN];
        unsigned rlflags;
	int rlfloor;
        int rlorder;
        };


