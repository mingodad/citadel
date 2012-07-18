
#include "webcit.h"

typedef struct UserStateStruct {
	StrBuf *UserName;
	StrBuf *Room;
	StrBuf *Host;
	StrBuf *RealRoom;
	StrBuf *RealHost;
	long LastActive;
	int Session;
	int Idle;
	int IdleSince;
	int SessionCount;
} UserStateStruct;

void DestroyUserStruct(void *vUser)
{
	UserStateStruct *User = (UserStateStruct*) vUser;
	FreeStrBuf(&User->UserName);
	FreeStrBuf(&User->Room);
	FreeStrBuf(&User->Host);
	FreeStrBuf(&User->RealRoom);
	FreeStrBuf(&User->RealHost);
	free(User);
}

int CompareUserStruct(const void *VUser1, const void *VUser2)
{
	const UserStateStruct *User1 = (UserStateStruct*) GetSearchPayload(VUser1);
	const UserStateStruct *User2 = (UserStateStruct*) GetSearchPayload(VUser2);
	
	if (User1->Idle != User2->Idle)
		return User1->Idle > User2->Idle;
	return strcasecmp(ChrPtr(User1->UserName), 
			  ChrPtr(User2->UserName));
}


int GetWholistSection(HashList *List, time_t now, StrBuf *Buf, const char *FilterName, long FNLen)
{
	wcsession *WCC = WC;
	UserStateStruct *User, *OldUser;
	void *VOldUser;
	size_t BufLen;
	const char *Pos;

	serv_puts("RWHO");
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) == 1) {
		while (BufLen = StrBuf_ServGetln(Buf), 
		       ((BufLen >= 0) && 
			((BufLen != 3) || strcmp(ChrPtr(Buf), "000"))))
		{
			if (BufLen <= 0)
			    continue;
			Pos = NULL;
			User = (UserStateStruct*) malloc(sizeof(UserStateStruct));
			User->Session = StrBufExtractNext_int(Buf, &Pos, '|');

			User->UserName = NewStrBufPlain(NULL, BufLen);
			StrBufExtract_NextToken(User->UserName, Buf, &Pos, '|');
			
			User->Room = NewStrBufPlain(NULL, BufLen);
			StrBufExtract_NextToken(User->Room, Buf, &Pos, '|');

			User->Host = NewStrBufPlain(NULL, BufLen);
			StrBufExtract_NextToken(User->Host, Buf, &Pos, '|');

			StrBufSkip_NTokenS(Buf, &Pos, '|', 1);

			User->LastActive = StrBufExtractNext_long(Buf, &Pos, '|');
			StrBufSkip_NTokenS(Buf, &Pos, '|', 3);

			User->RealRoom = NewStrBufPlain(NULL, BufLen);
			StrBufExtract_NextToken(User->RealRoom, Buf, &Pos, '|');

			User->RealHost = NewStrBufPlain(NULL, BufLen);
			StrBufExtract_NextToken(User->RealHost, Buf, &Pos, '|');
			
			User->Idle = (now - User->LastActive) > 900L;
			User->IdleSince = (now - User->LastActive) / 60;
			User->SessionCount = 1;

			if (FilterName == NULL) {
				if (GetHash(List, 
					    SKEY(User->UserName), 
					    &VOldUser)) {
					OldUser = VOldUser;
					OldUser->SessionCount++;
					if (!User->Idle) {
						if (User->Session == WCC->ctdl_pid) 
							OldUser->Session = User->Session;
						
						OldUser->Idle = User->Idle;
						OldUser->LastActive = User->LastActive;
					}
					DestroyUserStruct(User);
				}
				else
					Put(List, 
					    SKEY(User->UserName), 
					    User, DestroyUserStruct);
			}
			else {
				if (strcmp(FilterName, ChrPtr(User->UserName)) == 0)
				{
					Put(List, 
					    SKEY(User->UserName), 
					    User, DestroyUserStruct);
				}
				else 
				{
					DestroyUserStruct(User);
				}
			}
		}
		if (FilterName == NULL)
			SortByPayload(List, CompareUserStruct);
		return 1;
	}
	else {
		return 0;
	}
}

/*
 * end session
 */
void terminate_session(void)
{
	char buf[SIZ];

	serv_printf("TERM %s", bstr("which_session"));
	serv_getln(buf, sizeof buf);
	url_do_template();
}


/*
 * Change your session info (fake roomname and hostname)
 */
void edit_me(void)
{
	char buf[SIZ];

	output_headers(1, 0, 0, 0, 0, 0);
	if (havebstr("change_room_name_button")) {
		serv_printf("RCHG %s", bstr("fake_roomname"));
		serv_getln(buf, sizeof buf);
		do_template("who");
	} else if (havebstr("change_host_name_button")) {
		serv_printf("HCHG %s", bstr("fake_hostname"));
		serv_getln(buf, sizeof buf);
		do_template("who");
	} else if (havebstr("change_user_name_button")) {
		serv_printf("UCHG %s", bstr("fake_username"));
		serv_getln(buf, sizeof buf);
		do_template("who");
	} else if (havebstr("cancel_button")) {
		do_template("who");
	} else {
		do_template("who_edit");
	}
	end_burst();
}

void _terminate_session(void) {
	slrp_highest();
	terminate_session();
}

HashList *GetWholistHash(StrBuf *Target, WCTemplputParams *TP)

{
	const char *ch = NULL;
	int HashUniq = 1;
	long len;
	StrBuf *FilterNameStr = NULL;
	StrBuf *Buf;
	HashList *List;
        time_t now;

	Buf = NewStrBuf();

	serv_puts("TIME");
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL)  == 2) {
		const char *pos = ChrPtr(Buf) + 4;
		now = StrBufExtractNext_long(Buf, &pos, '|');
	}
	else {
		now = time(NULL);
	}
	if (HaveTemplateTokenString(NULL, TP, 2, &ch, &len))
	{
		FilterNameStr = NewStrBuf();
		GetTemplateTokenString(FilterNameStr, TP, 2, &ch, &len);
		HashUniq = 0;
	}

	List = NewHash(HashUniq, NULL);
	GetWholistSection(List, now, Buf, ch, len);
	FreeStrBuf(&Buf);
	FreeStrBuf(&FilterNameStr);
	return List;
}


void DeleteWholistHash(HashList **KillMe)
{
	DeleteHash(KillMe);
}

void tmplput_who_username(StrBuf *Target, WCTemplputParams *TP)
{
	UserStateStruct *User = (UserStateStruct*) CTX(CTX_WHO);
	StrBufAppendTemplate(Target, TP, User->UserName, 0);
}

void tmplput_who_room(StrBuf *Target, WCTemplputParams *TP)
{
	UserStateStruct *User = (UserStateStruct*) CTX(CTX_WHO);
	StrBufAppendTemplate(Target, TP, User->Room, 0);
}

void tmplput_who_host(StrBuf *Target, WCTemplputParams *TP)
{
	UserStateStruct *User = (UserStateStruct*) CTX(CTX_WHO);
	StrBufAppendTemplate(Target, TP, User->Host, 0);
}

void tmplput_who_realroom(StrBuf *Target, WCTemplputParams *TP)
{
	UserStateStruct *User = (UserStateStruct*) CTX(CTX_WHO);
	StrBufAppendTemplate(Target, TP, User->RealRoom, 0);
}
int conditional_who_realroom(StrBuf *Target, WCTemplputParams *TP)
{
	UserStateStruct *User = (UserStateStruct*) CTX(CTX_WHO);
	return StrLength(User->RealRoom) > 0;
}

void tmplput_who_realhost(StrBuf *Target, WCTemplputParams *TP)
{
	UserStateStruct *User = (UserStateStruct*) CTX(CTX_WHO);
	StrBufAppendTemplate(Target, TP, User->RealHost, 0);
}
int conditional_who_realhost(StrBuf *Target, WCTemplputParams *TP)
{
	UserStateStruct *User = (UserStateStruct*) CTX(CTX_WHO);
	return StrLength(User->RealHost) > 0;
}

void tmplput_who_lastactive(StrBuf *Target, WCTemplputParams *TP)
{
	UserStateStruct *User = (UserStateStruct*) CTX(CTX_WHO);
	StrBufAppendPrintf(Target, "%d", User->LastActive);
}

void tmplput_who_idlesince(StrBuf *Target, WCTemplputParams *TP)
{
	UserStateStruct *User = (UserStateStruct*) CTX(CTX_WHO);
	StrBufAppendPrintf(Target, "%d", User->IdleSince);
}

void tmplput_who_session(StrBuf *Target, WCTemplputParams *TP)
{
	UserStateStruct *User = (UserStateStruct*) CTX(CTX_WHO);
	StrBufAppendPrintf(Target, "%d", User->Session);
}

int conditional_who_idle(StrBuf *Target, WCTemplputParams *TP)
{
	UserStateStruct *User = (UserStateStruct*) CTX(CTX_WHO);
	return User->Idle;
}

int conditional_who_nsessions(StrBuf *Target, WCTemplputParams *TP)
{
	UserStateStruct *User = (UserStateStruct*) CTX(CTX_WHO);
	return User->SessionCount;
}

void tmplput_who_nsessions(StrBuf *Target, WCTemplputParams *TP)
{
	UserStateStruct *User = (UserStateStruct*) CTX(CTX_WHO);
	StrBufAppendPrintf(Target, "%d", User->SessionCount);
}

int conditional_who_isme(StrBuf *Target, WCTemplputParams *TP)
{
	UserStateStruct *User = (UserStateStruct*) CTX(CTX_WHO);
	return (User->Session == WC->ctdl_pid);
}

void 
InitModule_WHO
(void)
{
	

	WebcitAddUrlHandler(HKEY("terminate_session"), "", 0, _terminate_session, 0);
	WebcitAddUrlHandler(HKEY("edit_me"), "", 0, edit_me, 0);

	RegisterIterator("WHOLIST", 1, NULL, GetWholistHash, NULL, DeleteWholistHash, CTX_WHO, CTX_NONE, IT_NOFLAG);

	RegisterNamespace("WHO:NAME",        0, 1, tmplput_who_username, NULL, CTX_WHO);
	RegisterNamespace("WHO:ROOM",        0, 1, tmplput_who_room, NULL, CTX_WHO);
	RegisterNamespace("WHO:HOST",        0, 1, tmplput_who_host, NULL, CTX_WHO);
	RegisterNamespace("WHO:REALROOM",    0, 1, tmplput_who_realroom, NULL, CTX_WHO);
	RegisterNamespace("WHO:REALHOST",    0, 1, tmplput_who_realhost, NULL, CTX_WHO);
	RegisterNamespace("WHO:LASTACTIVE",  0, 1, tmplput_who_lastactive, NULL, CTX_WHO);
	RegisterNamespace("WHO:IDLESINCE",   0, 1, tmplput_who_idlesince, NULL, CTX_WHO);
	RegisterNamespace("WHO:SESSION",     0, 1, tmplput_who_session, NULL, CTX_WHO);
	RegisterNamespace("WHO:NSESSIONS",   0, 1, tmplput_who_nsessions, NULL, CTX_WHO);
	RegisterNamespace("WHO:NSESSIONS",   0, 1, tmplput_who_nsessions, NULL, CTX_WHO);

	RegisterConditional(HKEY("WHO:IDLE"),      1, conditional_who_idle, CTX_WHO);
	RegisterConditional(HKEY("WHO:NSESSIONS"), 1, conditional_who_nsessions, CTX_WHO);
	RegisterConditional(HKEY("WHO:ISME"),      1, conditional_who_isme, CTX_WHO);
	RegisterConditional(HKEY("WHO:REALROOM"),  1, conditional_who_realroom, CTX_WHO);
	RegisterConditional(HKEY("WHO:REALHOST"),  1, conditional_who_realhost, CTX_WHO);
}
