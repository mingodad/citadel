/* $Id$ */
void listzrooms(CtdlIPC *ipc);
void readinfo(CtdlIPC *ipc);
void forget(CtdlIPC *ipc);
void entroom(CtdlIPC *ipc);
void killroom(CtdlIPC *ipc);
void invite(CtdlIPC *ipc);
void kickout(CtdlIPC *ipc);
void editthisroom(CtdlIPC *ipc);
void roomdir(CtdlIPC *ipc);
void download(CtdlIPC *ipc, int proto);
void ungoto(CtdlIPC *ipc);
void whoknows(CtdlIPC *ipc);
void enterinfo(CtdlIPC *ipc);
void knrooms(CtdlIPC *ipc, int kn_floor_mode);
void load_floorlist(CtdlIPC *ipc);
void create_floor(CtdlIPC *ipc);
void edit_floor(CtdlIPC *ipc);
void kill_floor(CtdlIPC *ipc);
void enter_bio(CtdlIPC *ipc);
void download_to_local_disk(CtdlIPC *ipc, char *, long);
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


