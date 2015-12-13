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
#include "calendar.h"

CtxType CTX_VCARD = CTX_NONE;
CtxType CTX_VCARD_LIST = CTX_NONE;
CtxType CTX_VCARD_TYPE = CTX_NONE;
long VCEnumCounter = 0;

typedef enum _VCStrEnum {
	FlatString,
	StringCluster,
	PhoneNumber,
	EmailAddr,
	Address,
	Street,
	Number,
	AliasFor,
	Base64BinaryAttachment,
	UnKnown,
	TerminateList
}VCStrEnum;
typedef struct vcField vcField;
struct vcField {
	ConstStr STR;
	VCStrEnum Type;
	vcField *Sub;
	long cval;
	long parentCVal;
	ConstStr Name;
};

vcField VCStr_Ns [] = {
	{{HKEY("last")},   FlatString,    NULL, 0, 0, {HKEY("Last Name")}},
	{{HKEY("first")},  FlatString,    NULL, 0, 0, {HKEY("First Name")}},
	{{HKEY("middle")}, FlatString,    NULL, 0, 0, {HKEY("Middle Name")}},
	{{HKEY("prefix")}, FlatString,    NULL, 0, 0, {HKEY("Prefix")}},
	{{HKEY("suffix")}, FlatString,    NULL, 0, 0, {HKEY("Suffix")}},
	{{HKEY("")},       TerminateList, NULL, 0, 0, {HKEY("")}}
};

vcField VCStr_Addrs [] = {
	{{HKEY("POBox")},    Address,       NULL, 0, 0, {HKEY("PO box")}},
	{{HKEY("extadr")},   Address,       NULL, 0, 0, {HKEY("Address")}},
	{{HKEY("street")},   Address,       NULL, 0, 0, {HKEY("")}},
	{{HKEY("city")},     Address,       NULL, 0, 0, {HKEY("City")}},
	{{HKEY("state")},    Address,       NULL, 0, 0, {HKEY("State")}},
	{{HKEY("zip")},      Address,       NULL, 0, 0, {HKEY("ZIP code")}},
	{{HKEY("country")},  Address,       NULL, 0, 0, {HKEY("Country")}},
	{{HKEY("")},         TerminateList, NULL, 0, 0, {HKEY("")}}
};

vcField VCStrE [] = {
	{{HKEY("version")},         Number,                 NULL,        0, 0, {HKEY("")}},
	{{HKEY("rev")},             Number,                 NULL,        0, 0, {HKEY("")}},
	{{HKEY("label")},           FlatString,             NULL,        0, 0, {HKEY("")}},
	{{HKEY("uid")},             FlatString,             NULL,        0, 0, {HKEY("")}},
	{{HKEY("n")},               StringCluster,          VCStr_Ns,    0, 0, {HKEY("")}}, /* N is name, but only if there's no FN already there */
	{{HKEY("fn")},              FlatString,             NULL,        0, 0, {HKEY("")}}, /* FN (full name) is a true 'display name' field */
	{{HKEY("title")},           FlatString,             NULL,        0, 0, {HKEY("Title:")}},
	{{HKEY("org")},             FlatString,             NULL,        0, 0, {HKEY("Organization:")}},/* organization */
	{{HKEY("email")},           EmailAddr,              NULL,        0, 0, {HKEY("E-mail:")}},
	{{HKEY("tel")},             PhoneNumber,            NULL,        0, 0, {HKEY("Telephone:")}},
	{{HKEY("adr")},             StringCluster,          VCStr_Addrs, 0, 0, {HKEY("Address:")}},
	{{HKEY("photo")},           Base64BinaryAttachment, NULL,        0, 0, {HKEY("Photo:")}},
	{{HKEY("tel;home")},        PhoneNumber,            NULL,        0, 0, {HKEY(" (home)")}},
	{{HKEY("tel;work")},        PhoneNumber,            NULL,        0, 0, {HKEY(" (work)")}},
	{{HKEY("tel;fax")},         PhoneNumber,            NULL,        0, 0, {HKEY(" (fax)")}},
	{{HKEY("tel;cell")},        PhoneNumber,            NULL,        0, 0, {HKEY(" (cell)")}},
	{{HKEY("email;internet")},  EmailAddr,              NULL,        0, 0, {HKEY("E-mail:")}},
	{{HKEY("UNKNOWN")},         UnKnown,                NULL,        0, 0, {HKEY("")}},
	{{HKEY("")},                TerminateList,          NULL,        0, 0, {HKEY("")}}
};

ConstStr VCStr [] = {
	{HKEY("")},
	{HKEY("n")}, /* N is name, but only if there's no FN already there */
	{HKEY("fn")}, /* FN (full name) is a true 'display name' field */
	{HKEY("title")},   /* title */
	{HKEY("org")},    /* organization */
	{HKEY("email")},
	{HKEY("tel")},
	{HKEY("work")},
	{HKEY("home")},
	{HKEY("cell")},
	{HKEY("adr")},
	{HKEY("photo")},
	{HKEY("version")},
	{HKEY("rev")},
	{HKEY("label")},
	{HKEY("uid")}
};

/*
 * Address book entry (keep it short and sweet, it's just a quickie lookup
 * which we can use to get to the real meat and bones later)
 */
typedef struct _addrbookent {
	StrBuf *name;
	HashList *VC;
	long ab_msgnum;		/* message number of address book entry */
	StrBuf *msgNoStr;
} addrbookent;

void deleteAbEnt(void *v) {
	addrbookent *vc = (addrbookent*)v;
	DeleteHash(&vc->VC);
	FreeStrBuf(&vc->name);
	FreeStrBuf(&vc->msgNoStr);
	free(vc);
}

HashList *DefineToToken = NULL;
HashList *VCTokenToDefine = NULL;
HashList *vcNames = NULL; /* todo: fill with the name strings */
vcField* vcfUnknown = NULL;

/******************************************************************************
 *                   initialize vcard structure                               *
 ******************************************************************************/

void RegisterVCardToken(vcField* vf, StrBuf *name, int inTokenCount)
{
	if (vf->Type == UnKnown) {
		vcfUnknown = vf;
	}
	RegisterTokenParamDefine(SKEY(name), vf->cval);
	Put(DefineToToken, LKEY(vf->cval), vf, reference_free_handler);
	Put(vcNames, LKEY(vf->cval), NewStrBufPlain(CKEY(vf->Name)), HFreeStrBuf);

	syslog(LOG_DEBUG, "Token: %s -> %ld, %d", 
	       ChrPtr(name),
	       vf->cval, 
	       inTokenCount);

}

void autoRegisterTokens(long *enumCounter, vcField* vf, StrBuf *BaseStr, int layer, long parentCVal)
{
	int i = 0;
	StrBuf *subStr = NewStrBuf();
	while (vf[i].STR.len > 0) {
		FlushStrBuf(subStr);
		vf[i].cval = (*enumCounter) ++;
		vf[i].parentCVal = parentCVal;
		StrBufAppendBuf(subStr, BaseStr, 0);
		if (StrLength(subStr) > 0) {
			StrBufAppendBufPlain(subStr, HKEY("."), 0);
		}
		StrBufAppendBufPlain(subStr, CKEY(vf[i].STR), 0);
		if (layer == 0) {
			Put(VCTokenToDefine, CKEY(vf[i].STR), &vf[i], reference_free_handler);
		}
		switch (vf[i].Type) {
		case FlatString:
			break;
		case StringCluster:
		{
			autoRegisterTokens(enumCounter, vf[i].Sub, subStr, 1, vf[i].cval);
		}
		break;
		case PhoneNumber:
			break;
		case EmailAddr:
			break;
		case Street:
			break;
		case Number:
			break;
		case AliasFor:
			break;
		case Base64BinaryAttachment:
			break;
		case TerminateList:
			break;
		case Address:
			break;
		case UnKnown:
			break;
		}
		RegisterVCardToken(&vf[i], subStr, i);
		i++;
	}
	FreeStrBuf(&subStr);
}

/******************************************************************************
 *               VCard template functions                                     *
 ******************************************************************************/

int preeval_vcard_item(WCTemplateToken *Token)
{
	WCTemplputParams TPP;
	WCTemplputParams *TP;
	int searchFieldNo;
	StrBuf *Target = NULL;

	memset(&TPP, 0, sizeof(WCTemplputParams));
	TP = &TPP;
	TP->Tokens = Token;
	searchFieldNo = GetTemplateTokenNumber(Target, TP, 0, 0);
	if (searchFieldNo >= VCEnumCounter) {
		LogTemplateError(NULL, "VCardItem", ERR_PARM1, TP,
				 "Invalid define");
		return 0;
	}
	return 1;
}

void tmpl_vcard_item(StrBuf *Target, WCTemplputParams *TP)
{
	void *vItem;
	long searchFieldNo = GetTemplateTokenNumber(Target, TP, 0, 0);
	addrbookent *ab = (addrbookent*) CTX(CTX_VCARD);
	if (GetHash(ab->VC, LKEY(searchFieldNo), &vItem) && (vItem != NULL)) {
		StrBufAppendTemplate(Target, TP, (StrBuf*) vItem, 1);
	}
}

void tmpl_vcard_context_item(StrBuf *Target, WCTemplputParams *TP)
{
	void *vItem;
	vcField *t = (vcField*) CTX(CTX_VCARD_TYPE);
	addrbookent *ab = (addrbookent*) CTX(CTX_VCARD);

	if (t == NULL) {
		LogTemplateError(NULL, "VCard item", ERR_NAME, TP,
				 "Missing context");
		return;
	}

	if (GetHash(ab->VC, LKEY(t->cval), &vItem) && (vItem != NULL)) {
		StrBufAppendTemplate(Target, TP, (StrBuf*) vItem, 0);
	}
	else {
		LogTemplateError(NULL, "VCard item", ERR_NAME, TP,
				 "Doesn't have that key - did you miss to filter in advance?");
	}
}
int preeval_vcard_name_str(WCTemplateToken *Token)
{
	WCTemplputParams TPP;
	WCTemplputParams *TP;
	int searchFieldNo;
	StrBuf *Target = NULL;

	memset(&TPP, 0, sizeof(WCTemplputParams));
	TP = &TPP;
	TP->Tokens = Token;
	searchFieldNo = GetTemplateTokenNumber(Target, TP, 0, 0);
	if (searchFieldNo >= VCEnumCounter) {
		LogTemplateError(NULL, "VCardName", ERR_PARM1, TP,
				 "Invalid define");
		return 0;
	}
	return 1;
}

void tmpl_vcard_name_str(StrBuf *Target, WCTemplputParams *TP)
{
	void *vItem;
	long searchFieldNo = GetTemplateTokenNumber(Target, TP, 0, 0);
	/* todo: get descriptive string for this vcard type */
	if (GetHash(vcNames, LKEY(searchFieldNo), &vItem) && (vItem != NULL)) {
		StrBufAppendTemplate(Target, TP, (StrBuf*) vItem, 1);
	}
	else {
		LogTemplateError(NULL, "VCard item type", ERR_NAME, TP,
				 "No i18n string for this.");
		return;
	}
}

void tmpl_vcard_msgno(StrBuf *Target, WCTemplputParams *TP)
{
	addrbookent *ab = (addrbookent*) CTX(CTX_VCARD);
	if (ab->msgNoStr == NULL) {
		ab->msgNoStr = NewStrBufPlain(NULL, 64);
	}
	StrBufPrintf(ab->msgNoStr, "%ld", ab->ab_msgnum);
	StrBufAppendTemplate(Target, TP, ab->msgNoStr, 0);
}
void tmpl_vcard_context_name_str(StrBuf *Target, WCTemplputParams *TP)
{
	void *vItem;
	vcField *t = (vcField*) CTX(CTX_VCARD_TYPE);

	if (t == NULL) {
		LogTemplateError(NULL, "VCard item type", ERR_NAME, TP,
				 "Missing context");
		return;
	}
	
	if (GetHash(vcNames, LKEY(t->cval), &vItem) && (vItem != NULL)) {
		StrBufAppendTemplate(Target, TP, (StrBuf*) vItem, 1);
	}
	else {
		LogTemplateError(NULL, "VCard item type", ERR_NAME, TP,
				 "No i18n string for this.");
		return;
	}
}

int filter_VC_ByType(const char* key, long len, void *Context, StrBuf *Target, WCTemplputParams *TP)
{
	long searchType;
	long type = 0;
	void *v;
	int rc = 0;
	vcField *vf = (vcField*) Context;

	memcpy(&type, key, sizeof(long));
	searchType = GetTemplateTokenNumber(Target, TP, IT_ADDT_PARAM(0), 0);
	
	if (vf->Type == searchType) {
		addrbookent *ab = (addrbookent*) CTX(CTX_VCARD);
		if (GetHash(ab->VC, LKEY(vf->cval), &v) && v != NULL)
			return 1;
	}
	return rc;
}

HashList *getContextVcard(StrBuf *Target, WCTemplputParams *TP)
{
	vcField *vf = (vcField*) CTX(CTX_VCARD_TYPE);
	addrbookent *ab = (addrbookent*) CTX(CTX_VCARD);

	if ((vf == NULL) || (ab == NULL)) {
		LogTemplateError(NULL, "VCard item type", ERR_NAME, TP,
				 "Need VCard and Vcard type in context");
		
		return NULL;
	}
	return ab->VC;
}

int filter_VC_ByContextType(const char* key, long len, void *Context, StrBuf *Target, WCTemplputParams *TP)
{
	long searchType;
	vcField *vf = (vcField*) CTX(CTX_VCARD_TYPE);

	memcpy(&searchType, key, sizeof(long));
	
	if (vf->cval == searchType) {
		return 1;
	}
	else {
		return 0;
	}
}

int conditional_VC_Havetype(StrBuf *Target, WCTemplputParams *TP)
{
	addrbookent *ab = (addrbookent*) CTX(CTX_VCARD);
	long HaveFieldType = GetTemplateTokenNumber(Target, TP, 2, 0);
	int rc = 0;	
	void *vVCitem;
	const char *Key;
	long len;
	HashPos *it = GetNewHashPos(ab->VC, 0);
	while (GetNextHashPos(ab->VC, it, &len, &Key, &vVCitem) && 
	       (vVCitem != NULL)) 
	{
		void *vvcField;
		long type = 0;
		memcpy(&type, Key, sizeof(long));
		if (GetHash(DefineToToken, LKEY(type), &vvcField) &&
		    (vvcField != NULL))
		{
			vcField *t = (vcField*) vvcField;
			if (t && t->Type == HaveFieldType) {
				rc = 1;
				break;
			}
		}
	}
	DeleteHashPos(&it);
	return rc;
}

/******************************************************************************
 *              parse one VCard                                               *
 ******************************************************************************/

void PutVcardItem(HashList *thisVC, vcField *thisField, StrBuf *ThisFieldStr, int is_qp, StrBuf *Swap)
{
	/* if we have some untagged QP, detect it here. */
	if (is_qp || (strstr(ChrPtr(ThisFieldStr), "=?")!=NULL)){
		FlushStrBuf(Swap);
		StrBuf_RFC822_to_Utf8(Swap, ThisFieldStr, NULL, NULL); /* default charset, current charset */
		SwapBuffers(Swap, ThisFieldStr);
		FlushStrBuf(Swap);
	}
	Put(thisVC, LKEY(thisField->cval), ThisFieldStr, HFreeStrBuf);
}

void parse_vcard(StrBuf *Target, struct vCard *v, HashList *VC, wc_mime_attachment *Mime)
{
	StrBuf *Swap = NULL;
	int i, j, k;
	char buf[SIZ];
	int is_qp = 0;
	int is_b64 = 0;
	int ntokens, len;
	StrBuf *thisname = NULL;
	char firsttoken[SIZ];
	StrBuf *thisVCToken;
	void *vField = NULL;

	Swap = NewStrBuf ();
	thisname = NewStrBuf();
	thisVCToken = NewStrBufPlain(NULL, 63);
	for (i=0; i<(v->numprops); ++i) {
		FlushStrBuf(thisVCToken);
		is_qp = 0;
		is_b64 = 0;
		syslog(LOG_DEBUG, "i: %d oneprop: %s - value: %s", i, v->prop[i].name, v->prop[i].value);
		StrBufPlain(thisname, v->prop[i].name, -1);
		StrBufLowerCase(thisname);
		
		/*len = */extract_token(firsttoken, ChrPtr(thisname), 0, ';', sizeof firsttoken);
		ntokens = num_tokens(ChrPtr(thisname), ';');
		for (j=0, k=0; j < ntokens && k < 10; ++j) {
			len = extract_token(buf, ChrPtr(thisname), j, ';', sizeof buf);
			if (!strcasecmp(buf, "encoding=quoted-printable")) {
				is_qp = 1;
			}
			else if (!strcasecmp(buf, "encoding=base64")) {
				is_b64 = 1;
			}
			else{
				if (StrLength(thisVCToken) > 0) {
					StrBufAppendBufPlain(thisVCToken, HKEY(";"), 0);
				}
				StrBufAppendBufPlain(thisVCToken, buf, len, 0);
			}
		}

		vField = NULL;	
		if ((StrLength(thisVCToken) > 0) &&
		    GetHash(VCTokenToDefine, SKEY(thisVCToken), &vField) && 
		    (vField != NULL)) {
			vcField *thisField = (vcField *)vField;
			StrBuf *ThisFieldStr = NULL;
			syslog(LOG_DEBUG, "got this token: %s, found: %s", ChrPtr(thisVCToken), thisField->STR.Key);
			switch (thisField->Type) {
			case StringCluster: {
				int j = 0;
				const char *Pos = NULL;
				StrBuf *thisArray = NewStrBufPlain(v->prop[i].value, -1);
				StrBuf *Buf = NewStrBufPlain(NULL, StrLength(thisArray));
				while (thisField->Sub[j].STR.len > 0) {
					StrBufExtract_NextToken(Buf, thisArray, &Pos, ';');
					ThisFieldStr = NewStrBufDup(Buf);
					
					PutVcardItem(VC, &thisField->Sub[j], ThisFieldStr, is_qp, Swap);
					j++;
				}
				FreeStrBuf(&thisArray);
				FreeStrBuf(&Buf);
			}
				break;
			case Address:
			case FlatString:
			case PhoneNumber:
			case EmailAddr:
			case Street:
			case Number:
			case AliasFor:
				/* copy over the payload into a StrBuf */
				ThisFieldStr = NewStrBufPlain(v->prop[i].value, -1);
				PutVcardItem(VC, thisField, ThisFieldStr, is_qp, Swap);

				break;
			case Base64BinaryAttachment:
				ThisFieldStr = NewStrBufPlain(v->prop[i].value, -1);
				StrBufDecodeBase64(ThisFieldStr);
				PutVcardItem(VC, thisField, ThisFieldStr, is_qp, Swap);
				break;
			case TerminateList:
			case UnKnown:
				break;
			}

		}
		else if (StrLength(thisVCToken) > 0) {
			/* Add it to the UNKNOWN field... */
			void *pv = NULL;
			StrBuf *oldVal;
			GetHash(VC, IKEY(vcfUnknown->cval), &pv);
			oldVal = (StrBuf*) pv;
			if (oldVal == NULL) {
				oldVal = NewStrBuf();
				Put(VC, IKEY(vcfUnknown->cval), oldVal, HFreeStrBuf);
			}
			else {
				StrBufAppendBufPlain(oldVal, HKEY("\n"), 0);
			}

			StrBufAppendBuf(oldVal, thisVCToken, 0);
			StrBufAppendBufPlain(oldVal, HKEY(":"), 0);
			StrBufAppendBufPlain(oldVal, v->prop[i].value, -1, 0);
			continue;
		}
	}
	FreeStrBuf(&thisname);
	FreeStrBuf(&Swap);
	FreeStrBuf(&thisVCToken);
}

HashList *CtxGetVcardList(StrBuf *Target, WCTemplputParams *TP)
{
	HashList *pb = CTX(CTX_VCARD_LIST);
	return pb;
}

/******************************************************************************
 * Extract an embedded photo from a vCard for display on the client           *
 ******************************************************************************/

void display_vcard_photo_img(void)
{
	long msgnum = 0L;
	StrBuf *vcard;
	struct vCard *v;
	char *photosrc;
	const char *contentType;
	wcsession *WCC = WC;

	msgnum = StrBufExtract_long(WCC->Hdr->HR.ReqLine, 0, '/');
	
	vcard = load_mimepart(msgnum,"1");
	v = VCardLoad(vcard);
	
	photosrc = vcard_get_prop(v, "PHOTO", 1,0,0);
	FlushStrBuf(WCC->WBuf);
	StrBufAppendBufPlain(WCC->WBuf, photosrc, -1, 0);
	if (StrBufDecodeBase64(WCC->WBuf) <= 0) {
		FlushStrBuf(WCC->WBuf);
		
		hprintf("HTTP/1.1 500 %s\n","Unable to get photo");
		output_headers(0, 0, 0, 0, 0, 0);
		hprintf("Content-Type: text/plain\r\n");
		begin_burst();
		wc_printf(_("Could Not decode vcard photo\n"));
		end_burst();
		return;
	}
	contentType = GuessMimeType(ChrPtr(WCC->WBuf), StrLength(WCC->WBuf));
	http_transmit_thing(contentType, 0);
	free(v);
	free(photosrc);
}

wc_mime_attachment *load_vcard(message_summary *Msg) 
{
	HashPos  *it;
	StrBuf *FoundCharset = NewStrBuf();
	StrBuf *Error;
	void *vMime;
	const char *Key;
	long len;
	wc_mime_attachment *Mime;
	wc_mime_attachment *VCMime = NULL;

	Msg->MsgBody =  (wc_mime_attachment*) malloc(sizeof(wc_mime_attachment));
	memset(Msg->MsgBody, 0, sizeof(wc_mime_attachment));
	Msg->MsgBody->msgnum = Msg->msgnum;

	load_message(Msg, FoundCharset, &Error);

	FreeStrBuf(&FoundCharset);
	/* look up the vcard... */
	it = GetNewHashPos(Msg->AllAttach, 0);
	while (GetNextHashPos(Msg->AllAttach, it, &len, &Key, &vMime) && 
	       (vMime != NULL)) 
	{
		Mime = (wc_mime_attachment*) vMime;
		if ((strcmp(ChrPtr(Mime->ContentType),
			   "text/x-vcard") == 0) ||
		    (strcmp(ChrPtr(Mime->ContentType),
			    "text/vcard") == 0))
		{
			VCMime = Mime;
			break;
		}
	}
	DeleteHashPos(&it);
	if (VCMime == NULL)
		return NULL;

	if (VCMime->Data == NULL)
		MimeLoadData(VCMime);
	return VCMime;
}

/*
 * Edit the vCard component of a MIME message.  
 * Supply the message number
 * and MIME part number to fetch.  Or, specify -1 for the message number
 * to start with a blank card.
 */
void do_edit_vcard(long msgnum, char *partnum, 
		   message_summary *VCMsg,
		   wc_mime_attachment *VCAtt,
		   const char *return_to, 
		   const char *force_room) {
	WCTemplputParams SubTP;
	wcsession *WCC = WC;
	message_summary *Msg = NULL;
	wc_mime_attachment *VCMime = NULL;
	struct vCard *v;
	char whatuser[256];
	addrbookent ab;

	memset(&ab, 0, sizeof(addrbookent));
	ab.VC = NewHash(0, lFlathash);
	/* Display the form */
	output_headers(1, 1, 1, 0, 0, 0);

	safestrncpy(whatuser, "", sizeof whatuser);

	if ((msgnum >= 0) || 
	    ((VCMsg != NULL) && (VCAtt != NULL)))
	{
		if ((VCMsg == NULL) && (VCAtt == NULL)) {

			Msg = (message_summary *) malloc(sizeof(message_summary));
			memset(Msg, 0, sizeof(message_summary));
			Msg->msgnum = msgnum;
			VCMime = load_vcard(Msg);
			if (VCMime == NULL) {
				convenience_page("770000", _("Error"), "");/*TODO: important message*/
				DestroyMessageSummary(Msg);
				return;
				DeleteHash(&ab.VC);
			}
		
			v = VCardLoad(VCMime->Data);
		}
		else {
			v = VCardLoad(VCAtt->Data);
		}

		parse_vcard(WCC->WBuf, v, ab.VC, NULL);
	
	
		vcard_free(v);
	}

        memset(&SubTP, 0, sizeof(WCTemplputParams));    
	{
		WCTemplputParams *TP = NULL;
		WCTemplputParams SubTP;

		StackContext(TP, &SubTP, &ab, CTX_VCARD, 0, NULL);

		DoTemplate(HKEY("vcard_edit"), WCC->WBuf, &SubTP);
		UnStackContext(&SubTP);
	}
	DeleteHash(&ab.VC);


	wDumpContent(1);
	if (Msg != NULL) {
		DestroyMessageSummary(Msg);
	}
}


/*
 *  commit the edits to the citadel server
 */
void edit_vcard(void) {
	long msgnum;
	char *partnum;

	msgnum = lbstr("msgnum");
	partnum = bstr("partnum");
	do_edit_vcard(msgnum, partnum, NULL, NULL, "", NULL);
}

/*
 *  parse edited vcard from the browser
 */
void submit_vcard(void) {
	struct vCard *v;
	char *serialized_vcard;
	StrBuf *Buf;
	const StrBuf *ForceRoom;
	HashList* postVcard;
	HashPos *it, *itSub;
	const char *Key;
	long len;
	void *pv;
	StrBuf *SubStr;
	const StrBuf *s;
	const char *Pos = NULL;

	if (!havebstr("ok_button")) { 
		readloop(readnew, eUseDefault);
		return;
	}

	if (havebstr("force_room")) {
		ForceRoom = sbstr("force_room");
		if (gotoroom(ForceRoom) != 200) {
			AppendImportantMessage(_("Unable to enter the room to save your message"), -1);
			AppendImportantMessage(HKEY(": "));
			AppendImportantMessage(SKEY(ForceRoom));
			AppendImportantMessage(HKEY("; "));
			AppendImportantMessage(_("Aborting."), -1);

			if (!strcmp(bstr("return_to"), "select_user_to_edit")) {
				select_user_to_edit(NULL);
			}
			else if (!strcmp(bstr("return_to"), "do_welcome")) {
				do_welcome();
			}
			else if (!IsEmptyStr(bstr("return_to"))) {
				http_redirect(bstr("return_to"));
			}
			else {
				readloop(readnew, eUseDefault);
			}
			return;
		}
	}

	postVcard = getSubStruct(HKEY("VC"));
	if (postVcard == NULL) {
		AppendImportantMessage(_("An error has occurred."), -1);
		edit_vcard();
		return;/*/// more details*/
	}
	
	Buf = NewStrBuf();
	serv_write(HKEY("ENT0 1|||4\n"));
	if (!StrBuf_ServGetln(Buf) && (GetServerStatus(Buf, NULL) != 4))
	{
		edit_vcard();
		return;
	}
	
	/* Make a vCard structure out of the data supplied in the form */
	StrBufPrintf(Buf, "begin:vcard\r\n%s\r\nend:vcard\r\n",
		     bstr("extrafields")
	);
	v = VCardLoad(Buf);	/* Start with the extra fields */
	if (v == NULL) {
		AppendImportantMessage(_("An error has occurred."), -1);
		edit_vcard();
		FreeStrBuf(&Buf);
		return;
	}

	SubStr = NewStrBuf();
	it = GetNewHashPos(DefineToToken, 0);
	while (GetNextHashPos(DefineToToken, it, &len, &Key, &pv) && 
	       (pv != NULL)) 
	{
		char buf[32];
		long blen;
		vcField *t = (vcField*) pv;

 		if (t->Sub != NULL){
			vcField *Sub;
			FlushStrBuf(SubStr);
			itSub = GetNewHashPos(DefineToToken, 0);
			while (GetNextHashPos(DefineToToken, itSub, &len, &Key, &pv) && 
			       (pv != NULL)) 
			{
				Sub = (vcField*) pv;
				if (Sub->parentCVal == t->cval) {
					if (StrLength(SubStr) > 0)
						StrBufAppendBufPlain(SubStr, HKEY(";"), 0);



					blen = snprintf(buf, sizeof(buf), "%ld", Sub->cval);
					s = SSubBstr(postVcard, buf, blen);
			
					if ((s != NULL) && (StrLength(s) > 0)) {
						/// todo: utf8 qp
						StrBufAppendBuf(SubStr, s, 0);
					}
				}
			}
			if (StrLength(SubStr) > 0) {
				vcard_add_prop(v, t->STR.Key, ChrPtr(SubStr));
			}
			DeleteHashPos(&itSub);
		}
		else if (t->parentCVal == 0) {
			blen = snprintf(buf, sizeof(buf), "%ld", t->cval);
			s = SSubBstr(postVcard, buf, blen);
			
			if ((s != NULL) && (StrLength(s) > 0)) {
				vcard_add_prop(v, t->STR.Key, ChrPtr(s));
			}
		}
	}
	DeleteHashPos(&it);

	s = sbstr("other_inetemail");
	if (StrLength(s) > 0) {
		FlushStrBuf(SubStr);
		while (StrBufSipLine(SubStr, s, &Pos), ((Pos!=StrBufNOTNULL) && (Pos!=NULL)) ) {
			if (StrLength(SubStr) > 0) {
				vcard_add_prop(v, "email;internet", ChrPtr(SubStr));
			}
		}
	}

	FreeStrBuf(&SubStr);


	serialized_vcard = vcard_serialize(v);
	vcard_free(v);
	if (serialized_vcard == NULL) {
		AppendImportantMessage(_("An error has occurred."), -1);
		edit_vcard();
		FreeStrBuf(&Buf);
		return;
	}

	printf("%s", serialized_vcard);
	serv_write(HKEY("Content-type: text/x-vcard; charset=UTF-8\n"));
	serv_write(HKEY("\n"));
	serv_printf("%s\r\n", serialized_vcard);
	serv_write(HKEY("000\n"));
	free(serialized_vcard);

	if (!strcmp(bstr("return_to"), "select_user_to_edit")) {
		select_user_to_edit(NULL);
	}
	else if (!strcmp(bstr("return_to"), "do_welcome")) {
		do_welcome();
	}
	else if (!IsEmptyStr(bstr("return_to"))) {
		http_redirect(bstr("return_to"));
	}
	else {
		readloop(readnew, eUseDefault);
	}
	FreeStrBuf(&Buf);
}

/******************************************************************************
 *              Render Addressbooks                                           *
 ******************************************************************************/

typedef struct _vcardview_struct {
	long is_singlecard;
	HashList *addrbook;

} vcardview_struct;

int vcard_GetParamsGetServerCall(SharedMessageStatus *Stat, 
				 void **ViewSpecific, 
				 long oper, 
				 char *cmd, 
				 long len,
				 char *filter,
				 long flen)
{
	vcardview_struct *VS;

	VS = (vcardview_struct*) malloc (sizeof(vcardview_struct));
	memset(VS, 0, sizeof(vcardview_struct));
	*ViewSpecific = (void*)VS;

	VS->is_singlecard = ibstr("is_singlecard");
	if (VS->is_singlecard != 1) {
		VS->addrbook = NewHash(0, NULL);
		if (oper == do_search) {
			snprintf(cmd, len, "MSGS SEARCH|%s", bstr("query"));
		}
		else {
			strcpy(cmd, "MSGS ALL");
		}
		Stat->maxmsgs = 9999999;
	}
	return 200;
}

int vcard_LoadMsgFromServer(SharedMessageStatus *Stat, 
			    void **ViewSpecific, 
			    message_summary* Msg, 
			    int is_new, 
			    int i)
{
	wcsession *WCC = WC;
	WCTemplputParams *TP = NULL;
	WCTemplputParams SubTP;
	vcardview_struct *VS;
	wc_mime_attachment *VCMime = NULL;
	struct vCard *v;
	addrbookent* abEntry;

	VS = (vcardview_struct*) *ViewSpecific;

	VCMime = load_vcard(Msg);
	if (VCMime == NULL)
		return 0;

	v = VCardLoad(VCMime->Data);

	if (v == NULL) return 0;

	abEntry = (addrbookent*) malloc(sizeof(addrbookent));
	memset(abEntry, 0, sizeof(addrbookent));
	abEntry->name = NewStrBuf();
	abEntry->VC = NewHash(0, lFlathash);
	abEntry->ab_msgnum = Msg->msgnum;
	parse_vcard(WCC->WBuf, v, abEntry->VC, VCMime);

        memset(&SubTP, 0, sizeof(WCTemplputParams));    
	StackContext(TP, &SubTP, abEntry, CTX_VCARD, 0, NULL);

	DoTemplate(HKEY("vcard_list_name"), WCC->WBuf, &SubTP);
	UnStackContext(&SubTP);

	if (StrLength(abEntry->name) == 0) {
		StrBufPlain(abEntry->name, _("(no name)"), -1);
	}

	vcard_free(v);
	
	Put(VS->addrbook, SKEY(abEntry->name), abEntry, deleteAbEnt);
	return 0;
}


/*
 * Render the address book using info we gathered during the scan
 *
 * addrbook	the addressbook to render
 * num_ab	the number of the addressbook
 */
static int NAMESPERPAGE = 60;
void do_addrbook_view(vcardview_struct* VS) {
	long i = 0;
	int num_pages = 0;
	int tabfirst = 0;
	int tablast = 0;
	StrBuf **tablabels;
	int num_ab = GetCount(VS->addrbook);
	HashList *headlines;
	wcsession *WCC = WC;

	WCTemplputParams *TP = NULL;
	WCTemplputParams SubTP;

        memset(&SubTP, 0, sizeof(WCTemplputParams));    
	
	if (num_ab == 0) {
		do_template("vcard_list_empty");
		return;
	}

	if (num_ab > 1) {
		SortByHashKey(VS->addrbook, 1);
	}

	num_pages = (GetCount(VS->addrbook) / NAMESPERPAGE) + 1;

	tablabels = malloc(num_pages * sizeof (StrBuf *));
	if (tablabels == NULL) {
		return;
	}

	headlines = NewHash(0, lFlathash);
	for (i=0; i<num_pages; ++i) {
		void *v1 = NULL;
		void *v2 = NULL;
		long hklen1, hklen2;
		const char *c1, *c2;
		StrBuf *headline;
		addrbookent *a1, *a2;

		tabfirst = i * NAMESPERPAGE;
		tablast = tabfirst + NAMESPERPAGE - 1;
		if (tablast > (num_ab - 1)) tablast = (num_ab - 1);

		headline = NewStrBufPlain(NULL, StrLength(v1) + StrLength(v2) + 10);
		if (GetHashAt(VS->addrbook, tabfirst, &hklen1, &c1, &v1)) {
			a1 = (addrbookent*) v1;
			StrBufAppendBuf(headline, a1->name, 0);
			StrBuf_Utf8StrCut(headline, 3);
			if (GetHashAt(VS->addrbook, tablast, &hklen2, &c2, &v2)) {

				a2 = (addrbookent*) v2;
				StrBufAppendBufPlain(headline, HKEY(" - "), 0);
				StrBufAppendBuf(headline, a2->name, 0);
				StrBuf_Utf8StrCut(headline, 9);
			}
		}
		tablabels[i] = headline;
		Put(headlines, LKEY(i), headline, HFreeStrBuf);
	}
	StrTabbedDialog(WC->WBuf, num_pages, tablabels);
	StackContext(TP, &SubTP, VS->addrbook, CTX_VCARD_LIST, 0, NULL);

	DoTemplate(HKEY("vcard_list"), WCC->WBuf, &SubTP);
	UnStackContext(&SubTP);
	DeleteHash(&headlines);
	free(tablabels);
	StrBufAppendBufPlain(WCC->WBuf, HKEY("</div>"), 0);/* closes: id=global */
}



int vcard_RenderView_or_Tail(SharedMessageStatus *Stat, void **ViewSpecific, long oper)
{
	const StrBuf *Mime;
	vcardview_struct *VS;

	VS = (vcardview_struct*) *ViewSpecific;
	if (VS->is_singlecard)
		read_message(WC->WBuf, HKEY("view_message"), lbstr("startmsg"), NULL, &Mime, NULL);
	else
		do_addrbook_view(VS);	/* Render the address book */
	return 0;
}

int vcard_Cleanup(void **ViewSpecific)
{
	vcardview_struct *VS;

	VS = (vcardview_struct*) *ViewSpecific;
	wDumpContent(1);
	if ((VS != NULL) && 
	    (VS->addrbook != NULL))
		DeleteHash(&VS->addrbook);
	if (VS != NULL) 
		free(VS);

	return 0;
}

void render_MIME_VCard(StrBuf *Target, WCTemplputParams *TP, StrBuf *FoundCharset)
{
	wc_mime_attachment *Mime = (wc_mime_attachment *) CTX(CTX_MIME_ATACH);
	wcsession *WCC = WC;
	if (StrLength(Mime->Data) == 0)
		MimeLoadData(Mime);
	if (StrLength(Mime->Data) > 0) {
		struct vCard *v;
		StrBuf *Buf;

		Buf = NewStrBuf();
		/** If it's my vCard I can edit it */
		if (	(!strcasecmp(ChrPtr(WCC->CurRoom.name), USERCONFIGROOM))
			|| ((StrLength(WCC->CurRoom.name) > 11) &&
			    (!strcasecmp(&(ChrPtr(WCC->CurRoom.name)[11]), USERCONFIGROOM)))
			|| (WCC->CurRoom.view == VIEW_ADDRESSBOOK)
			) {
			StrBufAppendPrintf(Buf, "<a href=\"edit_vcard?msgnum=%ld?partnum=%s\">",
				Mime->msgnum, ChrPtr(Mime->PartNum));
			StrBufAppendPrintf(Buf, "[%s]</a>", _("edit"));
		}

		/* In all cases, display the full card */

		v = VCardLoad(Mime->Data);

		if (v != NULL) {
			WCTemplputParams *TP = NULL;
			WCTemplputParams SubTP;
			addrbookent ab;
			memset(&ab, 0, sizeof(addrbookent));

			ab.VC = NewHash(0, lFlathash);
			ab.ab_msgnum = Mime->msgnum;

			parse_vcard(Target, v, ab.VC, Mime);

			memset(&SubTP, 0, sizeof(WCTemplputParams));    
			StackContext(TP, &SubTP, &ab, CTX_VCARD, 0, NULL);

			DoTemplate(HKEY("vcard_msg_display"), Target, &SubTP);
			UnStackContext(&SubTP);
			DeleteHash(&ab.VC);
			vcard_free(v);

		}
		else {
			StrBufPlain(Buf, _("failed to load vcard"), -1);
		}
		FreeStrBuf(&Mime->Data);
		Mime->Data = Buf;
	}

}

void 
ServerStartModule_VCARD
(void)
{
}

void 
ServerShutdownModule_VCARD
(void)
{
	DeleteHash(&DefineToToken);
	DeleteHash(&vcNames);
	DeleteHash(&VCTokenToDefine);
}

void 
InitModule_VCARD
(void)
{
	StrBuf *Prefix  = NewStrBufPlain(HKEY("VC:"));
	DefineToToken   = NewHash(1, lFlathash);
	vcNames         = NewHash(1, lFlathash);
	VCTokenToDefine = NewHash(1, NULL);
	autoRegisterTokens(&VCEnumCounter, VCStrE, Prefix, 0, 0);
	FreeStrBuf(&Prefix);

	REGISTERTokenParamDefine(NAMESPERPAGE);


	RegisterCTX(CTX_VCARD);
	RegisterCTX(CTX_VCARD_LIST);
	RegisterCTX(CTX_VCARD_TYPE);

	RegisterReadLoopHandlerset(
		VIEW_ADDRESSBOOK,
		vcard_GetParamsGetServerCall,
		NULL,
		NULL,
		NULL, 
		vcard_LoadMsgFromServer,
		vcard_RenderView_or_Tail,
		vcard_Cleanup);

	RegisterIterator("MAIL:VCARDS", 0, NULL, CtxGetVcardList, NULL, NULL, CTX_VCARD, CTX_VCARD_LIST, IT_NOFLAG);


	WebcitAddUrlHandler(HKEY("edit_vcard"), "", 0, edit_vcard, 0);
	WebcitAddUrlHandler(HKEY("submit_vcard"), "", 0, submit_vcard, 0);
	WebcitAddUrlHandler(HKEY("vcardphoto"), "", 0, display_vcard_photo_img, NEED_URL);

	RegisterNamespace("VC:ITEM", 2, 2, tmpl_vcard_item, preeval_vcard_item, CTX_VCARD);
	RegisterNamespace("VC:CTXITEM", 1, 1, tmpl_vcard_context_item, NULL, CTX_VCARD_TYPE);
	RegisterNamespace("VC:NAME", 1, 1, tmpl_vcard_name_str, preeval_vcard_name_str, CTX_VCARD);
	RegisterNamespace("VC:MSGNO", 0, 1, tmpl_vcard_msgno, NULL, CTX_VCARD);
	RegisterNamespace("VC:CTXNAME", 1, 1, tmpl_vcard_context_name_str, NULL, CTX_VCARD_TYPE);
	REGISTERTokenParamDefine(FlatString);
	REGISTERTokenParamDefine(StringCluster);
	REGISTERTokenParamDefine(PhoneNumber);
	REGISTERTokenParamDefine(EmailAddr);
	REGISTERTokenParamDefine(Street);
	REGISTERTokenParamDefine(Number);
	REGISTERTokenParamDefine(AliasFor);
	REGISTERTokenParamDefine(Base64BinaryAttachment);
	REGISTERTokenParamDefine(TerminateList);
	REGISTERTokenParamDefine(Address);

	RegisterConditional("VC:HAVE:TYPE",      1, conditional_VC_Havetype, CTX_VCARD);
	RegisterFilteredIterator("VC:TYPE", 1, DefineToToken, NULL, NULL, NULL, filter_VC_ByType, CTX_VCARD_TYPE, CTX_VCARD, IT_NOFLAG);
	RegisterFilteredIterator("VC:TYPE:ITEMS", 0, NULL, getContextVcard, NULL, NULL, filter_VC_ByContextType, CTX_STRBUF, CTX_VCARD_TYPE, IT_NOFLAG);

	RegisterMimeRenderer(HKEY("text/x-vcard"), render_MIME_VCard, 1, 201);
	RegisterMimeRenderer(HKEY("text/vcard"), render_MIME_VCard, 1, 200);
}
