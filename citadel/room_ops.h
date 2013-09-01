int is_known (struct ctdlroom *roombuf, int roomnum,
	      struct ctdluser *userbuf);
int has_newmsgs (struct ctdlroom *roombuf, int roomnum,
		 struct ctdluser *userbuf);
int is_zapped (struct ctdlroom *roombuf, int roomnum,
	       struct ctdluser *userbuf);
void b_putroom(struct ctdlroom *qrbuf, char *room_name);
void b_deleteroom(char *);
void lgetfloor (struct floor *flbuf, int floor_num);
void lputfloor (struct floor *flbuf, int floor_num);
int sort_msglist (long int *listptrs, int oldcount);
void list_roomname(struct ctdlroom *qrbuf, int ra, int current_view, int default_view);

void convert_room_name_macros(char *towhere, size_t maxlen);


typedef enum _POST_TYPE{
	POST_LOGGED_IN,
	POST_EXTERNAL,
	CHECK_EXISTANCE,
	POST_LMTP
}PostType;

int CtdlDoIHavePermissionToPostInThisRoom(char *errmsgbuf, 
	size_t n, 
	const char* RemoteIdentifier,
	PostType PostPublic,
	int is_reply
);
int CtdlDoIHavePermissionToDeleteMessagesFromThisRoom(void);
int CtdlDoIHavePermissionToReadMessagesInThisRoom(void);
