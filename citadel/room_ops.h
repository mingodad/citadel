/* $Id$ */
int is_known (struct ctdlroom *roombuf, int roomnum,
	      struct ctdluser *userbuf);
int has_newmsgs (struct ctdlroom *roombuf, int roomnum,
		 struct ctdluser *userbuf);
int is_zapped (struct ctdlroom *roombuf, int roomnum,
	       struct ctdluser *userbuf);
int getroom(struct ctdlroom *qrbuf, char *room_name);
void b_putroom(struct ctdlroom *qrbuf, char *room_name);
void putroom(struct ctdlroom *);
void b_deleteroom(char *);
int lgetroom(struct ctdlroom *qrbuf, char *room_name);
void lputroom(struct ctdlroom *qrbuf);
void getfloor (struct floor *flbuf, int floor_num);
struct floor *cgetfloor(int floor_num);
void lgetfloor (struct floor *flbuf, int floor_num);
void putfloor (struct floor *flbuf, int floor_num);
void lputfloor (struct floor *flbuf, int floor_num);
int sort_msglist (long int *listptrs, int oldcount);
void cmd_lrms (char *argbuf);
void cmd_lkra (char *argbuf);
void cmd_lkrn (char *argbuf);
void cmd_lkro (char *argbuf);
void cmd_lzrm (char *argbuf);
void cmd_lprm (char *argbuf);
void usergoto (char *where, int display_result, int transiently,
			int *msgs, int *new);
void cmd_goto (char *gargs);
void cmd_whok (void);
void cmd_rdir (void);
void cmd_getr (void);
void cmd_setr (char *args);
void cmd_geta (void);
void cmd_seta (char *new_ra);
void cmd_rinf (void);
void cmd_kill (char *argbuf);
unsigned create_room(char *new_room_name,
			int new_room_type,
			char *new_room_pass,
			int new_room_floor,
			int really_create,
			int avoid_access,
			int new_room_view);
void cmd_cre8 (char *args);
void cmd_einf (char *ok);
void cmd_lflr (void);
void cmd_cflr (char *argbuf);
void cmd_kflr (char *argbuf);
void cmd_eflr (char *argbuf);
void ForEachRoom(void (*CallBack)(struct ctdlroom *EachRoom, void *out_data),
	void *in_data);
void schedule_room_for_deletion(struct ctdlroom *qrbuf);
void delete_room(struct ctdlroom *qrbuf);
void list_roomname(struct ctdlroom *qrbuf, int ra, int current_view, int default_view);
int is_noneditable(struct ctdlroom *qrbuf);
void CtdlRoomAccess(struct ctdlroom *roombuf, struct ctdluser *userbuf,
		int *result, int *view);
int CtdlDoIHavePermissionToDeleteThisRoom(struct ctdlroom *qr);

int CtdlRenameRoom(char *old_name, char *new_name, int new_floor);
void convert_room_name_macros(char *towhere, size_t maxlen);
/*
 * Possible return values for CtdlRenameRoom()
 */
enum {
	crr_ok,				/* success */
	crr_room_not_found,		/* room not found */
	crr_already_exists,		/* new name already exists */
	crr_noneditable,		/* cannot edit this room */
	crr_invalid_floor,		/* target floor does not exist */
	crr_access_denied		/* not allowed to edit this room */
};
