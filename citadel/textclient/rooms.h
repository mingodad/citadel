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
void dotungoto(CtdlIPC *ipc, char *towhere);
void whoknows(CtdlIPC *ipc);
void enterinfo(CtdlIPC *ipc);
void knrooms(CtdlIPC *ipc, int kn_floor_mode);
void dotknown(CtdlIPC *ipc, int what, char *match);
void load_floorlist(CtdlIPC *ipc);
void create_floor(CtdlIPC *ipc);
void edit_floor(CtdlIPC *ipc);
void kill_floor(CtdlIPC *ipc);
void enter_bio(CtdlIPC *ipc);
void hit_any_key(CtdlIPC *ipc);
int save_buffer(void *file, size_t filelen, const char *pathname);
void destination_directory(char *dest, const char *supplied_filename);
void do_edit(CtdlIPC *ipc,
		char *desc, char *read_cmd, char *check_cmd, char *write_cmd);



/* 
 * This struct holds a list of rooms for client display.
 * (oooh, a tree!)
 */
struct ctdlroomlisting {
        struct ctdlroomlisting *lnext;
	struct ctdlroomlisting *rnext;
        char rlname[ROOMNAMELEN];
        unsigned rlflags;
	int rlfloor;
        int rlorder;
        };


enum {
        LISTRMS_NEW_ONLY,
        LISTRMS_OLD_ONLY,
        LISTRMS_ALL
};


