#include "webcit.h"
#include "webserver.h"

/*
 * Free a session's march list
 */
void free_march_list(wcsession *wcf)
{
	struct march *mptr;

	while (wcf->march != NULL) {
		mptr = wcf->march->next;
		free(wcf->march);
		wcf->march = mptr;
	}
}



/*
 * remove a room from the march list
 */
void remove_march(const StrBuf *aaa)
{
	struct march *mptr, *mptr2;

	if (WC->march == NULL)
		return;

	if (!strcasecmp(WC->march->march_name, ChrPtr(aaa))) {
		mptr = WC->march->next;
		free(WC->march);
		WC->march = mptr;
		return;
	}
	mptr2 = WC->march;
	for (mptr = WC->march; mptr != NULL; mptr = mptr->next) {
		if (!strcasecmp(mptr->march_name, ChrPtr(aaa))) {
			mptr2->next = mptr->next;
			free(mptr);
			mptr = mptr2;
		} else {
			mptr2 = mptr;
		}
	}
}



/**
 * \brief Locate the room on the march list which we most want to go to.  
 * Each room
 * is measured given a "weight" of preference based on various factors.
 * \param desired_floor the room number on the citadel server
 * \return the roomname
 */
char *pop_march(int desired_floor)
{
	static char TheRoom[128];
	int TheWeight = 0;
	int weight;
	struct march *mptr = NULL;

	strcpy(TheRoom, "_BASEROOM_");
	if (WC->march == NULL)
		return (TheRoom);

	for (mptr = WC->march; mptr != NULL; mptr = mptr->next) {
		weight = 0;
		if ((strcasecmp(mptr->march_name, "_BASEROOM_")))
			weight = weight + 10000;
		if (mptr->march_floor == desired_floor)
			weight = weight + 5000;

		weight = weight + ((128 - (mptr->march_floor)) * 128);
		weight = weight + (128 - (mptr->march_order));

		if (weight > TheWeight) {
			TheWeight = weight;
			strcpy(TheRoom, mptr->march_name);
/* TODOO: and now????
			TheFloor = mptr->march_floor;
			TheOrder = mptr->march_order;
*/
		}
	}
	return (TheRoom);
}



/*
 * Goto next room having unread messages.
 *
 * We want to skip over rooms that the user has already been to, and take the
 * user back to the lobby when done.  The room we end up in is placed in
 * newroom - which is set to 0 (the lobby) initially.
 * We start the search in the current room rather than the beginning to prevent
 * two or more concurrent users from dragging each other back to the same room.
 */
void gotonext(void)
{
	char buf[256];
	struct march *mptr = NULL;
	struct march *mptr2 = NULL;
	char room_name[128];
	StrBuf *next_room;
	int ELoop = 0;

	/*
	 * First check to see if the march-mode list is already allocated.
	 * If it is, pop the first room off the list and go there.
	 */
	if (havebstr("startmsg")) {
		readloop(readnew, eUseDefault);
		return;
	}

	if (WC->march == NULL) {
		serv_puts("LKRN");
		serv_getln(buf, sizeof buf);
		if (buf[0] == '1')
			while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
				if (IsEmptyStr(buf)) {
					if (ELoop > 10000)
						return;
					if (ELoop % 100 == 0)
						sleeeeeeeeeep(1);
					ELoop ++;
					continue;					
				}
				extract_token(room_name, buf, 0, '|', sizeof room_name);
				if (strcasecmp(room_name, ChrPtr(WC->CurRoom.name))) {
					mptr = (struct march *) malloc(sizeof(struct march));
					mptr->next = NULL;
					safestrncpy(mptr->march_name, room_name, sizeof mptr->march_name);
					mptr->march_floor = extract_int(buf, 2);
					mptr->march_order = extract_int(buf, 3);
					if (WC->march == NULL) 
						WC->march = mptr;
					else 
						mptr2->next = mptr;
					mptr2 = mptr;
				}
				buf[0] = '\0';
			}
		/*
		 * add _BASEROOM_ to the end of the march list, so the user will end up
		 * in the system base room (usually the Lobby>) at the end of the loop
		 */
		mptr = (struct march *) malloc(sizeof(struct march));
		mptr->next = NULL;
		mptr->march_order = 0;
	    	mptr->march_floor = 0;
		strcpy(mptr->march_name, "_BASEROOM_");
		if (WC->march == NULL) {
			WC->march = mptr;
		} else {
			mptr2 = WC->march;
			while (mptr2->next != NULL)
				mptr2 = mptr2->next;
			mptr2->next = mptr;
		}
		/*
		 * ...and remove the room we're currently in, so a <G>oto doesn't make us
		 * walk around in circles
		 */
		remove_march(WC->CurRoom.name);
	}
	if (WC->march != NULL) {
		next_room = NewStrBufPlain(pop_march(-1), -1);/*TODO: migrate march to strbuf */
		putlbstr("gotonext", 1);
	} else {
		next_room = NewStrBufPlain(HKEY("_BASEROOM_"));
	}


	smart_goto(next_room);
	FreeStrBuf(&next_room);
}

/*
 * un-goto the previous room
 */
void ungoto(void)
{
	StrBuf *Buf;

	if (havebstr("startmsg")) {
		readloop(readnew, eUseDefault);
		return;
	}

	if (!strcmp(WC->ugname, "")) {
		smart_goto(WC->CurRoom.name);
		return;
	}
	serv_printf("GOTO %s", WC->ugname);
	Buf = NewStrBuf();
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) != 2) {
		smart_goto(WC->CurRoom.name);
		FreeStrBuf(&Buf);
		return;
	}
	if (WC->uglsn >= 0L) {
		serv_printf("SLRP %ld", WC->uglsn);
		StrBuf_ServGetln(Buf);
	}
	FlushStrBuf(Buf);
	StrBufAppendBufPlain(Buf, WC->ugname, -1, 0);
	strcpy(WC->ugname, "");
	smart_goto(Buf);
	FreeStrBuf(&Buf);
}



void tmplput_ungoto(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;

	if ((WCC!=NULL) && 
	    (!IsEmptyStr(WCC->ugname)))
		StrBufAppendBufPlain(Target, WCC->ugname, -1, 0);
}

void _gotonext(void) {
	slrp_highest();
	gotonext();
}




int ConditionalHaveUngoto(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	
	return ((WCC!=NULL) && 
		(!IsEmptyStr(WCC->ugname)) && 
		(strcasecmp(WCC->ugname, ChrPtr(WCC->CurRoom.name)) == 0));
}


void 
InitModule_MARCHLIST
(void)
{
	RegisterConditional("COND:UNGOTO", 0, ConditionalHaveUngoto, CTX_NONE);
	RegisterNamespace("ROOM:UNGOTO", 0, 0, tmplput_ungoto, NULL, CTX_NONE);

	WebcitAddUrlHandler(HKEY("gotonext"), "", 0, _gotonext, NEED_URL);
	WebcitAddUrlHandler(HKEY("skip"), "", 0, gotonext, NEED_URL);
	WebcitAddUrlHandler(HKEY("ungoto"), "", 0, ungoto, NEED_URL);
}
