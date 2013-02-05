/*
 * Copyright (c) 1996-2012 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "webcit.h"
#include "webserver.h"

CtxType CTX_USERLIST = CTX_NONE;
/*
 *  show a list of available users to edit them
 *  message the header message???
 *  preselect = which user should be selected in the browser
 */
void select_user_to_edit(const char *preselect)
{
	output_headers(1, 0, 0, 0, 1, 0);
	do_template("aide_edituser_select");
        end_burst();
}


typedef struct _UserListEntry {
	int UID;
	int AccessLevel;
	int nLogons;
	int nPosts;

	StrBuf *UserName;
	StrBuf *Passvoid;
	time_t LastLogonT;
	/* Just available for Single users to view: */
	unsigned int Flags;
	int DaysTillPurge;
	int HasBio;
} UserListEntry;


UserListEntry* NewUserListOneEntry(StrBuf *SerializedUser, const char *Pos)
{
	UserListEntry *ul;

	if (StrLength(SerializedUser) < 8) 
		return NULL;

	ul = (UserListEntry*) malloc(sizeof(UserListEntry));
	ul->UserName = NewStrBuf();
	ul->Passvoid = NewStrBuf();

	StrBufExtract_NextToken(ul->UserName,               SerializedUser, &Pos, '|');
	StrBufExtract_NextToken(ul->Passvoid,               SerializedUser, &Pos, '|');
	ul->Flags         = StrBufExtractNext_unsigned_long(SerializedUser, &Pos, '|');
	ul->nLogons       = StrBufExtractNext_int(          SerializedUser, &Pos, '|');
	ul->nPosts        = StrBufExtractNext_int(          SerializedUser, &Pos, '|');
	ul->AccessLevel   = StrBufExtractNext_int(          SerializedUser, &Pos, '|');
	ul->UID           = StrBufExtractNext_int(          SerializedUser, &Pos, '|');
	ul->LastLogonT    = StrBufExtractNext_long(         SerializedUser, &Pos, '|');
	ul->DaysTillPurge = StrBufExtractNext_int(          SerializedUser, &Pos, '|');
	return ul;
}

void DeleteUserListEntry(void *vUserList)
{
	UserListEntry *ul = (UserListEntry*) vUserList;
	if (!ul) return;
	FreeStrBuf(&ul->UserName);
	FreeStrBuf(&ul->Passvoid);
	free(ul);
}

UserListEntry* NewUserListEntry(StrBuf *SerializedUserList)
{
	const char *Pos = NULL;
	UserListEntry *ul;

	if (StrLength(SerializedUserList) < 8) 
		return NULL;

	ul = (UserListEntry*) malloc(sizeof(UserListEntry));
	ul->UserName = NewStrBuf();
	ul->Passvoid = NewStrBuf();

	StrBufExtract_NextToken(ul->UserName,    SerializedUserList, &Pos, '|');
	ul->AccessLevel = StrBufExtractNext_int( SerializedUserList, &Pos, '|');
	ul->UID         = StrBufExtractNext_int( SerializedUserList, &Pos, '|');
	ul->LastLogonT  = StrBufExtractNext_long(SerializedUserList, &Pos, '|');
	ul->nLogons     = StrBufExtractNext_int( SerializedUserList, &Pos, '|');
	ul->nPosts      = StrBufExtractNext_int( SerializedUserList, &Pos, '|');
	StrBufExtract_NextToken(ul->Passvoid,    SerializedUserList, &Pos, '|');
	ul->Flags = 0;
	ul->HasBio = 0;
	ul->DaysTillPurge = -1;
	return ul;
}

/*
 * Sort by Username
 */
int CompareUserListName(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) GetSearchPayload(vUser1);
	UserListEntry *u2 = (UserListEntry*) GetSearchPayload(vUser2);

	return strcmp(ChrPtr(u1->UserName), ChrPtr(u2->UserName));
}

int CompareUserListNameRev(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) GetSearchPayload(vUser1);
	UserListEntry *u2 = (UserListEntry*) GetSearchPayload(vUser2);
	return strcmp(ChrPtr(u2->UserName), ChrPtr(u1->UserName));
}

int GroupchangeUserListName(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) vUser1;
	UserListEntry *u2 = (UserListEntry*) vUser2;
	return ChrPtr(u2->UserName)[0] != ChrPtr(u1->UserName)[0];
}

/*
 * Sort by access level
 */
int CompareAccessLevel(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) GetSearchPayload(vUser1);
	UserListEntry *u2 = (UserListEntry*) GetSearchPayload(vUser2);

	return (u1->AccessLevel > u2->AccessLevel);
}

int CompareAccessLevelRev(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) GetSearchPayload(vUser1);
	UserListEntry *u2 = (UserListEntry*) GetSearchPayload(vUser2);

	return (u2->AccessLevel > u1->AccessLevel);
}

int GroupchangeAccessLevel(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) vUser1;
	UserListEntry *u2 = (UserListEntry*) vUser2;

	return u2->AccessLevel != u1->AccessLevel;
}

/*
 * Sort by UID
 */
int CompareUID(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) GetSearchPayload(vUser1);
	UserListEntry *u2 = (UserListEntry*) GetSearchPayload(vUser2);

	return (u1->UID > u2->UID);
}

int CompareUIDRev(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) GetSearchPayload(vUser1);
	UserListEntry *u2 = (UserListEntry*) GetSearchPayload(vUser2);

	return (u2->UID > u1->UID);
}

int GroupchangeUID(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) vUser1;
	UserListEntry *u2 = (UserListEntry*) vUser2;

	return (u2->UID / 10) != (u1->UID / 10);
}

/*
 * Sort By Date /// TODO!
 */
int CompareLastLogon(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) GetSearchPayload(vUser1);
	UserListEntry *u2 = (UserListEntry*) GetSearchPayload(vUser2);

	return (u1->LastLogonT > u2->LastLogonT);
}

int CompareLastLogonRev(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) GetSearchPayload(vUser1);
	UserListEntry *u2 = (UserListEntry*) GetSearchPayload(vUser2);

	return (u2->LastLogonT > u1->LastLogonT);
}

int GroupchangeLastLogon(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) vUser1;
	UserListEntry *u2 = (UserListEntry*) vUser2;

	return (u2->LastLogonT != u1->LastLogonT);
}

/*
 * Sort By Number of Logons
 */
int ComparenLogons(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) GetSearchPayload(vUser1);
	UserListEntry *u2 = (UserListEntry*) GetSearchPayload(vUser2);

	return (u1->nLogons > u2->nLogons);
}

int ComparenLogonsRev(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) GetSearchPayload(vUser1);
	UserListEntry *u2 = (UserListEntry*) GetSearchPayload(vUser2);

	return (u2->nLogons > u1->nLogons);
}

int GroupchangenLogons(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) vUser1;
	UserListEntry *u2 = (UserListEntry*) vUser2;

	return (u2->nLogons / 100) != (u1->nLogons / 100);
}

/*
 * Sort By Number of Posts
 */
int ComparenPosts(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) GetSearchPayload(vUser1);
	UserListEntry *u2 = (UserListEntry*) GetSearchPayload(vUser2);

	return (u1->nPosts > u2->nPosts);
}

int ComparenPostsRev(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) GetSearchPayload(vUser1);
	UserListEntry *u2 = (UserListEntry*) GetSearchPayload(vUser2);

	return (u2->nPosts > u1->nPosts);
}

int GroupchangenPosts(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) vUser1;
	UserListEntry *u2 = (UserListEntry*) vUser2;

	return (u2->nPosts / 100) != (u1->nPosts / 100);
}


HashList *iterate_load_userlist(StrBuf *Target, WCTemplputParams *TP)
{
	int Done = 0;
	CompareFunc SortIt;
	HashList *Hash = NULL;
	StrBuf *Buf;
	UserListEntry* ul;
	int len;
	int UID;
	void *vData;
	WCTemplputParams SubTP;

	memset(&SubTP, 0, sizeof(WCTemplputParams));	
        serv_puts("LIST");
	Buf = NewStrBuf();
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) == 1) {
		Hash = NewHash(1, Flathash);

		while (!Done) {
			len = StrBuf_ServGetln(Buf);
			if ((len <0) || 
			    ((len == 3) &&
			     !strcmp(ChrPtr(Buf), "000")))
			{
				Done = 1;
				break;
			}
			ul = NewUserListEntry(Buf);
			if (ul == NULL)
				continue;

			Put(Hash, IKEY(ul->UID), ul, DeleteUserListEntry); 
		}

		serv_puts("LBIO 1");
		StrBuf_ServGetln(Buf);
		if (GetServerStatus(Buf, NULL) == 1)
			Done = 0;
			while (!Done) {
			len = StrBuf_ServGetln(Buf);
			if ((len <0) || 
			    ((len == 3) &&
			     !strcmp(ChrPtr(Buf), "000")))
			{
				Done = 1;
				break;
			}
			UID = atoi(ChrPtr(Buf));
			if (GetHash(Hash, IKEY(UID), &vData) && vData != 0)
			{
				ul = (UserListEntry*)vData;
				ul->HasBio = 1;
			}
		}
		SubTP.Filter.ContextType = CTX_USERLIST;
		SortIt = RetrieveSort(&SubTP, HKEY("USER"), HKEY("user:uid"), 0);
		if (SortIt != NULL)
			SortByPayload(Hash, SortIt);
		else 
			SortByPayload(Hash, CompareUID);
        }
	FreeStrBuf(&Buf);
	return Hash;
}


void tmplput_USERLIST_UserName(StrBuf *Target, WCTemplputParams *TP)
{
	UserListEntry *ul = (UserListEntry*) CTX(CTX_USERLIST);
	StrBufAppendTemplate(Target, TP, ul->UserName, 0);
}

void tmplput_USERLIST_Password(StrBuf *Target, WCTemplputParams *TP)
{
	UserListEntry *ul = (UserListEntry*) CTX(CTX_USERLIST);
	StrBufAppendTemplate(Target, TP, ul->Passvoid, 0);
}

void tmplput_USERLIST_AccessLevelNo(StrBuf *Target, WCTemplputParams *TP)
{
	UserListEntry *ul = (UserListEntry*) CTX(CTX_USERLIST);

	StrBufAppendPrintf(Target, "%d", ul->AccessLevel, 0);
}

void tmplput_USERLIST_AccessLevelStr(StrBuf *Target, WCTemplputParams *TP)
{
	UserListEntry *ul = (UserListEntry*) CTX(CTX_USERLIST);
	
	StrBufAppendBufPlain(Target, _(axdefs[ul->AccessLevel]), -1, 0);
}

void tmplput_USERLIST_UID(StrBuf *Target, WCTemplputParams *TP)
{
	UserListEntry *ul = (UserListEntry*) CTX(CTX_USERLIST);

	StrBufAppendPrintf(Target, "%d", ul->UID, 0);
}

void tmplput_USERLIST_LastLogonNo(StrBuf *Target, WCTemplputParams *TP)
{
	UserListEntry *ul = (UserListEntry*) CTX(CTX_USERLIST);

	StrBufAppendPrintf(Target,"%ld", ul->LastLogonT, 0);
}
void tmplput_USERLIST_LastLogonStr(StrBuf *Target, WCTemplputParams *TP)
{
	UserListEntry *ul = (UserListEntry*) CTX(CTX_USERLIST);
	StrEscAppend(Target, NULL, asctime(localtime(&ul->LastLogonT)), 0, 0);
}

void tmplput_USERLIST_nLogons(StrBuf *Target, WCTemplputParams *TP)
{
	UserListEntry *ul = (UserListEntry*) CTX(CTX_USERLIST);

	StrBufAppendPrintf(Target, "%d", ul->nLogons, 0);
}

void tmplput_USERLIST_nPosts(StrBuf *Target, WCTemplputParams *TP)
{
	UserListEntry *ul = (UserListEntry*) CTX(CTX_USERLIST);

	StrBufAppendPrintf(Target, "%d", ul->nPosts, 0);
}

void tmplput_USERLIST_Flags(StrBuf *Target, WCTemplputParams *TP)
{
	UserListEntry *ul = (UserListEntry*) CTX(CTX_USERLIST);

	StrBufAppendPrintf(Target, "%d", ul->Flags, 0);
}

void tmplput_USERLIST_DaysTillPurge(StrBuf *Target, WCTemplputParams *TP)
{
	UserListEntry *ul = (UserListEntry*) CTX(CTX_USERLIST);

	StrBufAppendPrintf(Target, "%d", ul->DaysTillPurge, 0);
}

int ConditionalUser(StrBuf *Target, WCTemplputParams *TP)
{
	UserListEntry *ul = (UserListEntry*) CTX(CTX_USERLIST);
	if (havebstr("usernum")) {
		return ibstr("usernum") == ul->UID;
	}
	else if (havebstr("username")) {
		return strcmp(bstr("username"), ChrPtr(ul->UserName)) == 0;
	}
	else 
		return 0;
}

int ConditionalFlagINetEmail(StrBuf *Target, WCTemplputParams *TP)
{
	UserListEntry *ul = (UserListEntry*) CTX(CTX_USERLIST);
	return (ul->Flags & US_INTERNET) != 0;
}

int ConditionalUserAccess(StrBuf *Target, WCTemplputParams *TP)
{
	UserListEntry *ul = (UserListEntry*) CTX(CTX_USERLIST);
	
	if (ul == NULL)
		return 0;

	return GetTemplateTokenNumber(Target, 
				      TP, 
				      3, 
				      AxNewU)
		==
		ul->AccessLevel;
}
int ConditionalHaveBIO(StrBuf *Target, WCTemplputParams *TP)
{
	UserListEntry *ul = (UserListEntry*) CTX(CTX_USERLIST);
	
	if (ul == NULL)
		return 0;
	return ul->HasBio;
}

void tmplput_USER_BIO(StrBuf *Target, WCTemplputParams *TP)
{
	int Done = 0;
	StrBuf *Buf;
	const char *who;
	long len;

	GetTemplateTokenString(Target, TP, 0, &who, &len);

	Buf = NewStrBuf();
	serv_printf("RBIO %s", who);
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) == 1) {
		StrBuf *BioBuf = NewStrBufPlain(NULL, SIZ);
		while (!Done && StrBuf_ServGetln(Buf)>=0) {
			if ( (StrLength(Buf)==3) && 
			     !strcmp(ChrPtr(Buf), "000")) 
				Done = 1;
			else {
				StrBufAppendBuf(BioBuf, Buf, 0);
				StrBufAppendBufPlain(BioBuf, HKEY("\n"), 0);
			}
		}
		StrBufAppendTemplate(Target, TP, BioBuf, 1);
		FreeStrBuf(&BioBuf);
	}
	FreeStrBuf(&Buf);
}

int Conditional_USER_HAS_PIC(StrBuf *Target, WCTemplputParams *TP)
{
	StrBuf *Buf;
	const char *who;
	long len;
	int r = 0;

	GetTemplateTokenString(Target, TP, 2, &who, &len);

	Buf = NewStrBuf();
	serv_printf("OIMG _userpic_|%s", who);
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) != 2) {
		r = 1;
	}
	else {
		r = 0;
	}
	serv_puts("CLOS");
	StrBuf_ServGetln(Buf);
	GetServerStatus(Buf, NULL);
	FreeStrBuf(&Buf);
	return(r);
}


/*
 *  Locate the message number of a user's vCard in the current room
 *  Returns the message id of his vcard
 */
long locate_user_vcard_in_this_room(message_summary **VCMsg, wc_mime_attachment **VCAtt)
{
	wcsession *WCC = WC;
	HashPos *at;
	HashPos *att;
	const char *HashKey;
	long HKLen;
	void *vMsg;
	message_summary *Msg;
	wc_mime_attachment *Att;
	StrBuf *Buf;
	long vcard_msgnum = (-1L);
	int already_tried_creating_one = 0;
	StrBuf *FoundCharset = NewStrBuf();
	StrBuf *Error = NULL;
	SharedMessageStatus Stat;


	Buf = NewStrBuf();
TRYAGAIN:
	memset(&Stat, 0, sizeof(SharedMessageStatus));
	Stat.maxload = 10000;
	Stat.lowest_found = (-1);
	Stat.highest_found = (-1);
	/* Search for the user's vCard */
	if (load_msg_ptrs("MSGS ALL||||1", NULL, &Stat, NULL) > 0) {
		at = GetNewHashPos(WCC->summ, 0);
		while (GetNextHashPos(WCC->summ, at, &HKLen, &HashKey, &vMsg)) {
			Msg = (message_summary*) vMsg;		
			Msg->MsgBody =  (wc_mime_attachment*) malloc(sizeof(wc_mime_attachment));
			memset(Msg->MsgBody, 0, sizeof(wc_mime_attachment));
			Msg->MsgBody->msgnum = Msg->msgnum;

			load_message(Msg, FoundCharset, &Error);
			
			if (Msg->AllAttach != NULL) {
				att = GetNewHashPos(Msg->AllAttach, 0);
				while (GetNextHashPos(Msg->AllAttach, att, &HKLen, &HashKey, &vMsg) && 
				       (vcard_msgnum == -1)) {
					Att = (wc_mime_attachment*) vMsg;
					if (
						(strcasecmp(ChrPtr(Att->ContentType), "text/x-vcard") == 0)
						|| (strcasecmp(ChrPtr(Att->ContentType), "text/vcard") == 0)
					) {
						*VCAtt = Att;
						*VCMsg = Msg;
						vcard_msgnum = Msg->msgnum;
						if (Att->Data == NULL) {
							MimeLoadData(Att);
						}
					}
				}
				DeleteHashPos(&att);
			}
			FreeStrBuf(&Error);	/* don't care... */
			
		}
		DeleteHashPos(&at);		
	}

	/* If there's no vcard, create one */
	if ((*VCMsg == NULL) && (already_tried_creating_one == 0)) {
		FlushStrBuf(Buf);
		already_tried_creating_one = 1;
		serv_puts("ENT0 1|||4");
		StrBuf_ServGetln(Buf);
		if (GetServerStatus(Buf, NULL) == 4) {
			serv_puts("Content-type: text/x-vcard");
			serv_puts("");
			serv_puts("begin:vcard");
			serv_puts("end:vcard");
			serv_puts("000");
		}
		else 
			syslog(LOG_WARNING, "Error while creating user vcard: %s\n", ChrPtr(Buf));
		goto TRYAGAIN;
	}
	FreeStrBuf(&Buf);
	FreeStrBuf(&FoundCharset);

	return(vcard_msgnum);
}


/*
 *  Display the form for editing a user's address book entry
 *  username the name of the user
 *  usernum the citadel-uid of the user
 */
void display_edit_address_book_entry(const char *username, long usernum) {
	message_summary *VCMsg = NULL;
	wc_mime_attachment *VCAtt = NULL;
	StrBuf *roomname;
	StrBuf *Buf;
	long vcard_msgnum = (-1L);

	/* Locate the user's config room, creating it if necessary */
	Buf = NewStrBuf();
	roomname = NewStrBuf();
	StrBufPrintf(roomname, "%010ld.%s", usernum, USERCONFIGROOM);
	serv_printf("GOTO %s||1", ChrPtr(roomname));
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) != 2) {
		serv_printf("CRE8 1|%s|5|||1|", ChrPtr(roomname));
		StrBuf_ServGetln(Buf);
		GetServerStatus(Buf, NULL);
		serv_printf("GOTO %s||1", ChrPtr(roomname));
		StrBuf_ServGetln(Buf);
		if (GetServerStatusMsg(Buf, NULL, 1, 2) != 2) {
			select_user_to_edit(username);
			FreeStrBuf(&Buf);
			FreeStrBuf(&roomname);
			return;
		}
	}
	FreeStrBuf(&Buf);

	locate_user_vcard_in_this_room(&VCMsg, &VCAtt);

	if (VCMsg == NULL) {
		AppendImportantMessage(_("An error occurred while trying to create or edit this address book entry."), -1);
		select_user_to_edit(username);
		FreeStrBuf(&roomname);
		return;
	}

	do_edit_vcard(vcard_msgnum, "1", 
		      VCMsg,
		      VCAtt,
		      "select_user_to_edit", 
		      ChrPtr(roomname));
	FreeStrBuf(&roomname);
}

/*
 *  burge a user 
 *  username the name of the user to remove
 */
void delete_user(char *username) {
	StrBuf *Buf;
	
	Buf = NewStrBuf();
	serv_printf("ASUP %s|0|0|0|0|0|", username);
	StrBuf_ServGetln(Buf);
	GetServerStatusMsg(Buf, NULL, 1, 2);

	select_user_to_edit( bstr("username"));
	FreeStrBuf(&Buf);
}
		

void display_edituser(const char *supplied_username, int is_new) {
	const char *Pos;
	UserListEntry* UL;
	StrBuf *Buf;
	char username[256];

	if (supplied_username != NULL) {
		safestrncpy(username, supplied_username, sizeof username);
	}
	else {
		safestrncpy(username, bstr("username"), sizeof username);
	}

	Buf = NewStrBuf();
	serv_printf("AGUP %s", username);
	StrBuf_ServGetln(Buf);
	if (GetServerStatusMsg(Buf, NULL, 1, 2) != 2) {
		select_user_to_edit(username);
		FreeStrBuf(&Buf);
		return;
	}
	else {
		Pos = ChrPtr(Buf) + 4;
		UL = NewUserListOneEntry(Buf, Pos);
		if ((UL != NULL) && havebstr("edit_abe_button")) {
			display_edit_address_book_entry(username, UL->UID);
		}
		else if ((UL != NULL) && havebstr("delete_button")) {
			delete_user(username);
		}
		else if (UL != NULL) {
			WCTemplputParams SubTP;
			memset(&SubTP, 0, sizeof(WCTemplputParams));
			SubTP.Filter.ContextType = CTX_USERLIST;
			SubTP.Context = UL;
			output_headers(1, 0, 0, 0, 1, 0);
			DoTemplate(HKEY("aide_edituser_detailview"), NULL, &SubTP);
			end_burst();
		}
		DeleteUserListEntry(UL);
		
	}
	FreeStrBuf(&Buf);
}

/*
 *  do the backend operation of the user edit on the server
 */
void edituser(void) {
	int is_new = 0;
	unsigned int flags = 0;
	const char *username;

	is_new = ibstr("is_new");
	username = bstr("username");

	if (!havebstr("ok_button")) {
		AppendImportantMessage(_("Changes were not saved."), -1);
	}	
	else {
		StrBuf *Buf = NewStrBuf();

		flags = ibstr("flags");
		if (yesbstr("inetmail")) {
			flags |= US_INTERNET;
		}
		else {
			flags &= ~US_INTERNET ;
		}

		if ((havebstr("newname")) && (strcasecmp(bstr("username"), bstr("newname")))) {
			serv_printf("RENU %s|%s", bstr("username"), bstr("newname"));
			StrBuf_ServGetln(Buf);
			if (GetServerStatusMsg(Buf, NULL, 1, 2) != 2) {
				username = bstr("newname");
			}
		}

		serv_printf("ASUP %s|%s|%d|%s|%s|%s|%s|%s|%s|",
			username,
			bstr("password"),
			flags,
			bstr("timescalled"),
			bstr("msgsposted"),
			bstr("axlevel"),
			bstr("usernum"),
			bstr("lastcall"),
			bstr("purgedays")
		);
		StrBuf_ServGetln(Buf);
		GetServerStatusMsg(Buf, NULL, 1, 2);
		FreeStrBuf(&Buf);
	}

	/*
	 * If we are in the middle of creating a new user, move on to
	 * the vCard edit screen.
	 */
	if (is_new) {
		display_edit_address_book_entry(username, lbstr("usernum") );
	}
	else {
		select_user_to_edit(username);
	}
}



/*
 *  create a new user
 * take the web environment username and create it on the citadel server
 */
void create_user(void) {
	long FullState;
	StrBuf *Buf;
	const char *username;

	Buf = NewStrBuf();
	username = bstr("username");
	serv_printf("CREU %s", username);
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, &FullState) == 2) {
		AppendImportantMessage(_("A new user has been created."), -1);
		display_edituser(username, 1);
	}
	else if (FullState == 570) {
		AppendImportantMessage(_("You are attempting to create a new user from within Citadel "
					 "while running in host based authentication mode.  In this mode, "
					 "you must create new users on the host system, not within Citadel."), 
				       -1);
		select_user_to_edit(NULL);
	}
	else {
		AppendImportantMessage(ChrPtr(Buf) + 4, StrLength(Buf) - 4);
		select_user_to_edit(NULL);
	}
	FreeStrBuf(&Buf);
}


void _select_user_to_edit(void) {
	select_user_to_edit(NULL);
}


void _display_edituser(void) {
	display_edituser(NULL, 0);
}

void 
InitModule_USEREDIT
(void)
{
	RegisterCTX(CTX_USERLIST);
	WebcitAddUrlHandler(HKEY("select_user_to_edit"), "", 0, _select_user_to_edit, 0);
	WebcitAddUrlHandler(HKEY("display_edituser"), "", 0, _display_edituser, 0);
	WebcitAddUrlHandler(HKEY("edituser"), "", 0, edituser, 0);
	WebcitAddUrlHandler(HKEY("create_user"), "", 0, create_user, 0);

	RegisterNamespace("USERLIST:USERNAME",      0, 1, tmplput_USERLIST_UserName, NULL, CTX_USERLIST);
	RegisterNamespace("USERLIST:PASSWD",        0, 1, tmplput_USERLIST_Password, NULL, CTX_USERLIST);
	RegisterNamespace("USERLIST:ACCLVLNO",      0, 0, tmplput_USERLIST_AccessLevelNo, NULL, CTX_USERLIST);
	RegisterNamespace("USERLIST:ACCLVLSTR",     0, 0, tmplput_USERLIST_AccessLevelStr, NULL, CTX_USERLIST);
	RegisterNamespace("USERLIST:UID",           0, 0, tmplput_USERLIST_UID, NULL, CTX_USERLIST);
	RegisterNamespace("USERLIST:LASTLOGON:STR", 0, 0, tmplput_USERLIST_LastLogonStr, NULL, CTX_USERLIST);
	RegisterNamespace("USERLIST:LASTLOGON:NO",  0, 0, tmplput_USERLIST_LastLogonNo, NULL, CTX_USERLIST);
	RegisterNamespace("USERLIST:NLOGONS",       0, 0, tmplput_USERLIST_nLogons, NULL, CTX_USERLIST);
	RegisterNamespace("USERLIST:NPOSTS",        0, 0, tmplput_USERLIST_nPosts, NULL, CTX_USERLIST);
						    
	RegisterNamespace("USERLIST:FLAGS",         0, 0, tmplput_USERLIST_Flags, NULL, CTX_USERLIST);
	RegisterNamespace("USERLIST:DAYSTILLPURGE", 0, 0, tmplput_USERLIST_DaysTillPurge, NULL, CTX_USERLIST);

	RegisterNamespace("USER:BIO", 1, 2, tmplput_USER_BIO,  NULL, CTX_NONE);

	RegisterConditional("COND:USERNAME",  0,    ConditionalUser, CTX_USERLIST);
	RegisterConditional("COND:USERACCESS", 0,   ConditionalUserAccess, CTX_USERLIST);
	RegisterConditional("COND:USERLIST:FLAG:USE_INTERNET", 0, ConditionalFlagINetEmail, CTX_USERLIST);
	RegisterConditional("COND:USERLIST:HAVEBIO", 0, ConditionalHaveBIO, CTX_USERLIST);

	RegisterConditional("COND:USER:PIC", 1, Conditional_USER_HAS_PIC,  CTX_NONE);

	RegisterIterator("USERLIST", 0, NULL, iterate_load_userlist, NULL, DeleteHash, CTX_USERLIST, CTX_NONE, IT_FLAG_DETECT_GROUPCHANGE);
	


	RegisterSortFunc(HKEY("user:name"),
			 HKEY("userlist"),
			 CompareUserListName,
			 CompareUserListNameRev,
			 GroupchangeUserListName,
			 CTX_USERLIST);
	RegisterSortFunc(HKEY("user:accslvl"),
			 HKEY("userlist"),
			 CompareAccessLevel,
			 CompareAccessLevelRev,
			 GroupchangeAccessLevel,
			 CTX_USERLIST);

	RegisterSortFunc(HKEY("user:nlogons"),
			 HKEY("userlist"),
			 ComparenLogons,
			 ComparenLogonsRev,
			 GroupchangenLogons,
			 CTX_USERLIST);

	RegisterSortFunc(HKEY("user:uid"),
			 HKEY("userlist"),
			 CompareUID,
			 CompareUIDRev,
			 GroupchangeUID,
			 CTX_USERLIST);

	RegisterSortFunc(HKEY("user:lastlogon"),
			 HKEY("userlist"),
			 CompareLastLogon,
			 CompareLastLogonRev,
			 GroupchangeLastLogon,
			 CTX_USERLIST);

	RegisterSortFunc(HKEY("user:nmsgposts"),
			 HKEY("userlist"),
			 ComparenPosts,
			 ComparenPostsRev,
			 GroupchangenPosts,
			 CTX_USERLIST);

	REGISTERTokenParamDefine(AxDeleted);
	REGISTERTokenParamDefine(AxNewU);
	REGISTERTokenParamDefine(AxProbU);
	REGISTERTokenParamDefine(AxLocU);
	REGISTERTokenParamDefine(AxNetU);
	REGISTERTokenParamDefine(AxPrefU);
	REGISTERTokenParamDefine(AxAideU);
}

