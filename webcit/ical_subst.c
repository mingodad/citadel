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
CtxType CTX_ICALATTENDEE = CTX_NONE;
CtxType CTX_ICALCONFLICT = CTX_NONE;
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
		t = (struct icaltimetype *) &DynamicTP[1];
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



void render_MIME_ICS_TPL(StrBuf *Target, WCTemplputParams *TP, StrBuf *FoundCharset)
{
	wc_mime_attachment *Mime = CTX(CTX_MIME_ATACH);
	icalproperty_method the_method = ICAL_METHOD_NONE;
	icalproperty *method = NULL;
	icalcomponent *cal = NULL;
	icalcomponent *c = NULL;
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

	StackContext (TP,
		      &SuperTP,
		      &the_method,
		      CTX_ICALMETHOD,
		      0,
		      TP->Tokens);

	StackContext (&SuperTP, 
		      &SubTP, 
		      c,
		      CTX_ICAL,
		      0,
		      SuperTP.Tokens);
	FlushStrBuf(Mime->Data);
///	DoTemplate(HKEY("ical_attachment_display"), Mime->Data, &SubTP);
	DoTemplate(HKEY("ical_edit"), Mime->Data, &SubTP);

	/*/ cal_process_object(Mime->Data, cal, 0, Mime->msgnum, ChrPtr(Mime->PartNum)); */

	/* Free the memory we obtained from libical's constructor */
	StrBufPlain(Mime->ContentType, HKEY("text/html"));
	StrBufAppendPrintf(WC->trailing_javascript,
		"eventEditAllDay();		\n"
		"RecurrenceShowHide();		\n"
		"EnableOrDisableCheckButton();	\n"
	);

	UnStackContext(&SuperTP);
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


typedef struct CalendarConflict
{
	long is_update;
	long existing_msgnum;
	StrBuf *conflict_event_uid;
	StrBuf *conflict_event_summary;
}CalendarConflict;
void DeleteConflict(void *vConflict)
{
	CalendarConflict *c = (CalendarConflict *) vConflict;

	FreeStrBuf(&c->conflict_event_uid);
	FreeStrBuf(&c->conflict_event_summary);
	free(c);
}
HashList *iterate_FindConflict(StrBuf *Target, WCTemplputParams *TP)
{
	StrBuf *Line;
	HashList *Conflicts = NULL;
	CalendarConflict *Conflict;
	wc_mime_attachment *Mime = (wc_mime_attachment *) CTX(CTX_MIME_ATACH);

	serv_printf("ICAL conflicts|%ld|%s|", Mime->msgnum, ChrPtr(Mime->PartNum));

	Line = NewStrBuf();
	StrBuf_ServGetln(Line);
	if (GetServerStatus(Line, NULL) == 1)
	{
		const char *Pos = NULL;
		int Done = 0;
		int n = 0;
		Conflicts = NewHash(1, Flathash);
		while(!Done && (StrBuf_ServGetln(Line) >= 0) )
			if ( (StrLength(Line)==3) && 
			     !strcmp(ChrPtr(Line), "000")) 
			{
				Done = 1;
			}
			else {
				Conflict = (CalendarConflict *) malloc(sizeof(CalendarConflict));
				Conflict->conflict_event_uid = NewStrBufPlain(NULL, StrLength(Line));
				Conflict->conflict_event_summary = NewStrBufPlain(NULL, StrLength(Line));

				Conflict->existing_msgnum = StrBufExtractNext_long(Line, &Pos, '|');
				StrBufSkip_NTokenS(Line, &Pos, '|', 1);
				StrBufExtract_NextToken(Conflict->conflict_event_uid, Line, &Pos, '|');
				StrBufExtract_NextToken(Conflict->conflict_event_summary, Line, &Pos, '|');
				Conflict->is_update = StrBufExtractNext_long(Line, &Pos, '|');

				Put(Conflicts, IKEY(n), Conflict, DeleteConflict);
				n++;
				Pos = NULL;
			}
	}
	FreeStrBuf(&Line);
	syslog(LOG_DEBUG, "...done.\n");
	return Conflicts;
}



void tmplput_ConflictEventMsgID(StrBuf *Target, WCTemplputParams *TP)
{
	CalendarConflict *C = (CalendarConflict *) CTX(CTX_ICALCONFLICT);
	char buf[sizeof(long) * 16];

	snprintf(buf, sizeof(buf), "%ld", C->existing_msgnum);
	StrBufAppendTemplateStr(Target, TP, buf, 0);
}
void tmplput_ConflictEUID(StrBuf *Target, WCTemplputParams *TP)
{
	CalendarConflict *C = (CalendarConflict *) CTX(CTX_ICALCONFLICT);
	
	StrBufAppendTemplate(Target, TP, C->conflict_event_uid, 0);
}
void tmplput_ConflictSummary(StrBuf *Target, WCTemplputParams *TP)
{
	CalendarConflict *C = (CalendarConflict *) CTX(CTX_ICALCONFLICT);

	StrBufAppendTemplate(Target, TP, C->conflict_event_summary, 0);
}
int cond_ConflictIsUpdate(StrBuf *Target, WCTemplputParams *TP)
{
	CalendarConflict *C = (CalendarConflict *) CTX(CTX_ICALCONFLICT);
	return C->is_update;
}

typedef struct CalAttendee
{
	StrBuf *AttendeeStr;
	icalparameter_partstat partstat;
} CalAttendee;

void DeleteAtt(void *vAtt)
{
	CalAttendee *att = (CalAttendee*) vAtt;
	FreeStrBuf(&att->AttendeeStr);
	free(vAtt);
}

HashList *iterate_get_ical_attendees(StrBuf *Target, WCTemplputParams *TP)
{
	icalcomponent *cal = (icalcomponent *) CTX(CTX_ICAL);
	icalparameter *partstat_param;
	icalproperty *p;
	CalAttendee *Att;
	HashList *Attendees = NULL;
	const char *ch;
	int n = 0;

	/* If the component has attendees, iterate through them. */
	for (p = icalcomponent_get_first_property(cal, ICAL_ATTENDEE_PROPERTY); 
	     (p != NULL); 
	     p = icalcomponent_get_next_property(cal, ICAL_ATTENDEE_PROPERTY)) {
		ch = icalproperty_get_attendee(p);
		if ((ch != NULL) && !strncasecmp(ch, "MAILTO:", 7)) {
			Att = (CalAttendee*) malloc(sizeof(CalAttendee));

			/** screen name or email address */
			Att->AttendeeStr = NewStrBufPlain(ch + 7, -1);
			StrBufTrim(Att->AttendeeStr);

			/** participant status */
			partstat_param = icalproperty_get_first_parameter(
				p,
				ICAL_PARTSTAT_PARAMETER
				);
			if (partstat_param == NULL) {
				Att->partstat = ICAL_PARTSTAT_X;
			}
			else {
				Att->partstat = icalparameter_get_partstat(partstat_param);
			}
			if (Attendees == NULL)
				Attendees = NewHash(1, Flathash);
			Put(Attendees, IKEY(n), Att, DeleteAtt);
			n++;
		}
	}
	return Attendees;
}

void tmplput_ICalAttendee(StrBuf *Target, WCTemplputParams *TP)
{
	CalAttendee *Att = (CalAttendee*) CTX(CTX_ICALATTENDEE);
	StrBufAppendTemplate(Target, TP, Att->AttendeeStr, 0);
}
int cond_ICalAttendeeState(StrBuf *Target, WCTemplputParams *TP)
{
	CalAttendee *Att = (CalAttendee*) CTX(CTX_ICALATTENDEE);
	icalparameter_partstat which_partstat;

	which_partstat = GetTemplateTokenNumber(Target, TP, 2, ICAL_PARTSTAT_X);
	return Att->partstat == which_partstat;
}
	/* If the component has subcomponents, recurse through them. * /
	for (c = icalcomponent_get_first_component(cal, ICAL_ANY_COMPONENT);
	     (c != 0);
	     c = icalcomponent_get_next_component(cal, ICAL_ANY_COMPONENT)) {
		// Recursively process subcomponent
		cal_process_object(Target, c, recursion_level+1, msgnum, cal_partnum);
	}
	*/


void 
InitModule_ICAL_SUBST
(void)
{
	RegisterCTX(CTX_ICAL);
/*
	RegisterMimeRenderer(HKEY("text/calendar"), render_MIME_ICS_TPL, 1, 501);
	RegisterMimeRenderer(HKEY("application/ics"), render_MIME_ICS_TPL, 1, 500);
*/

	CreateIcalComponendKindLookup ();
 	RegisterConditional("COND:ICAL:PROPERTY", 1, cond_ICalHaveItem, CTX_ICAL);
 	RegisterConditional("COND:ICAL:IS:A", 1, cond_ICalIsA, CTX_ICAL);


        RegisterIterator("ICAL:CONFLICT", 0, NULL, iterate_FindConflict, 
                         NULL, DeleteHash, CTX_MIME_ATACH, CTX_ICALCONFLICT, IT_NOFLAG);
	RegisterNamespace("ICAL:CONFLICT:MSGID", 0, 1, tmplput_ConflictEventMsgID, NULL, CTX_ICALCONFLICT);
	RegisterNamespace("ICAL:CONFLICT:EUID", 0, 1, tmplput_ConflictEUID, NULL, CTX_ICALCONFLICT);
	RegisterNamespace("ICAL:CONFLICT:SUMMARY", 0, 1, tmplput_ConflictSummary, NULL, CTX_ICALCONFLICT);
	RegisterConditional("ICAL:CONFLICT:IS:UPDATE", 0, cond_ConflictIsUpdate, CTX_ICALCONFLICT);


	RegisterCTX(CTX_ICALATTENDEE);
        RegisterIterator("ICAL:ATTENDEES", 0, NULL, iterate_get_ical_attendees, 
                         NULL, DeleteHash, CTX_ICALATTENDEE, CTX_ICAL, IT_NOFLAG);
	RegisterNamespace("ICAL:ATTENDEE", 1, 2, tmplput_ICalAttendee, NULL, CTX_ICALATTENDEE);
 	RegisterConditional("COND:ICAL:ATTENDEE", 1, cond_ICalAttendeeState, CTX_ICALATTENDEE);

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
