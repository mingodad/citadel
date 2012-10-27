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

extern IcalKindEnumMap icalproperty_kind_map[];
extern IcalMethodEnumMap icalproperty_method_map[];

HashList *IcalComponentMap = NULL;
CtxType CTX_ICAL = CTX_NONE;
CtxType CTX_ICALPROPERTY = CTX_NONE;
CtxType CTX_ICALMETHOD = CTX_NONE;
CtxType CTX_ICALTIME = CTX_NONE;
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
		/*	case */

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

void tmplput_CtxICalProperty(StrBuf *Target, WCTemplputParams *TP)
{
	icalproperty *p = (icalproperty *) CTX(CTX_ICALPROPERTY);
	const char *str;

	str = icalproperty_get_comment (p);
	StrBufAppendTemplateStr(Target, TP, str, 0);
}

int ReleaseIcalSubCtx(StrBuf *Target, WCTemplputParams *TP)
{
	WCTemplputParams *TPP = TP;
	UnStackContext(TP);
	free(TPP);
	return 0;
}
int cond_ICalIsA(StrBuf *Target, WCTemplputParams *TP)
{
	icalcomponent *cal = (icalcomponent *) CTX(CTX_ICAL);
	icalcomponent_kind c = GetTemplateTokenNumber(Target, TP, 2, ICAL_NO_COMPONENT);
	return icalcomponent_isa(cal) == c;
}

int cond_ICalHaveItem(StrBuf *Target, WCTemplputParams *TP)
{
	icalcomponent *cal = (icalcomponent *) CTX(CTX_ICAL);
	icalproperty *p;
	icalproperty_kind Kind;

	Kind = (icalproperty_kind) GetTemplateTokenNumber(Target, TP, 2, ICAL_ANY_PROPERTY);
	p = icalcomponent_get_first_property(cal, Kind);
	if (p != NULL) {
		WCTemplputParams *DynamicTP;
	
		DynamicTP = (WCTemplputParams*) malloc(sizeof(WCTemplputParams));
		StackDynamicContext (TP, 
				     DynamicTP, 
				     p,
				     CTX_ICALPROPERTY,
				     0,
				     TP->Tokens,
				     ReleaseIcalSubCtx,
				     TP->Tokens->Params[1]->lvalue);

		return 1;
	}
	return 0;
}

int ReleaseIcalTimeCtx(StrBuf *Target, WCTemplputParams *TP)
{
	WCTemplputParams *TPP = TP;

	UnStackContext(TP);
	free(TPP);
	return 0;
}

int cond_ICalHaveTimeItem(StrBuf *Target, WCTemplputParams *TP)
{
	icalcomponent *cal = (icalcomponent *) CTX(CTX_ICAL);
	icalproperty *p;
	icalproperty_kind Kind;

	Kind = (icalproperty_kind) GetTemplateTokenNumber(Target, TP, 2, ICAL_ANY_PROPERTY);
	p = icalcomponent_get_first_property(cal, Kind);
	if (p != NULL) {
		struct icaltimetype *t;
		struct icaltimetype tt;
		WCTemplputParams *DynamicTP;

		DynamicTP = (WCTemplputParams*) malloc(sizeof(WCTemplputParams) + 
						       sizeof(struct icaltimetype));
		t = (struct icaltimetype *) ((char*)DynamicTP) + sizeof(WCTemplputParams);
		memset(&tt, 0, sizeof(struct icaltimetype));
		switch (Kind)
		{
		case ICAL_DTSTART_PROPERTY:
			tt = icalproperty_get_dtstart(p);
			break;
		case ICAL_DTEND_PROPERTY:
			tt = icalproperty_get_dtend(p);
			break;
		default:
			break;
		}
		memcpy(t, &tt, sizeof(struct icaltimetype));

		StackDynamicContext (TP, 
				     DynamicTP, 
				     t,
				     CTX_ICALTIME,
				     0,
				     TP->Tokens,
				     ReleaseIcalTimeCtx,
				     TP->Tokens->Params[1]->lvalue);

		return 1;
	}
	return 0;
}


int cond_ICalTimeIsDate(StrBuf *Target, WCTemplputParams *TP)
{
	struct icaltimetype *t = (struct icaltimetype *) CTX(CTX_ICALTIME);
	return t->is_date;
}

void tmplput_ICalTime_Date(StrBuf *Target, WCTemplputParams *TP)
{
	struct tm d_tm;
	long len;
	char buf[256];
	struct icaltimetype *t = (struct icaltimetype *) CTX(CTX_ICALTIME);

	memset(&d_tm, 0, sizeof d_tm);
	d_tm.tm_year = t->year - 1900;
	d_tm.tm_mon = t->month - 1;
	d_tm.tm_mday = t->day;
	len = wc_strftime(buf, sizeof(buf), "%x", &d_tm);
	StrBufAppendBufPlain(Target, buf, len, 0);
}
void tmplput_ICalTime_Time(StrBuf *Target, WCTemplputParams *TP)
{
	long len;
	char buf[256];
	struct icaltimetype *t = (struct icaltimetype *) CTX(CTX_ICALTIME);
        time_t tt;

	tt = icaltime_as_timet(*t);
	len = webcit_fmt_date(buf, sizeof(buf), tt, DATEFMT_FULL);
	StrBufAppendBufPlain(Target, buf, len, 0);
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

void tmplput_CtxICalPropertyDate(StrBuf *Target, WCTemplputParams *TP)
{
	icalproperty *p = (icalproperty *) CTX(CTX_ICALPROPERTY);
	struct icaltimetype t;
	time_t tt;
	char buf[256];

	long len;
	t = icalproperty_get_dtend(p);
	tt = icaltime_as_timet(t);
	len = webcit_fmt_date(buf, sizeof(buf), tt, DATEFMT_FULL);
	StrBufAppendBufPlain(Target, buf, len, 0);
}



void render_MIME_ICS_TPL(wc_mime_attachment *Mime, StrBuf *RawData, StrBuf *FoundCharset)
{
	icalproperty_method the_method = ICAL_METHOD_NONE;
	icalproperty *method = NULL;
	icalcomponent *cal;
	icalcomponent *c;
        WCTemplputParams SubTP;
        WCTemplputParams SuperTP;
	static int divcount = 0;

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

	putlbstr("divname",  ++divcount);
	putbstr("cal_partnum", NewStrBufDup(Mime->PartNum));
	putlbstr("msgnum", Mime->msgnum);

        memset(&SubTP, 0, sizeof(WCTemplputParams));
        memset(&SuperTP, 0, sizeof(WCTemplputParams));

	/*//ical_dezonify(cal); */

	/* If the component has subcomponents, recurse through them. */
	c = icalcomponent_get_first_component(cal, ICAL_ANY_COMPONENT);
        c = (c != NULL) ? c : cal;

	method = icalcomponent_get_first_property(cal, ICAL_METHOD_PROPERTY);
	if (method != NULL) {
		the_method = icalproperty_get_method(method);
	}

	SuperTP.Context = &the_method;
	SuperTP.Filter.ContextType = CTX_ICALMETHOD,

	StackContext (&SuperTP, 
		      &SubTP, 
		      c,
		      CTX_ICAL,
		      0,
		      SuperTP.Tokens);
	FlushStrBuf(Mime->Data);
	DoTemplate(HKEY("ical_attachment_display"), Mime->Data, &SubTP);

	/*/ cal_process_object(Mime->Data, cal, 0, Mime->msgnum, ChrPtr(Mime->PartNum)); */

	/* Free the memory we obtained from libical's constructor */
	StrBufPlain(Mime->ContentType, HKEY("text/html"));
	StrBufAppendPrintf(WC->trailing_javascript,
		"eventEditAllDay();		\n"
		"RecurrenceShowHide();		\n"
		"EnableOrDisableCheckButton();	\n"
	);

	UnStackContext(&SubTP);
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




int cond_ICalIsMethod(StrBuf *Target, WCTemplputParams *TP)
{
	icalproperty_method *the_method = (icalproperty_method *) CTX(CTX_ICALMETHOD);
	icalproperty_method which_method;

	which_method = GetTemplateTokenNumber(Target, TP, 2, ICAL_METHOD_X);
	return *the_method == which_method;
}







void tmplput_Conflict(StrBuf *Target, WCTemplputParams *TP)
{}

HashList* IterateGetAttendees()
{
/*
	/* If the component has attendees, iterate through them. * /
	for (p = icalcomponent_get_first_property(cal, ICAL_ATTENDEE_PROPERTY); 
	     (p != NULL); 
	     p = icalcomponent_get_next_property(cal, ICAL_ATTENDEE_PROPERTY)) {
		StrBufAppendPrintf(Target, "<dt>");
		StrBufAppendPrintf(Target, _("Attendee:"));
		StrBufAppendPrintf(Target, "</dt><dd>");
		ch = icalproperty_get_attendee(p);
		if ((ch != NULL) && !strncasecmp(buf, "MAILTO:", 7)) {

			/** screen name or email address * /
			safestrncpy(buf, ch + 7, sizeof(buf));
			striplt(buf);
			StrEscAppend(Target, NULL, buf, 0, 0);
			StrBufAppendPrintf(Target, " ");

			/** participant status * /
			partstat_as_string(buf, p);
			StrEscAppend(Target, NULL, buf, 0, 0);
		}
		StrBufAppendPrintf(Target, "</dd>\n");
	}
*/
	return NULL;
	/* If the component has subcomponents, recurse through them. * /
	for (c = icalcomponent_get_first_component(cal, ICAL_ANY_COMPONENT);
	     (c != 0);
	     c = icalcomponent_get_next_component(cal, ICAL_ANY_COMPONENT)) {
		/* Recursively process subcomponent * /
		cal_process_object(Target, c, recursion_level+1, msgnum, cal_partnum);
	}
	*/
}



void 
InitModule_ICAL_SUBST
(void)
{
	RegisterCTX(CTX_ICAL);
//*
	RegisterMimeRenderer(HKEY("text/calendar"), render_MIME_ICS_TPL, 1, 501);
	RegisterMimeRenderer(HKEY("application/ics"), render_MIME_ICS_TPL, 1, 500);
//*/

	CreateIcalComponendKindLookup ();
 	RegisterConditional("COND:ICAL:PROPERTY", 1, cond_ICalHaveItem, CTX_ICAL);
 	RegisterConditional("COND:ICAL:IS:A", 1, cond_ICalIsA, CTX_ICAL);

	RegisterNamespace("ICAL:SERV:CHECK:CONFLICT", 0, 0, tmplput_Conflict, NULL, CTX_ICAL);

	RegisterCTX(CTX_ICALPROPERTY);
	RegisterNamespace("ICAL:ITEM", 1, 2, tmplput_ICalItem, NULL, CTX_ICAL);
	RegisterNamespace("ICAL:PROPERTY:STR", 0, 1, tmplput_CtxICalProperty, NULL, CTX_ICALPROPERTY);
	RegisterNamespace("ICAL:PROPERTY:DATE", 0, 1, tmplput_CtxICalPropertyDate, NULL, CTX_ICALPROPERTY);

	RegisterCTX(CTX_ICALMETHOD);
 	RegisterConditional("COND:ICAL:METHOD", 1, cond_ICalIsMethod, CTX_ICALMETHOD);


	RegisterCTX(CTX_ICALTIME);
 	RegisterConditional("COND:ICAL:DT:PROPERTY", 1, cond_ICalHaveTimeItem, CTX_ICAL);
 	RegisterConditional("COND:ICAL:DT:ISDATE", 0, cond_ICalTimeIsDate, CTX_ICALTIME);
	RegisterNamespace("ICAL:DT:DATE", 0, 1, tmplput_ICalTime_Date, NULL, CTX_ICALTIME);
	RegisterNamespace("ICAL:DT:DATETIME", 0, 1, tmplput_ICalTime_Time, NULL, CTX_ICALTIME);
}

void 
ServerShutdownModule_ICAL
(void)
{
	DeleteHash(&IcalComponentMap);
}




