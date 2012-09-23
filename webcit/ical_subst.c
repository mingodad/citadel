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

extern IcalEnumMap icalproperty_kind_map[];

HashList *IcalComponentMap = NULL;
CtxType CTX_ICAL = CTX_NONE;
#if 0
void SortPregetMatter(HashList *Cals)
{
	disp_cal *Cal;
	void *vCal;
	const char *Key;
        long KLen;
	IcalEnumMap *SortMap[10];
	IcalEnumMap *Map;
	void *vSort;
	const char *Next = NULL;
	const StrBuf *SortVector;
	StrBuf *SortBy;
	int i = 0;
	HashPos *It;

	SortVector = SBSTR("ICALSortVec");
	if (SortVector == NULL)
		return;

	for (i = 0; i < 10; i++) SortMap[i] = NULL;
	SortBy = NewStrBuf();
	while (StrBufExtract_NextToken(SortBy, SortVector, &Next, ':') > 0) {
		GetHash(IcalComponentMap, SKEY(SortBy), &vSort);
		Map = (IcalEnumMap*) vSort;
		SortMap[i] = Map;
		i++;
		if (i > 9)
			break;
	}

	if (i == 0)
		return;

	switch (SortMap[i - 1]->map) {
		///	case 

	default:
		break;
	}

	It = GetNewHashPos(Cals, 0);
	while (GetNextHashPos(Cals, It, &KLen, &Key, &vCal)) {
		i = 0;
		Cal = (disp_cal*) vCal;
		Cal->Status = icalcomponent_get_status(Cal->cal);
		Cal->SortBy = Cal->cal;
		

		while ((SortMap[i] != NULL) && 
		       (Cal->SortBy != NULL)) 
		{
			/****Cal->SortBy = icalcomponent_get_first_property(Cal->SortBy, SortMap[i++]->map); */
		}
	}
}
#endif


void tmplput_ICalItem(StrBuf *Target, WCTemplputParams *TP)
{
	icalcomponent *cal = (icalcomponent *) CTX(CTX_ICAL);
	icalproperty *p;
	icalproperty_kind Kind;
	const char *str;

	Kind = (icalproperty_kind) GetTemplateTokenNumber(Target, TP, 0, ICAL_ANY_PROPERTY);
	p = icalcomponent_get_first_property(cal, Kind);
	if (p != NULL) {
		str = icalproperty_get_comment (p);
		StrBufAppendTemplateStr(Target, TP, str, 1);
	}
}

void tmplput_ICalDate(StrBuf *Target, WCTemplputParams *TP)
{
	icalcomponent *cal = (icalcomponent *) CTX(CTX_ICAL);
	icalproperty *p;
	icalproperty_kind Kind;
	struct icaltimetype t;
	time_t tt;
	char buf[256];

	Kind = (icalproperty_kind) GetTemplateTokenNumber(Target, TP, 0, ICAL_ANY_PROPERTY);
	p = icalcomponent_get_first_property(cal, Kind);
	if (p != NULL) {
		long len;
		t = icalproperty_get_dtend(p);
		tt = icaltime_as_timet(t);
		len = webcit_fmt_date(buf, 256, tt, DATEFMT_FULL);
		StrBufAppendBufPlain(Target, buf, len, 0);
	}
}



void render_MIME_ICS_TPL(wc_mime_attachment *Mime, StrBuf *RawData, StrBuf *FoundCharset)
{
	icalcomponent *cal;
	icalcomponent *c;
        WCTemplputParams SubTP;


	if (StrLength(Mime->Data) == 0) {
		MimeLoadData(Mime);
	}
	if (StrLength(Mime->Data) > 0) {
		cal = icalcomponent_new_from_string(ChrPtr(Mime->Data));
	}
	if (cal == NULL) {
		StrBufAppendPrintf(Mime->Data, _("There was an error parsing this calendar item."));
		StrBufAppendPrintf(Mime->Data, "<br>\n");
		return;
	}

        memset(&SubTP, 0, sizeof(WCTemplputParams));
        SubTP.Filter.ContextType = CTX_ICAL;

	///ical_dezonify(cal);

	/* If the component has subcomponents, recurse through them. */
	c = icalcomponent_get_first_component(cal, ICAL_ANY_COMPONENT);

        SubTP.Context = (c != NULL) ? c : cal;

	FlushStrBuf(Mime->Data);
	DoTemplate(HKEY("ical_attachment_display"), Mime->Data, &SubTP);

	// cal_process_object(Mime->Data, cal, 0, Mime->msgnum, ChrPtr(Mime->PartNum));

	/* Free the memory we obtained from libical's constructor */
	icalcomponent_free(cal);
}
void CreateIcalComponendKindLookup(void)
{
	int i = 0;

	IcalComponentMap = NewHash (1, NULL);
	while (icalproperty_kind_map[i].NameLen != 0) {
		RegisterNS(icalproperty_kind_map[i].Name, 
			   icalproperty_kind_map[i].NameLen, 
			   0, 
			   10, 
			   tmplput_ICalItem,
			   NULL, 
			   CTX_ICAL);
		Put(IcalComponentMap, 
		    icalproperty_kind_map[i].Name, 
		    icalproperty_kind_map[i].NameLen, 
		    &icalproperty_kind_map[i],
		    reference_free_handler);
			   
			   
		i++;
	}
}
















void 
InitModule_ICAL_SUBST
(void)
{
	int i;
	for (i=0; icalproperty_kind_map[i].NameLen > 0; i++)
		RegisterTokenParamDefine (
			icalproperty_kind_map[i].Name,
			icalproperty_kind_map[i].NameLen,
			icalproperty_kind_map[i].map);
	

	RegisterCTX(CTX_ICAL);
	RegisterMimeRenderer(HKEY("text/calendar"), render_MIME_ICS_TPL, 1, 501);
	RegisterMimeRenderer(HKEY("application/ics"), render_MIME_ICS_TPL, 1, 500);
	CreateIcalComponendKindLookup ();
	RegisterNamespace("ICAL:ITEM", 1, 2, tmplput_ICalItem, NULL, CTX_ICAL);

	
}

void 
ServerShutdownModule_ICAL
(void)
{
	DeleteHash(&IcalComponentMap);
}
