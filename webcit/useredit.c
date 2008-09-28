/*
 * $Id$
 */
/**
 * \defgroup AdminTasks Administrative screen to add/change/delete user accounts
 * \ingroup CitadelConfig
 *
 */
/*@{*/

#include "webcit.h"
#include "webserver.h"


/**
 * \brief show a list of available users to edit them
 * \param message the header message???
 * \param preselect which user should be selected in the browser
 */
void select_user_to_edit(char *message, char *preselect)
{/*
	char buf[SIZ];
	char username[SIZ];
 */
	output_headers(1, 0, 0, 0, 1, 0);
	do_template("edituser_select", NULL);
        end_burst();

/*

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<img src=\"static/usermanag_48x.gif\">");
        wprintf("<h1>");
	wprintf(_("Edit or delete users"));
        wprintf("</h1>");
        wprintf("</div>");

        wprintf("<div id=\"content\" class=\"service\">\n");

	if (message != NULL) wprintf(message);

	wprintf("<table border=0 cellspacing=10><tr valign=top><td>\n");

	svput("BOXTITLE", WCS_STRING, _("Add users"));
	do_template("beginbox", NULL);

	wprintf(_("To create a new user account, enter the desired "
		"user name in the box below and click 'Create'."));
	wprintf("<br /><br />");

        wprintf("<center><form method=\"POST\" action=\"create_user\">\n");
	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
        wprintf(_("New user: "));
        wprintf("<input type=\"text\" name=\"username\"><br />\n"
        	"<input type=\"submit\" name=\"create_button\" value=\"%s\">"
		"</form></center>\n", _("Create"));

	do_template("endbox", NULL);

	wprintf("</td><td>");

	svput("BOXTITLE", WCS_STRING, _("Edit or Delete users"));
	do_template("beginbox", NULL);

	wprintf(_("To edit an existing user account, select the user "
		"name from the list and click 'Edit'."));
	wprintf("<br /><br />");
	
        wprintf("<center>"
		"<form method=\"POST\" action=\"display_edituser\">\n");
	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
        wprintf("<select name=\"username\" size=10 style=\"width:100%%\">\n");
        serv_puts("LIST");
        serv_getln(buf, sizeof buf);
        if (buf[0] == '1') {
                while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
                        extract_token(username, buf, 0, '|', sizeof username);
                        wprintf("<option");
			if (preselect != NULL)
			   if (!strcasecmp(username, preselect))
			      wprintf(" selected");
			wprintf(">");
                        escputs(username);
                        wprintf("\n");
                }
        }
        wprintf("</select><br />\n");

        wprintf("<input type=\"submit\" name=\"edit_config_button\" value=\"%s\">", _("Edit configuration"));
        wprintf("<input type=\"submit\" name=\"edit_abe_button\" value=\"%s\">", _("Edit address book entry"));
        wprintf("<input type=\"submit\" name=\"delete_button\" value=\"%s\" "
		"onClick=\"return confirm('%s');\">", _("Delete user"), _("Delete this user?"));
        wprintf("</form></center>\n");
	do_template("endbox", NULL);

	wprintf("</td></tr></table>\n");

	wDumpContent(1);
*/
}


typedef struct _UserListEntry {
	int UID;
	int AccessLevel;
	int nLogons;
	int nPosts;

	StrBuf *UserName;
	StrBuf *Passvoid;
	StrBuf *LastLogon;
	time_t LastLogonT;
	/* Just available for Single users to view: */
	unsigned int Flags;
	int DaysTillPurge;
} UserListEntry;


UserListEntry* NewUserListOneEntry(StrBuf *SerializedUser)
{
	UserListEntry *ul;

	if (StrLength(SerializedUser) < 8) 
		return NULL;

	ul = (UserListEntry*) malloc(sizeof(UserListEntry));
	ul->UserName = NewStrBuf();
	ul->LastLogon = NewStrBuf();
	ul->Passvoid = NewStrBuf();

	StrBufExtract_token(ul->UserName, SerializedUser, 0, '|');
	StrBufExtract_token(ul->Passvoid, SerializedUser, 1, '|');
	ul->Flags = (unsigned int)StrBufExtract_long(SerializedUser, 2, '|');
	ul->nLogons = StrBufExtract_int(SerializedUser, 3, '|');
	ul->nPosts = StrBufExtract_int(SerializedUser, 4, '|');
	ul->AccessLevel = StrBufExtract_int(SerializedUser, 5, '|');
	ul->UID = StrBufExtract_int(SerializedUser, 6, '|');
	StrBufExtract_token(ul->LastLogon, SerializedUser, 7, '|');
	/// TODO: ul->LastLogon -> ulLastLogonT
	ul->DaysTillPurge = StrBufExtract_int(SerializedUser, 8, '|');
	return ul;
}

void DeleteUserListEntry(void *vUserList)
{
	UserListEntry *ul = (UserListEntry*) vUserList;
	FreeStrBuf(&ul->UserName);
	FreeStrBuf(&ul->LastLogon);
	FreeStrBuf(&ul->Passvoid);
	free(ul);
}

UserListEntry* NewUserListEntry(StrBuf *SerializedUserList)
{
	UserListEntry *ul;

	if (StrLength(SerializedUserList) < 8) 
		return NULL;

	ul = (UserListEntry*) malloc(sizeof(UserListEntry));
	ul->UserName = NewStrBuf();
	ul->LastLogon = NewStrBuf();
	ul->Passvoid = NewStrBuf();

	StrBufExtract_token(ul->UserName, SerializedUserList, 0, '|');
	ul->AccessLevel = StrBufExtract_int(SerializedUserList, 1, '|');
	ul->UID = StrBufExtract_int(SerializedUserList, 2, '|');
	StrBufExtract_token(ul->LastLogon, SerializedUserList, 3, '|');
	/// TODO: ul->LastLogon -> ulLastLogonT
	ul->nLogons = StrBufExtract_int(SerializedUserList, 4, '|');
	ul->nPosts = StrBufExtract_int(SerializedUserList, 5, '|');
	StrBufExtract_token(ul->Passvoid, SerializedUserList, 6, '|');
	ul->Flags = 0;
	ul->DaysTillPurge = -1;
	return ul;
}

/*
 * Sort by Username
 */
int CompareUserListName(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) vUser1;
	UserListEntry *u2 = (UserListEntry*) vUser2;

	return strcmp(ChrPtr(u1->UserName), ChrPtr(u2->UserName));
}
int CompareUserListNameRev(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) vUser1;
	UserListEntry *u2 = (UserListEntry*) vUser2;
	return strcmp(ChrPtr(u2->UserName), ChrPtr(u1->UserName));
}

/*
 * Sort by AccessLevel
 */
int CompareAccessLevel(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) vUser1;
	UserListEntry *u2 = (UserListEntry*) vUser2;

	return (u1->AccessLevel > u2->AccessLevel);
}
int CompareAccessLevelRev(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) vUser1;
	UserListEntry *u2 = (UserListEntry*) vUser2;

	return (u2->AccessLevel > u1->AccessLevel);
}


/*
 * Sort by UID
 */
int CompareUID(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) vUser1;
	UserListEntry *u2 = (UserListEntry*) vUser2;

	return (u1->UID > u2->UID);
}
int CompareUIDRev(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) vUser1;
	UserListEntry *u2 = (UserListEntry*) vUser2;

	return (u2->UID > u1->UID);
}

/*
 * Sort By Date /// TODO!
 */
int CompareLastLogon(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) vUser1;
	UserListEntry *u2 = (UserListEntry*) vUser2;

	return (u1->LastLogonT > u2->LastLogonT);
}
int CompareLastLogonRev(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) vUser1;
	UserListEntry *u2 = (UserListEntry*) vUser2;

	return (u2->LastLogonT > u1->LastLogonT);
}

/*
 * Sort By Number of Logons
 */
int ComparenLogons(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) vUser1;
	UserListEntry *u2 = (UserListEntry*) vUser2;

	return (u1->nLogons > u2->nLogons);
}
int ComparenLogonsRev(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) vUser1;
	UserListEntry *u2 = (UserListEntry*) vUser2;

	return (u2->nLogons > u1->nLogons);
}

/*
 * Sort By Number of Posts
 */
int ComparenPosts(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) vUser1;
	UserListEntry *u2 = (UserListEntry*) vUser2;

	return (u1->nPosts > u2->nPosts);
}
int ComparenPostsRev(const void *vUser1, const void *vUser2)
{
	UserListEntry *u1 = (UserListEntry*) vUser1;
	UserListEntry *u2 = (UserListEntry*) vUser2;

	return (u2->nPosts > u1->nPosts);
}


HashList *iterate_load_userlist(WCTemplateToken *Token)
{
	HashList *Hash;
	char buf[SIZ];
	StrBuf *Buf;
	UserListEntry* ul;
	char nnn[64];
	int nUsed;
	int Order;
	int len;
	
        serv_puts("LIST");
        serv_getln(buf, sizeof buf);
        if (buf[0] == '1') {
		Hash = NewHash(1, NULL);

		Buf = NewStrBuf();
		while ((len = StrBuf_ServGetln(Buf),
			strcmp(ChrPtr(Buf), "000"))) {
			ul = NewUserListEntry(Buf);
			if (ul == NULL)
				continue;
			nUsed = GetCount(Hash);
			nUsed = snprintf(nnn, sizeof(nnn), "%d", nUsed+1);
			Put(Hash, nnn, nUsed, ul, DeleteUserListEntry); 
		}
		FreeStrBuf(&Buf);
		Order = ibstr("SortOrder");
		switch (ibstr("SortBy")){
		case 1: /*NAME*/
			SortByPayload(Hash, (Order)? 
				      CompareUserListName:
				      CompareUserListNameRev);
			break;
		case 2: /*AccessLevel*/
			SortByPayload(Hash, (Order)? 
				      CompareAccessLevel:
				      CompareAccessLevelRev);
			break;
		case 3: /*nLogons*/
			SortByPayload(Hash, (Order)? 
				      ComparenLogons:
				      ComparenLogonsRev);
			break;
		case 4: /*UID*/
			SortByPayload(Hash, (Order)? 
				      CompareUID:
				      CompareUIDRev);
			break;
		case 5: /*LastLogon*/
			SortByPayload(Hash, (Order)? 
				      CompareLastLogon:
				      CompareLastLogonRev);
			break;
		case 6: /* nLogons */
			SortByPayload(Hash, (Order)? 
				      ComparenLogons:
				      ComparenLogonsRev);
			break;
		case 7: /* Posts */
			SortByPayload(Hash, (Order)? 
				      ComparenPosts:
				      ComparenPostsRev);
			break;
		}
		return Hash;
        }
	return NULL;
}


void tmplput_USERLIST_UserName(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	UserListEntry *ul = (UserListEntry*) Context;
/// TODO: X
	StrBufAppendBuf(Target, ul->UserName, 0);
}

void tmplput_USERLIST_AccessLevelNo(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	UserListEntry *ul = (UserListEntry*) Context;

	StrBufAppendPrintf(Target, "%d", ul->AccessLevel, 0);
}

void tmplput_USERLIST_AccessLevelStr(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	UserListEntry *ul = (UserListEntry*) Context;
	
	StrBufAppendBufPlain(Target, _(axdefs[ul->AccessLevel]), -1, 0);
}

void tmplput_USERLIST_UID(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	UserListEntry *ul = (UserListEntry*) Context;

	StrBufAppendPrintf(Target, "%d", ul->UID, 0);
}

void tmplput_USERLIST_LastLogonNo(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	UserListEntry *ul = (UserListEntry*) Context;

	StrBufAppendBuf(Target, ul->LastLogon, 0);
}
void tmplput_USERLIST_LastLogonStr(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	UserListEntry *ul = (UserListEntry*) Context;
	StrEscAppend(Target, NULL, asctime(localtime(&ul->LastLogonT)), 0, 0);
}

void tmplput_USERLIST_nLogons(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	UserListEntry *ul = (UserListEntry*) Context;

	StrBufAppendPrintf(Target, "%d", ul->nLogons, 0);
}

void tmplput_USERLIST_nPosts(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	UserListEntry *ul = (UserListEntry*) Context;

	StrBufAppendPrintf(Target, "%d", ul->nPosts, 0);
}

void tmplput_USERLIST_Flags(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	UserListEntry *ul = (UserListEntry*) Context;

	StrBufAppendPrintf(Target, "%d", ul->Flags, 0);
}

void tmplput_USERLIST_DaysTillPurge(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	UserListEntry *ul = (UserListEntry*) Context;

	StrBufAppendPrintf(Target, "%d", ul->DaysTillPurge, 0);
}

int ConditionalUser(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	UserListEntry *ul = (UserListEntry*) Context;
	if (havebstr("usernum")) {
		return ibstr("usernum") == ul->UID;
	}
	else if (havebstr("username")) {
		return strcmp(bstr("username"), ChrPtr(ul->UserName)) == 0;
	}
	else 
		return 0;
}

int ConditionalFlagINetEmail(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	UserListEntry *ul = (UserListEntry*) Context;
	return (ul->Flags & US_INTERNET) != 0;
}

int ConditionalUserAccess(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	UserListEntry *ul = (UserListEntry*) Context;

	if (Tokens->Params[3]->Type == TYPE_LONG)
		return (Tokens->Params[3]->lvalue == ul->AccessLevel);
	else
		return 0;
}

/**
 * \brief Locate the message number of a user's vCard in the current room
 * \param username the plaintext name of the user
 * \param usernum the number of the user on the citadel server
 * \return the message id of his vcard
 */
long locate_user_vcard(char *username, long usernum) {
	char buf[SIZ];
	long vcard_msgnum = (-1L);
	char content_type[SIZ];
	char partnum[SIZ];
	int already_tried_creating_one = 0;

	struct stuff_t {
		struct stuff_t *next;
		long msgnum;
	};

	struct stuff_t *stuff = NULL;
	struct stuff_t *ptr;

TRYAGAIN:
	/** Search for the user's vCard */
	serv_puts("MSGS ALL");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		ptr = malloc(sizeof(struct stuff_t));
		ptr->msgnum = atol(buf);
		ptr->next = stuff;
		stuff = ptr;
	}

	/** Iterate through the message list looking for vCards */
	while (stuff != NULL) {
		serv_printf("MSG0 %ld|2", stuff->msgnum);
		serv_getln(buf, sizeof buf);
		if (buf[0]=='1') {
			while(serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
				if (!strncasecmp(buf, "part=", 5)) {
					extract_token(partnum, &buf[5], 2, '|', sizeof partnum);
					extract_token(content_type, &buf[5], 4, '|', sizeof content_type);
					if (  (!strcasecmp(content_type, "text/x-vcard"))
					   || (!strcasecmp(content_type, "text/vcard")) ) {
						vcard_msgnum = stuff->msgnum;
					}
				}
			}
		}

		ptr = stuff->next;
		free(stuff);
		stuff = ptr;
	}

	/** If there's no vcard, create one */
	if (vcard_msgnum < 0) if (already_tried_creating_one == 0) {
		already_tried_creating_one = 1;
		serv_puts("ENT0 1|||4");
		serv_getln(buf, sizeof buf);
		if (buf[0] == '4') {
			serv_puts("Content-type: text/x-vcard");
			serv_puts("");
			serv_puts("begin:vcard");
			serv_puts("end:vcard");
			serv_puts("000");
		}
		goto TRYAGAIN;
	}

	return(vcard_msgnum);
}


/**
 * \brief Display the form for editing a user's address book entry
 * \param username the name of the user
 * \param usernum the citadel-uid of the user
 */
void display_edit_address_book_entry(char *username, long usernum) {
	char roomname[SIZ];
	char buf[SIZ];
	char error_message[SIZ];
	long vcard_msgnum = (-1L);

	/** Locate the user's config room, creating it if necessary */
	sprintf(roomname, "%010ld.%s", usernum, USERCONFIGROOM);
	serv_printf("GOTO %s||1", roomname);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') {
		serv_printf("CRE8 1|%s|5|||1|", roomname);
		serv_getln(buf, sizeof buf);
		serv_printf("GOTO %s||1", roomname);
		serv_getln(buf, sizeof buf);
		if (buf[0] != '2') {
			sprintf(error_message,
				"<img src=\"static/error.gif\" align=center>"
				"%s<br /><br />\n", &buf[4]);
			select_user_to_edit(error_message, username);
			return;
		}
	}

	vcard_msgnum = locate_user_vcard(username, usernum);

	if (vcard_msgnum < 0) {
		sprintf(error_message,
			"<img src=\"static/error.gif\" align=center>%s<br /><br />\n",
			_("An error occurred while trying to create or edit this address book entry.")
		);
		select_user_to_edit(error_message, username);
		return;
	}

	do_edit_vcard(vcard_msgnum, "1", "select_user_to_edit", roomname);
}


void display_edituser(char *supplied_username, int is_new) {
	UserListEntry* UL;
	StrBuf *Buf;
	char error_message[1024];
	char MajorStatus;
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
	MajorStatus = ChrPtr(Buf)[0];
	StrBufCutLeft(Buf, 4);
	if (MajorStatus != '2') {
		///TODO ImportantMessage
		sprintf(error_message,
			"<img src=\"static/error.gif\" align=center>"
			"%s<br /><br />\n", ChrPtr(Buf));
		select_user_to_edit(error_message, username);
		FreeStrBuf(&Buf);
		return;
	}
	else {
		UL = NewUserListOneEntry(Buf);
		output_headers(1, 0, 0, 0, 1, 0);
		DoTemplate(HKEY("userlist_detailview"), NULL, (void*) UL, CTX_USERLIST);
		end_burst();
		
	}
	FreeStrBuf(&Buf);
}



/* *
 * \brief Edit a user.  
 * If supplied_username is null, look in the "username"
 * web variable for the name of the user to edit.
 * 
 * If "is_new" is set to nonzero, this screen will set the web variables
 * to send the user to the vCard editor next.
 * \param supplied_username user to look up or NULL if to search in the environment
 * \param is_new should we create the user?
 * /
void display_edituser(char *supplied_username, int is_new) {
	char buf[1024];
	char error_message[1024];
	time_t now;

	char username[256];
	char password[256];
	unsigned int flags;
	int timescalled;
	int msgsposted;
	int axlevel;
	long usernum;
	time_t lastcall;
	int purgedays;
	int i;

	if (supplied_username != NULL) {
		safestrncpy(username, supplied_username, sizeof username);
	}
	else {
		safestrncpy(username, bstr("username"), sizeof username);
	}

	serv_printf("AGUP %s", username);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') {
		sprintf(error_message,
			"<img src=\"static/error.gif\" align=center>"
			"%s<br /><br />\n", &buf[4]);
		select_user_to_edit(error_message, username);
		return;
	}

	extract_token(username, &buf[4], 0, '|', sizeof username);
	extract_token(password, &buf[4], 1, '|', sizeof password);
	flags = extract_int(&buf[4], 2);
	timescalled = extract_int(&buf[4], 3);
	msgsposted = extract_int(&buf[4], 4);
	axlevel = extract_int(&buf[4], 5);
	usernum = extract_long(&buf[4], 6);
	lastcall = extract_long(&buf[4], 7);
	purgedays = extract_long(&buf[4], 8);

	if (havebstr("edit_abe_button")) {
		display_edit_address_book_entry(username, usernum);
		return;
	}

	if (havebstr("delete_button")) {
		delete_user(username);
		return;
	}

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<h1>");
	wprintf(_("Edit user account: "));
	escputs(username);
        wprintf("</h1>");
        wprintf("</div>");

        wprintf("<div id=\"content\" class=\"service\">\n");

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table class=\"useredit_background\"><tr><td>\n");
	wprintf("<form method=\"POST\" action=\"edituser\">\n"
		"<input type=\"hidden\" name=\"username\" value=\"");
	escputs(username);
	wprintf("\">\n");
	wprintf("<input type=\"hidden\" name=\"is_new\" value=\"%d\">\n"
		"<input type=\"hidden\" name=\"usernum\" value=\"%ld\">\n",
		is_new, usernum);
	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);

	wprintf("<input type=\"hidden\" name=\"flags\" value=\"%d\">\n", flags);

	wprintf("<center><table>");

	wprintf("<tr><td>");
	wprintf(_("User name:"));
	wprintf("</td><td>"
		"<input type=\"text\" name=\"newname\" value=\"");
	escputs(username);
	wprintf("\" maxlength=\"63\"></td></tr>\n");

	wprintf("<tr><td>");
	wprintf(_("Password"));
	wprintf("</td><td>"
		"<input type=\"password\" name=\"password\" value=\"");
	escputs(password);
	wprintf("\" maxlength=\"20\"></td></tr>\n");

	wprintf("<tr><td>");
	wprintf(_("Permission to send Internet mail"));
	wprintf("</td><td>");
	wprintf("<input type=\"checkbox\" name=\"inetmail\" value=\"yes\" ");
	if (flags & US_INTERNET) {
		wprintf("checked ");
	}
	wprintf("></td></tr>\n");

	wprintf("<tr><td>");
	wprintf(_("Number of logins"));
	wprintf("</td><td>"
		"<input type=\"text\" name=\"timescalled\" value=\"");
	wprintf("%d", timescalled);
	wprintf("\" maxlength=\"6\"></td></tr>\n");

	wprintf("<tr><td>");
	wprintf(_("Messages submitted"));
	wprintf("</td><td>"
		"<input type=\"text\" name=\"msgsposted\" value=\"");
	wprintf("%d", msgsposted);
	wprintf("\" maxlength=\"6\"></td></tr>\n");

	wprintf("<tr><td>");
	wprintf(_("Access level"));
	wprintf("</td><td>"
		"<select name=\"axlevel\">\n");
	for (i=0; i<7; ++i) {
		wprintf("<option ");
		if (axlevel == i) {
			wprintf("selected ");
		}
		wprintf("value=\"%d\">%d - %s</option>\n",
			i, i, axdefs[i]);
	}
	wprintf("</select></td></tr>\n");

	wprintf("<tr><td>");
	wprintf(_("User ID number"));
	wprintf("</td><td>"
		"<input type=\"text\" name=\"usernum\" value=\"");
	wprintf("%ld", usernum);
	wprintf("\" maxlength=\"7\"></td></tr>\n");

	now = time(NULL);
	wprintf("<tr><td>");
	wprintf(_("Date and time of last login"));
	wprintf("</td><td>"
		"<select name=\"lastcall\">\n");

	wprintf("<option selected value=\"%ld\">", lastcall);
	escputs(asctime(localtime(&lastcall)));
	wprintf("</option>\n");

	wprintf("<option value=\"%ld\">", now);
	escputs(asctime(localtime(&now)));
	wprintf("</option>\n");

	wprintf("</select></td></tr>");

	wprintf("<tr><td>");
	wprintf(_("Auto-purge after this many days"));
	wprintf("</td><td>"
		"<input type=\"text\" name=\"purgedays\" value=\"");
	wprintf("%d", purgedays);
	wprintf("\" maxlength=\"5\"></td></tr>\n");

	wprintf("</table>\n");

	wprintf("<input type=\"submit\" name=\"ok_button\" value=\"%s\">\n"
		"&nbsp;"
		"<input type=\"submit\" name=\"cancel\" value=\"%s\">\n"
		"<br /><br /></form>\n", _("Save changes"), _("Cancel"));

	wprintf("</center>\n");
	wprintf("</td></tr></table></div>\n");
	wDumpContent(1);

}
*/

/**
 * \brief do the backend operation of the user edit on the server
 */
void edituser(void) {
	char message[SIZ];
	char buf[SIZ];
	int is_new = 0;
	unsigned int flags = 0;
	char *username;

	is_new = ibstr("is_new");
	safestrncpy(message, "", sizeof message);
	username = bstr("username");

	if (!havebstr("ok_button")) {
		safestrncpy(message, _("Changes were not saved."), sizeof message);
	}
	
	else {
		flags = ibstr("flags");
		if (yesbstr("inetmail")) {
			flags |= US_INTERNET;
		}
		else {
			flags &= ~US_INTERNET ;
		}

		if ((havebstr("newname")) && (strcasecmp(bstr("username"), bstr("newname")))) {
			serv_printf("RENU %s|%s", bstr("username"), bstr("newname"));
			serv_getln(buf, sizeof buf);
			if (buf[0] != '2') {
				sprintf(&message[strlen(message)],
					"<img src=\"static/error.gif\" align=center>"
					"%s<br /><br />\n", &buf[4]);
			}
			else {
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
		serv_getln(buf, sizeof buf);
		if (buf[0] != '2') {
			sprintf(&message[strlen(message)],
				"<img src=\"static/error.gif\" align=center>"
				"%s<br /><br />\n", &buf[4]);
		}
	}

	/**
	 * If we are in the middle of creating a new user, move on to
	 * the vCard edit screen.
	 */
	if (is_new) {
		display_edit_address_book_entry(username, lbstr("usernum") );
	}
	else {
		select_user_to_edit(message, username);
	}
}

/*
 * \brief burge a user 
 * \param username the name of the user to remove
 */
void delete_user(char *username) {
	char buf[SIZ];
	char message[SIZ];

	serv_printf("ASUP %s|0|0|0|0|0|", username);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') {
		sprintf(message,
			"<img src=\"static/error.gif\" align=center>"
			"%s<br /><br />\n", &buf[4]);
	}
	else {
		safestrncpy(message, "", sizeof message);
	}
	select_user_to_edit(message, bstr("username"));
}
		


/**
 * \brief create a new user
 * take the web environment username and create it on the citadel server
 */
void create_user(void) {
	char buf[SIZ];
	char error_message[SIZ];
	char username[SIZ];

	safestrncpy(username, bstr("username"), sizeof username);

	serv_printf("CREU %s", username);
	serv_getln(buf, sizeof buf);

	if (buf[0] == '2') {
		sprintf(WC->ImportantMessage, _("A new user has been created."));
		display_edituser(username, 1);
	}
	else if (!strncmp(buf, "570", 3)) {
		sprintf(error_message,
			"<img src=\"static/error.gif\" align=center>"
			"%s<br /><br />\n",
			_("You are attempting to create a new user from within Citadel "
			"while running in host based authentication mode.  In this mode, "
			"you must create new users on the host system, not within Citadel.")
		);
		select_user_to_edit(error_message, NULL);
	}
	else {
		sprintf(error_message,
			"<img src=\"static/error.gif\" align=center>"
			"%s<br /><br />\n", &buf[4]);
		select_user_to_edit(error_message, NULL);
	}

}


void _select_user_to_edit(void){select_user_to_edit(NULL, NULL);}
void _display_edituser(void) {display_edituser(NULL, 0);}

void 
InitModule_USEREDIT
(void)
{
	WebcitAddUrlHandler(HKEY("select_user_to_edit"), _select_user_to_edit, 0);
	WebcitAddUrlHandler(HKEY("display_edituser"), _display_edituser, 0);
	WebcitAddUrlHandler(HKEY("edituser"), edituser, 0);
	WebcitAddUrlHandler(HKEY("create_user"), create_user, 0);

	RegisterNamespace("USERLIST:USERNAME",      0, 1, tmplput_USERLIST_UserName, CTX_USERLIST);
	RegisterNamespace("USERLIST:ACCLVLNO",      0, 0, tmplput_USERLIST_AccessLevelNo, CTX_USERLIST);
	RegisterNamespace("USERLIST:ACCLVLSTR",     0, 0, tmplput_USERLIST_AccessLevelStr, CTX_USERLIST);
	RegisterNamespace("USERLIST:UID",           0, 0, tmplput_USERLIST_UID, CTX_USERLIST);
	RegisterNamespace("USERLIST:LASTLOGON:STR", 0, 0, tmplput_USERLIST_LastLogonStr, CTX_USERLIST);
	RegisterNamespace("USERLIST:LASTLOGON:NO",  0, 0, tmplput_USERLIST_LastLogonNo, CTX_USERLIST);
	RegisterNamespace("USERLIST:NLOGONS",       0, 0, tmplput_USERLIST_nLogons, CTX_USERLIST);
	RegisterNamespace("USERLIST:NPOSTS",        0, 0, tmplput_USERLIST_nPosts, CTX_USERLIST);
						    
	RegisterNamespace("USERLIST:FLAGS",         0, 0, tmplput_USERLIST_Flags, CTX_USERLIST);
	RegisterNamespace("USERLIST:DAYSTILLPURGE", 0, 0, tmplput_USERLIST_DaysTillPurge, CTX_USERLIST);

	RegisterConditional(HKEY("COND:USERNAME"),  0,    ConditionalUser, CTX_USERLIST);
	RegisterConditional(HKEY("COND:USERACCESS"), 0,   ConditionalUserAccess, CTX_USERLIST);
	RegisterConditional(HKEY("COND:USERLIST:FLAG:USE_INTERNET"), 0, ConditionalFlagINetEmail, CTX_USERLIST);

	RegisterConditional(HKEY("COND:USERNAME"), 0, ConditionalUser, CTX_USERLIST);
	RegisterIterator("USERLIST", 0, NULL, iterate_load_userlist, NULL, DeleteHash, CTX_USERLIST);
}
/*@}*/
