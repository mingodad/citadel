int is_known (struct quickroom *roombuf, int roomnum,
	      struct usersupp *userbuf);
int has_newmsgs (struct quickroom *roombuf, int roomnum,
		 struct usersupp *userbuf);
int is_zapped (struct quickroom *roombuf, int roomnum,
	       struct usersupp *userbuf);
int getroom(struct quickroom *qrbuf, char *room_name);
void putroom(struct quickroom *qrbuf, char *room_name);
int lgetroom(struct quickroom *qrbuf, char *room_name);
void lputroom(struct quickroom *qrbuf, char *room_name);
void getfloor (struct floor *flbuf, int floor_num);
void lgetfloor (struct floor *flbuf, int floor_num);
void putfloor (struct floor *flbuf, int floor_num);
void lputfloor (struct floor *flbuf, int floor_num);
void get_msglist (struct quickroom *whichroom);
void put_msglist (struct quickroom *whichroom);
long int MessageFromList (int whichpos);
void SetMessageInList (int whichpos, long int newmsgnum);
int sort_msglist (long int *listptrs, int oldcount);
void cmd_lrms (char *argbuf);
void cmd_lkra (char *argbuf);
void cmd_lkrn (char *argbuf);
void cmd_lkro (char *argbuf);
void cmd_lzrm (char *argbuf);
void usergoto (char *where, int display_result);
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
			int new_room_floor);
void cmd_cre8 (char *args);
void cmd_einf (char *ok);
void cmd_lflr (void);
void cmd_cflr (char *argbuf);
void cmd_kflr (char *argbuf);
void cmd_eflr (char *argbuf);
void ForEachRoom(void (*CallBack)(struct quickroom *EachRoom));
void assoc_file_name(char *buf, struct quickroom *qrbuf, char *prefix);
void delete_room(struct quickroom *qrbuf);
void GetExpirePolicy(struct ExpirePolicy *epbuf, struct quickroom *qrbuf);
