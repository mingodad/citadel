/* $Id$ */
int is_known (struct quickroom *roombuf, int roomnum,
	      struct usersupp *userbuf);
int has_newmsgs (struct quickroom *roombuf, int roomnum,
		 struct usersupp *userbuf);
int is_zapped (struct quickroom *roombuf, int roomnum,
	       struct usersupp *userbuf);
int getroom(struct quickroom *qrbuf, char *room_name);
void b_putroom(struct quickroom *qrbuf, char *room_name);
void putroom(struct quickroom *);
void b_deleteroom(char *);
int lgetroom(struct quickroom *qrbuf, char *room_name);
void lputroom(struct quickroom *qrbuf);
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
void usergoto (char *where, int display_result, int *msgs, int *new);
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
			int really_create);
void cmd_cre8 (char *args);
void cmd_einf (char *ok);
void cmd_lflr (void);
void cmd_cflr (char *argbuf);
void cmd_kflr (char *argbuf);
void cmd_eflr (char *argbuf);
void ForEachRoom(void (*CallBack)(struct quickroom *EachRoom, void *out_data),
	void *in_data);
void assoc_file_name(char *buf, size_t n,
		     struct quickroom *qrbuf, const char *prefix);
void delete_room(struct quickroom *qrbuf);
void list_roomname(struct quickroom *qrbuf);
int is_noneditable(struct quickroom *qrbuf);
int CtdlRoomAccess(struct quickroom *roombuf, struct usersupp *userbuf);
int CtdlDoIHavePermissionToDeleteThisRoom(struct quickroom *qr);

int CtdlRenameRoom(char *old_name, char *new_name, int new_floor);
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
