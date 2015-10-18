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


HashList *DefineToToken = NULL;
HashList *VCTokenToDefine = NULL;
HashList *vcNames = NULL; /* todo: fill with the name strings */
vcField* vcfUnknown = NULL;

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
	HashList *vc = (HashList*) CTX(CTX_VCARD);
	if (GetHash(vc, LKEY(searchFieldNo), &vItem) && (vItem != NULL)) {
		StrBufAppendTemplate(Target, TP, (StrBuf*) vItem, 1);
	}
}

void tmpl_vcard_context_item(StrBuf *Target, WCTemplputParams *TP)
{
	void *vItem;
	vcField *t = (vcField*) CTX(CTX_VCARD_TYPE);
	HashList *vc = (HashList*) CTX(CTX_VCARD);

	if (t == NULL) {
		LogTemplateError(NULL, "VCard item", ERR_NAME, TP,
				 "Missing context");
		return;
	}

	if (GetHash(vc, LKEY(t->cval), &vItem) && (vItem != NULL)) {
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
		HashList *vc = (HashList*) CTX(CTX_VCARD);
		if (GetHash(vc, LKEY(vf->cval), &v) && v != NULL)
			return 1;
	}
	return rc;
}




HashList *getContextVcard(StrBuf *Target, WCTemplputParams *TP)
{
	vcField *vf = (vcField*) CTX(CTX_VCARD_TYPE);
	HashList *vc = (HashList*) CTX(CTX_VCARD);

	if ((vf == NULL) || (vc == NULL)) {
		LogTemplateError(NULL, "VCard item type", ERR_NAME, TP,
				 "Need VCard and Vcard type in context");
		
		return NULL;
	}
	return vc;
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
	HashList *vc = (HashList*) CTX(CTX_VCARD);
	long HaveFieldType = GetTemplateTokenNumber(Target, TP, 2, 0);
	int rc = 0;	
	void *vVCitem;
	const char *Key;
	long len;
	HashPos *it = GetNewHashPos(vc, 0);
	while (GetNextHashPos(vc, it, &len, &Key, &vVCitem) && 
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

/*
 * Record compare function for sorting address book indices
 */
int abcmp(const void *ab1, const void *ab2) {
	return(strcasecmp(
		(((const addrbookent *)ab1)->ab_name),
		(((const addrbookent *)ab2)->ab_name)
	));
}


/*
 * Helper function for do_addrbook_view()
 * Converts a name into a three-letter tab label
 */
void nametab(char *tabbuf, long len, char *name) {
	stresc(tabbuf, len, name, 0, 0);
	tabbuf[0] = toupper(tabbuf[0]);
	tabbuf[1] = tolower(tabbuf[1]);
	tabbuf[2] = tolower(tabbuf[2]);
	tabbuf[3] = 0;
}


/*
 * If it's an old "Firstname Lastname" style record, try to convert it.
 */
void lastfirst_firstlast(char *namebuf) {
	char firstname[SIZ];
	char lastname[SIZ];
	int i;

	if (namebuf == NULL) return;
	if (strchr(namebuf, ';') != NULL) return;

	i = num_tokens(namebuf, ' ');
	if (i < 2) return;

	extract_token(lastname, namebuf, i-1, ' ', sizeof lastname);
	remove_token(namebuf, i-1, ' ');
	strcpy(firstname, namebuf);
	sprintf(namebuf, "%s; %s", lastname, firstname);
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
 * fetch the display name off a vCard
 */
void fetch_ab_name(message_summary *Msg, char **namebuf) {
	long len;
	int i;
	wc_mime_attachment *VCMime = NULL;

	if (namebuf == NULL) return;

	VCMime = load_vcard(Msg);
	if (VCMime == NULL)
		return;

	/* Grab the name off the card */
	display_vcard(WC->WBuf, VCMime, 0, 0, namebuf, Msg->msgnum);

	if (*namebuf != NULL) {
		lastfirst_firstlast(*namebuf);
		striplt(*namebuf);
		len = strlen(*namebuf);
		for (i=0; i<len; ++i) {
			if ((*namebuf)[i] != ';') return;
		}
		free (*namebuf);
		(*namebuf) = strdup(_("(no name)"));
	}
	else {
		(*namebuf) = strdup(_("(no name)"));
	}
}



/*
 * Turn a vCard "n" (name) field into something displayable.
 */
void vcard_n_prettyize(char *name)
{
	char *original_name;
	int i, j, len;

	original_name = strdup(name);
	len = strlen(original_name);
	for (i=0; i<5; ++i) {
		if (len > 0) {
			if (original_name[len-1] == ' ') {
				original_name[--len] = 0;
			}
			if (original_name[len-1] == ';') {
				original_name[--len] = 0;
			}
		}
	}
	strcpy(name, "");
	j=0;
	for (i=0; i<len; ++i) {
		if (original_name[i] == ';') {
			name[j++] = ',';
			name[j++] = ' ';			
		}
		else {
			name[j++] = original_name[i];
		}
	}
	name[j] = '\0';
	free(original_name);
}




/*
 * preparse a vcard name
 * display_vcard() calls this after parsing the textual vCard into
 * our 'struct vCard' data object.
 * This gets called instead of display_parsed_vcard() if we are only looking
 * to extract the person's name instead of displaying the card.
 */
void fetchname_parsed_vcard(struct vCard *v, char **storename) {
	char *name;
	char *prop;
	char buf[SIZ];
	int j, n, len;
	int is_qp = 0;
	int is_b64 = 0;

	*storename = NULL;

	name = vcard_get_prop(v, "n", 1, 0, 0);
	if (name != NULL) {
		len = strlen(name);
		prop = vcard_get_prop(v, "n", 1, 0, 1);
		n = num_tokens(prop, ';');

		for (j=0; j<n; ++j) {
			extract_token(buf, prop, j, ';', sizeof buf);
			if (!strcasecmp(buf, "encoding=quoted-printable")) {
				is_qp = 1;
			}
			if (!strcasecmp(buf, "encoding=base64")) {
				is_b64 = 1;
			}
		}
		if (is_qp) {
			/* %ff can become 6 bytes in utf8  */
			*storename = malloc(len * 2 + 3); 
			j = CtdlDecodeQuotedPrintable(
				*storename, name,
				len);
			(*storename)[j] = 0;
		}
		else if (is_b64) {
			/* ff will become one byte.. */
			*storename = malloc(len + 50);
			CtdlDecodeBase64(
				*storename, name,
				len);
		}
		else {
			size_t len;

			len = strlen (name);
			
			*storename = malloc(len + 3); /* \0 + eventualy missing ', '*/
			memcpy(*storename, name, len + 1);
		}
		/* vcard_n_prettyize(storename); */
	}

}




void PutVcardItem(HashList *thisVC, vcField *thisField, StrBuf *ThisFieldStr, int is_qp, StrBuf *Swap)
{
	/* if we have some untagged QP, detect it here. */
	if (is_qp || (strstr(ChrPtr(ThisFieldStr), "=?")!=NULL)){
		StrBuf *b;
		StrBuf_RFC822_to_Utf8(Swap, ThisFieldStr, NULL, NULL); /* default charset, current charset */
		b = ThisFieldStr;
		ThisFieldStr = Swap; 
		Swap = b;
		FlushStrBuf(Swap);
	}
	Put(thisVC, LKEY(thisField->cval), ThisFieldStr, HFreeStrBuf);
}
/*
 * html print a vcard
 * display_vcard() calls this after parsing the textual vCard into
 * our 'struct vCard' data object.
 *
 * Set 'full' to nonzero to display the full card, otherwise it will only
 * show a summary line.
 *
 * This code is a bit ugly, so perhaps an explanation is due: we do this
 * in two passes through the vCard fields.  On the first pass, we process
 * fields we understand, and then render them in a pretty fashion at the
 * end.  Then we make a second pass, outputting all the fields we don't
 * understand in a simple two-column name/value format.
 * v		the vCard to parse
 * msgnum	Citadel message pointer
 */
void parse_vcard(StrBuf *Target, struct vCard *v, HashList *VC, wc_mime_attachment *Mime)
{
	StrBuf *Val = NULL;
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
			///int evc[10];
			
			len = extract_token(buf, ChrPtr(thisname), j, ';', sizeof buf);
			if (!strcasecmp(buf, "encoding=quoted-printable")) {
				is_qp = 1;
/*				remove_token(thisname, j, ';');*/
			}
			else if (!strcasecmp(buf, "encoding=base64")) {
				is_b64 = 1;
/*				remove_token(thisname, j, ';');*/
			}
			else{
				if (StrLength(thisVCToken) > 0) {
					StrBufAppendBufPlain(thisVCToken, HKEY(";"), 0);
				}
				StrBufAppendBufPlain(thisVCToken, buf, len, 0);
				/*
				if (GetHash(VCToEnum, buf, len, &V))
				{
					evc[k] = (int) V;

					Put(VC, IKEY(evc), Val, HFreeStrBuf);

					syslog(LOG_DEBUG, "[%ul] -> k: %d %s - %s", evc, k, buf, VCStr[evc[k]].Key);
					k++;
				}
*/

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

		/* copy over the payload into a StrBuf */
		Val = NewStrBufPlain(v->prop[i].value, -1);
			
		/* if we have some untagged QP, detect it here. */
		if (is_qp || (strstr(v->prop[i].value, "=?")!=NULL)){
			StrBuf *b;
			StrBuf_RFC822_to_Utf8(Swap, Val, NULL, NULL); /* default charset, current charset */
			b = Val;
			Val = Swap; 
			Swap = b;
			FlushStrBuf(Swap);
		}
		else if (is_b64) {
			StrBufDecodeBase64(Val);
		}
#if 0
		syslog(LOG_DEBUG, "-> firsttoken: %s thisname: %s Value: [%s][%s]",
			firsttoken,
		       ChrPtr(thisname),
			ChrPtr(Val),
			v->prop[i].value);
		if (GetHash(VCToEnum, firsttoken, strlen(firsttoken), &V))
		{
			eVC evc = (eVC) V;
			Put(VC, IKEY(evc), Val, HFreeStrBuf);
			syslog(LOG_DEBUG, "[%ul]\n", evc);
			Val = NULL;
		}
		else
			syslog(LOG_DEBUG, "[]\n");
/*
TODO: check for layer II
		else 
		{
			long max = num_tokens(thisname, ';');
			firsttoken[len] = '_';

			for (j = 0; j < max; j++) {
//			firsttoken[len]

				extract_token(buf, thisname, j, ';', sizeof (buf));
					if (!strcasecmp(buf, "tel"))
						strcat(phone, "");
					else if (!strcasecmp(buf, "work"))
						strcat(phone, _(" (work)"));
					else if (!strcasecmp(buf, "home"))
						strcat(phone, _(" (home)"));
					else if (!strcasecmp(buf, "cell"))
						strcat(phone, _(" (cell)"));
					else {
						strcat(phone, " (");
						strcat(phone, buf);
						strcat(phone, ")");
					}
				}
			}

		}
*/
#endif	
		FreeStrBuf(&Val);
	}
	FreeStrBuf(&thisname);
	FreeStrBuf(&Swap);
	FreeStrBuf(&thisVCToken);
}

void tmplput_VCARD_ITEM(StrBuf *Target, WCTemplputParams *TP)
{
	HashList *VC = CTX(CTX_VCARD);
	int evc;
	void *vStr;

	evc = GetTemplateTokenNumber(Target, TP, 0, -1);
	if (evc != -1)
	{
		if (GetHash(VC, IKEY(evc), &vStr))
		{
			StrBufAppendTemplate(Target, TP,
					     (StrBuf*) vStr,
					     1);
		}
	}
	
}

void display_one_vcard (StrBuf *Target, struct vCard *v, int full, wc_mime_attachment *Mime)
{
	HashList *VC;	WCTemplputParams SubTP;

        memset(&SubTP, 0, sizeof(WCTemplputParams));    


	VC = NewHash(0, lFlathash);
	parse_vcard(Target, v, VC, Mime);

	{
		WCTemplputParams *TP = NULL;
		WCTemplputParams SubTP;
		StackContext(TP, &SubTP, VC, CTX_VCARD, 0, NULL);

		DoTemplate(HKEY("vcard_msg_display"), Target, &SubTP);
		UnStackContext(&SubTP);
	}
	DeleteHash(&VC);
}



/*
 * Display a textual vCard
 * (Converts to a vCard object and then calls the actual display function)
 * Set 'full' to nonzero to display the whole card instead of a one-liner.
 * Or, if "storename" is non-NULL, just store the person's name in that
 * buffer instead of displaying the card at all.
 *
 * vcard_source	the buffer containing the vcard text
 * alpha	Display only if name begins with this letter of the alphabet
 * full		Display the full vCard (otherwise just the display name)
 * storename	If not NULL, also store the display name here
 * msgnum	Citadel message pointer
 */
void display_vcard(StrBuf *Target, 
		   wc_mime_attachment *Mime, 
		   char alpha, 
		   int full, 
		   char **storename, 
		   long msgnum) 
{
	struct vCard *v;
	char *name;
	StrBuf *Buf;
	StrBuf *Buf2;
	char this_alpha = 0;

	v = VCardLoad(Mime->Data);

	if (v == NULL) return;

	name = vcard_get_prop(v, "n", 1, 0, 0);
	if (name != NULL) {
		Buf = NewStrBufPlain(name, -1);
		Buf2 = NewStrBufPlain(NULL, StrLength(Buf));
		StrBuf_RFC822_to_Utf8(Buf2, Buf, WC->DefaultCharset, NULL);
		this_alpha = ChrPtr(Buf)[0];
		FreeStrBuf(&Buf);
		FreeStrBuf(&Buf2);
	}

	if (storename != NULL) {
		fetchname_parsed_vcard(v, storename);
	}
	else if ((alpha == 0) || 
		 ((isalpha(alpha)) && (tolower(alpha) == tolower(this_alpha))) || 
		 ((!isalpha(alpha)) && (!isalpha(this_alpha)))
		) 
	{
		display_one_vcard (Target, v, full, Mime);
	}

	vcard_free(v);
}



/*
 * Render the address book using info we gathered during the scan
 *
 * addrbook	the addressbook to render
 * num_ab	the number of the addressbook
 */
void do_addrbook_view(addrbookent *addrbook, int num_ab) {
	int i = 0;
	int displayed = 0;
	int bg = 0;
	static int NAMESPERPAGE = 60;
	int num_pages = 0;
	int tabfirst = 0;
	char tabfirst_label[64];
	int tablast = 0;
	char tablast_label[64];
	char this_tablabel[64];
	int page = 0;
	char **tablabels;

	if (num_ab == 0) {
		wc_printf("<br><br><br><div align=\"center\"><i>");
		wc_printf(_("This address book is empty."));
		wc_printf("</i></div>\n");
		return;
	}

	if (num_ab > 1) {
		qsort(addrbook, num_ab, sizeof(addrbookent), abcmp);
	}

	num_pages = (num_ab / NAMESPERPAGE) + 1;

	tablabels = malloc(num_pages * sizeof (char *));
	if (tablabels == NULL) {
		wc_printf("<br><br><br><div align=\"center\"><i>");
		wc_printf(_("An internal error has occurred."));
		wc_printf("</i></div>\n");
		return;
	}

	for (i=0; i<num_pages; ++i) {
		tabfirst = i * NAMESPERPAGE;
		tablast = tabfirst + NAMESPERPAGE - 1;
		if (tablast > (num_ab - 1)) tablast = (num_ab - 1);
		nametab(tabfirst_label, 64, addrbook[tabfirst].ab_name);
		nametab(tablast_label, 64, addrbook[tablast].ab_name);
		sprintf(this_tablabel, "%s&nbsp;-&nbsp;%s", tabfirst_label, tablast_label);
		tablabels[i] = strdup(this_tablabel);
	}

	tabbed_dialog(num_pages, tablabels);
	page = (-1);

	for (i=0; i<num_ab; ++i) {

		if ((i / NAMESPERPAGE) != page) {	/* New tab */
			page = (i / NAMESPERPAGE);
			if (page > 0) {
				wc_printf("</tr></table>\n");
				end_tab(page-1, num_pages);
			}
			begin_tab(page, num_pages);
			wc_printf("<table border=\"0\" cellspacing=\"0\" cellpadding=\"3\" width=\"100%%\">\n");
			displayed = 0;
		}

		if ((displayed % 4) == 0) {
			if (displayed > 0) {
				wc_printf("</tr>\n");
			}
			bg = 1 - bg;
			wc_printf("<tr bgcolor=\"#%s\">",
				(bg ? "dddddd" : "ffffff")
			);
		}
	
		wc_printf("<td>");

		wc_printf("<a href=\"readfwd?startmsg=%ld?is_singlecard=1",
			addrbook[i].ab_msgnum);
		wc_printf("?maxmsgs=1?is_summary=0?alpha=%s\">", bstr("alpha"));
		vcard_n_prettyize(addrbook[i].ab_name);
		escputs(addrbook[i].ab_name);
		wc_printf("</a></td>\n");
		++displayed;
	}

	/* Placeholders for empty columns at end */
	if ((num_ab % 4) != 0) {
		for (i=0; i<(4-(num_ab % 4)); ++i) {
			wc_printf("<td>&nbsp;</td>");
		}
	}

	wc_printf("</tr></table>\n");
	end_tab((num_pages-1), num_pages);

	begin_tab(num_pages, num_pages);
	/* FIXME there ought to be something here */
	end_tab(num_pages, num_pages);

	for (i=0; i<num_pages; ++i) {
		free(tablabels[i]);
	}
	free(tablabels);
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
	HashList *VC;	WCTemplputParams SubTP;
	wcsession *WCC = WC;
	message_summary *Msg = NULL;
	wc_mime_attachment *VCMime = NULL;
	struct vCard *v;
	char whatuser[256];
	VC = NewHash(0, lFlathash);

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
				convenience_page("770000", _("Error"), "");///TODO: important message
				DestroyMessageSummary(Msg);
				return;
			}
		
			v = VCardLoad(VCMime->Data);
		}
		else {
			v = VCardLoad(VCAtt->Data);
		}

		parse_vcard(WCC->WBuf, v, VC, NULL);
	
	
		vcard_free(v);
	}

        memset(&SubTP, 0, sizeof(WCTemplputParams));    

	{
		WCTemplputParams *TP = NULL;
		WCTemplputParams SubTP;
		StackContext(TP, &SubTP, VC, CTX_VCARD, 0, NULL);

		DoTemplate(HKEY("vcard_edit"), WCC->WBuf, &SubTP);
		UnStackContext(&SubTP);
	}
	DeleteHash(&VC);


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
		return;//// more details
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
			/*
		    (vvcField != NULL))
		{
			vcField *t = (vcField*) vvcField;
			if (t->layer == 0) switch (t->Type) {
				break;
			case StringCluster:
			{
				
				i++;
				continue;
			}
			break;
				break;
			case EmailAddr:
				break;
			case Street:
				break;
			case FlatString:
			case PhoneNumber:
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

			}
*/
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



/*
 * Extract an embedded photo from a vCard for display on the client
 */
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

typedef struct _vcardview_struct {
	long is_singlecard;
	addrbookent *addrbook;
	long num_ab;

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
	vcardview_struct *VS;
	char *ab_name;

	VS = (vcardview_struct*) *ViewSpecific;

	ab_name = NULL;
	fetch_ab_name(Msg, &ab_name);
	if (ab_name == NULL) 
		return 0;
	++VS->num_ab;
	VS->addrbook = realloc(VS->addrbook,
			       (sizeof(addrbookent) * VS->num_ab) );
	safestrncpy(VS->addrbook[VS->num_ab-1].ab_name, ab_name,
		    sizeof(VS->addrbook[VS->num_ab-1].ab_name));
	VS->addrbook[VS->num_ab-1].ab_msgnum = Msg->msgnum;
	free(ab_name);
	return 0;
}


int vcard_RenderView_or_Tail(SharedMessageStatus *Stat, void **ViewSpecific, long oper)
{
	const StrBuf *Mime;
	vcardview_struct *VS;

	VS = (vcardview_struct*) *ViewSpecific;
	if (VS->is_singlecard)
		read_message(WC->WBuf, HKEY("view_message"), lbstr("startmsg"), NULL, &Mime);
	else
		do_addrbook_view(VS->addrbook, VS->num_ab);	/* Render the address book */
	return 0;
}

int vcard_Cleanup(void **ViewSpecific)
{
	vcardview_struct *VS;

	VS = (vcardview_struct*) *ViewSpecific;
	wDumpContent(1);
	if ((VS != NULL) && 
	    (VS->addrbook != NULL))
		free(VS->addrbook);
	if (VS != NULL) 
		free(VS);
	return 0;
}

void 
ServerStartModule_VCARD
(void)
{
	///VCToEnum = NewHash(0, NULL);

}

void 
ServerShutdownModule_VCARD
(void)
{
	DeleteHash(&DefineToToken);
	DeleteHash(&vcNames);
	DeleteHash(&VCTokenToDefine);
	/// DeleteHash(&VCToEnum);
}

void 
InitModule_VCARD
(void)
{
	RegisterCTX(CTX_VCARD);
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
	WebcitAddUrlHandler(HKEY("edit_vcard"), "", 0, edit_vcard, 0);
	WebcitAddUrlHandler(HKEY("submit_vcard"), "", 0, submit_vcard, 0);
	WebcitAddUrlHandler(HKEY("vcardphoto"), "", 0, display_vcard_photo_img, NEED_URL);
/*
	Put(VCToEnum, HKEY("n"), (void*)VC_n, reference_free_handler);
	Put(VCToEnum, HKEY("fn"), (void*)VC_fn, reference_free_handler);
	Put(VCToEnum, HKEY("title"), (void*)VC_title, reference_free_handler);
	Put(VCToEnum, HKEY("org"), (void*)VC_org, reference_free_handler);
	Put(VCToEnum, HKEY("email"), (void*)VC_email, reference_free_handler);
	Put(VCToEnum, HKEY("tel"), (void*)VC_tel, reference_free_handler);
	Put(VCToEnum, HKEY("work"), (void*)VC_work, reference_free_handler);
	Put(VCToEnum, HKEY("home"), (void*)VC_home, reference_free_handler);
	Put(VCToEnum, HKEY("cell"), (void*)VC_cell, reference_free_handler);
	Put(VCToEnum, HKEY("adr"), (void*)VC_adr, reference_free_handler);
	Put(VCToEnum, HKEY("photo"), (void*)VC_photo, reference_free_handler);
	Put(VCToEnum, HKEY("version"), (void*)VC_version, reference_free_handler);
	Put(VCToEnum, HKEY("rev"), (void*)VC_rev, reference_free_handler);
	Put(VCToEnum, HKEY("label"), (void*)VC_label, reference_free_handler);
*/
/*
	RegisterNamespace("VC", 1, 2, tmplput_VCARD_ITEM, NULL, CTX_VCARD);

	REGISTERTokenParamDefine(VC_n);
	REGISTERTokenParamDefine(VC_fn);
	REGISTERTokenParamDefine(VC_title);
	REGISTERTokenParamDefine(VC_org);
	REGISTERTokenParamDefine(VC_email);
	REGISTERTokenParamDefine(VC_tel);
	REGISTERTokenParamDefine(VC_work);
	REGISTERTokenParamDefine(VC_home);
	REGISTERTokenParamDefine(VC_cell);
	REGISTERTokenParamDefine(VC_adr);
	REGISTERTokenParamDefine(VC_photo);
	REGISTERTokenParamDefine(VC_version);
	REGISTERTokenParamDefine(VC_rev);
	REGISTERTokenParamDefine(VC_label);
*/

	{
		StrBuf *Prefix  = NewStrBufPlain(HKEY("VC:"));
		DefineToToken   = NewHash(1, lFlathash);
		vcNames         = NewHash(1, lFlathash);
		VCTokenToDefine = NewHash(1, NULL);
		autoRegisterTokens(&VCEnumCounter, VCStrE, Prefix, 0, 0);
		FreeStrBuf(&Prefix);
	}
	RegisterCTX(CTX_VCARD);
	RegisterNamespace("VC:ITEM", 2, 2, tmpl_vcard_item, preeval_vcard_item, CTX_VCARD);
	RegisterNamespace("VC:CTXITEM", 1, 1, tmpl_vcard_context_item, NULL, CTX_VCARD_TYPE);
	RegisterNamespace("VC:NAME", 1, 1, tmpl_vcard_name_str, preeval_vcard_name_str, CTX_VCARD);
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
}

