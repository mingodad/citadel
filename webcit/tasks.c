#include "webcit.h"
#include "calendar.h"
#include "webserver.h"

/*
 * qsort filter to move completed tasks to bottom of task list
 */
int task_completed_cmp(const void *vtask1, const void *vtask2) {
	disp_cal * Task1 = (disp_cal *)GetSearchPayload(vtask1);
/*	disp_cal * Task2 = (disp_cal *)GetSearchPayload(vtask2); */

	icalproperty_status t1 = icalcomponent_get_status((Task1)->cal);
	/* icalproperty_status t2 = icalcomponent_get_status(((struct disp_cal *)task2)->cal); */

	if (t1 == ICAL_STATUS_COMPLETED)
		return 1;
	return 0;
}


/*
 * Helper function for do_tasks_view().  Returns the due date/time of a vtodo.
 */
time_t get_task_due_date(icalcomponent *vtodo, int *is_date) {
	icalproperty *p;

	if (vtodo == NULL) {
		return(0L);
	}

	/*
	 * If we're looking at a fully encapsulated VCALENDAR
	 * rather than a VTODO component, recurse into the data
	 * structure until we get a VTODO.
	 */
	if (icalcomponent_isa(vtodo) == ICAL_VCALENDAR_COMPONENT) {
		return get_task_due_date(
			icalcomponent_get_first_component(
				vtodo, ICAL_VTODO_COMPONENT
				), is_date
			);
	}

	p = icalcomponent_get_first_property(vtodo, ICAL_DUE_PROPERTY);
	if (p != NULL) {
		struct icaltimetype t = icalproperty_get_due(p);

		if (is_date)
			*is_date = t.is_date;
		return(icaltime_as_timet(t));
	}
	else {
		return(0L);
	}
}

/*
 * Compare the due dates of two tasks (this is for sorting)
 */
int task_due_cmp(const void *vtask1, const void *vtask2) {
	disp_cal * Task1 = (disp_cal *)GetSearchPayload(vtask1);
	disp_cal * Task2 = (disp_cal *)GetSearchPayload(vtask2);

	time_t t1;
	time_t t2;

	t1 =  get_task_due_date(Task1->cal, NULL);
	t2 =  get_task_due_date(Task2->cal, NULL);
	if (t1 < t2) return(-1);
	if (t1 > t2) return(1);
	return(0);
}

/*
 * do the whole task view stuff
 */
int tasks_RenderView_or_Tail(SharedMessageStatus *Stat,
			      void **ViewSpecific,
			      long oper)
{
	long hklen;
	const char *HashKey;
	void *vCal;
	disp_cal *Cal;
	HashPos *Pos;
	int nItems;
	time_t due;
	char buf[SIZ];
	icalproperty *p;
	wcsession *WCC = WC;

	wc_printf("<table id=\"task_view_background\"><tbody class=\"taskview\">\n<tr class=\"taskview_headrow\">\n<th class=\"task_completed\">");
	wc_printf(_("Completed?"));
	wc_printf("</th><th class=\"task_name\">");
	wc_printf(_("Name of task"));
	wc_printf("</th><th class=\"task_due_date\">");
	wc_printf(_("Date due"));
	wc_printf("</th><th class=\"task_category\">");
	wc_printf(_("Category"));
	wc_printf(" (<select id=\"selectcategory\"><option value=\"showall\">%s</option></select>)</th></tr>\n",
		_("Show All"));

	nItems = GetCount(WC->disp_cal_items);

	/* Sort them if necessary
	if (nItems > 1) {
		SortByPayload(WC->disp_cal_items, task_due_cmp);
	}
	* this shouldn't be neccessary, since we sort by the start time.
	*/

	/* And then again, by completed */
	if (nItems > 1) {
		SortByPayload(WC->disp_cal_items,
			      task_completed_cmp);
	}

	Pos = GetNewHashPos(WCC->disp_cal_items, 0);
	while (GetNextHashPos(WCC->disp_cal_items, Pos, &hklen, &HashKey, &vCal)) {
		icalproperty_status todoStatus;
		int is_date;

		Cal = (disp_cal*)vCal;
		wc_printf("<tr><td class=\"task_completed\">");
		todoStatus = icalcomponent_get_status(Cal->cal);
		wc_printf("<input type=\"checkbox\" name=\"completed\" value=\"completed\" ");
		if (todoStatus == ICAL_STATUS_COMPLETED) {
			wc_printf("checked=\"checked\" ");
		}
		wc_printf("disabled=\"disabled\">\n</td><td class=\"task_name\">");
		p = icalcomponent_get_first_property(Cal->cal,
			ICAL_SUMMARY_PROPERTY);
		wc_printf("<a href=\"display_edit_task?msgnum=%ld?taskrm=", Cal->cal_msgnum);
		urlescputs(ChrPtr(WC->CurRoom.name));
		wc_printf("\">");
		/* wc_printf("<img align=middle "
		"src=\"static/taskmanag_16x.gif\" border=0>&nbsp;"); */
		if (p != NULL) {
			escputs((char *)icalproperty_get_comment(p));
		}
		wc_printf("</a>\n");
		wc_printf("</td>\n");

		due = get_task_due_date(Cal->cal, &is_date);
		wc_printf("<td class=\"task_due_date\"><span");
		if (due > 0) {
			webcit_fmt_date(buf, SIZ, due, is_date ? DATEFMT_RAWDATE : DATEFMT_FULL);
			wc_printf(">%s",buf);
		}
		else {
			wc_printf(">");
		}
		wc_printf("</span></td>");
		wc_printf("<td class=\"task_category\">");
		p = icalcomponent_get_first_property(Cal->cal,
			ICAL_CATEGORIES_PROPERTY);
		if (p != NULL) {
			escputs((char *)icalproperty_get_categories(p));
		}
		wc_printf("</td>");
		wc_printf("</tr>");
	}

	wc_printf("</tbody></table>\n");

	/* Free the list */
	DeleteHash(&WC->disp_cal_items);
	DeleteHashPos(&Pos);
	return 0;
}


/*
 * Display a task by itself (for editing)
 */
void display_edit_individual_task(icalcomponent *supplied_vtodo, long msgnum, char *from,
			int unread, calview *calv)
{
	wcsession *WCC = WC;
	icalcomponent *vtodo;
	icalproperty *p;
	struct icaltimetype IcalTime;
	int created_new_vtodo = 0;
	icalproperty_status todoStatus;

	if (supplied_vtodo != NULL) {
		vtodo = supplied_vtodo;

		/*
		 * It's safe to convert to UTC here because there are no recurrences to worry about.
		 */
		ical_dezonify(vtodo);

		/*
		 * If we're looking at a fully encapsulated VCALENDAR
		 * rather than a VTODO component, attempt to use the first
		 * relevant VTODO subcomponent.  If there is none, the
		 * NULL returned by icalcomponent_get_first_component() will
		 * tell the next iteration of this function to create a
		 * new one.
		 */
		if (icalcomponent_isa(vtodo) == ICAL_VCALENDAR_COMPONENT) {
			display_edit_individual_task(
				icalcomponent_get_first_component(
					vtodo, ICAL_VTODO_COMPONENT
					), 
				msgnum, from, unread, calv
				);
			return;
		}
	}
	else {
		vtodo = icalcomponent_new(ICAL_VTODO_COMPONENT);
		created_new_vtodo = 1;
	}
	
	/* TODO: Can we take all this and move it into a template?	 */
	output_headers(1, 1, 1, 0, 0, 0);
	wc_printf("<!-- start task edit form -->");
	p = icalcomponent_get_first_property(vtodo, ICAL_SUMMARY_PROPERTY);
	/* Get summary early for title */
	wc_printf("<div class=\"box\">\n");
	wc_printf("<div class=\"boxlabel\">");
	wc_printf(_("Edit task"));
	wc_printf("- ");
	if (p != NULL) {
		escputs((char *)icalproperty_get_comment(p));
	}
	wc_printf("</div>");
	
	wc_printf("<div class=\"boxcontent\">\n");
	wc_printf("<FORM METHOD=\"POST\" action=\"save_task\">\n");
	wc_printf("<div style=\"display: none;\">\n	");

	wc_printf("<input type=\"hidden\" name=\"go\" value=\"");
	StrEscAppend(WCC->WBuf, WCC->CurRoom.name, NULL, 0, 0);
	wc_printf("\">\n");

	wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
	wc_printf("<INPUT TYPE=\"hidden\" NAME=\"msgnum\" VALUE=\"%ld\">\n", msgnum);
	wc_printf("<INPUT TYPE=\"hidden\" NAME=\"return_to_summary\" VALUE=\"%d\">\n",
		ibstr("return_to_summary"));
	wc_printf("</div>");
	wc_printf("<table class=\"calendar_background\"><tr><td>");
	wc_printf("<TABLE STYLE=\"border: none;\">\n");

	wc_printf("<TR><TD>");
	wc_printf(_("Summary:"));
	wc_printf("</TD><TD>"
		"<INPUT TYPE=\"text\" NAME=\"summary\" "
		"MAXLENGTH=\"64\" SIZE=\"64\" VALUE=\"");
	p = icalcomponent_get_first_property(vtodo, ICAL_SUMMARY_PROPERTY);
	if (p != NULL) {
		escputs((char *)icalproperty_get_comment(p));
	}
	wc_printf("\"></TD></TR>\n");

	wc_printf("<TR><TD>");
	wc_printf(_("Start date:"));
	wc_printf("</TD><TD>");
	p = icalcomponent_get_first_property(vtodo, ICAL_DTSTART_PROPERTY);
	wc_printf("<INPUT TYPE=\"CHECKBOX\" NAME=\"nodtstart\" ID=\"nodtstart\" VALUE=\"NODTSTART\" ");
	if (p == NULL) {
		wc_printf("CHECKED=\"CHECKED\"");
	}
	wc_printf(">");
	wc_printf(_("No date"));
	
	wc_printf(" ");
	wc_printf("<span ID=\"dtstart_date\">");
	wc_printf(_("or"));
	wc_printf(" ");
	if (p != NULL) {
		IcalTime = icalproperty_get_dtstart(p);
	}
	else
		IcalTime = icaltime_current_time_with_zone(get_default_icaltimezone());
	display_icaltimetype_as_webform(&IcalTime, "dtstart", 0);

	wc_printf("<INPUT TYPE=\"CHECKBOX\" NAME=\"dtstart_time_assoc\" ID=\"dtstart_time_assoc\" VALUE=\"yes\"");
	if (!IcalTime.is_date) {
		wc_printf("CHECKED=\"CHECKED\"");
	}
	wc_printf(">");
	wc_printf(_("Time associated"));
	wc_printf("</span></TD></TR>\n");

	wc_printf("<TR><TD>");
	wc_printf(_("Due date:"));
	wc_printf("</TD><TD>");
	p = icalcomponent_get_first_property(vtodo, ICAL_DUE_PROPERTY);
	wc_printf("<INPUT TYPE=\"CHECKBOX\" NAME=\"nodue\" ID=\"nodue\" VALUE=\"NODUE\"");
	if (p == NULL) {
		wc_printf("CHECKED=\"CHECKED\"");
	}
	wc_printf(">");
	wc_printf(_("No date"));
	wc_printf(" ");
	wc_printf("<span ID=\"due_date\">\n");
	wc_printf(_("or"));
	wc_printf(" ");
	if (p != NULL) {
		IcalTime = icalproperty_get_due(p);
	}
	else
		IcalTime = icaltime_current_time_with_zone(get_default_icaltimezone());
	display_icaltimetype_as_webform(&IcalTime, "due", 0);

	wc_printf("<INPUT TYPE=\"CHECKBOX\" NAME=\"due_time_assoc\" ID=\"due_time_assoc\" VALUE=\"yes\"");
	if (!IcalTime.is_date) {
		wc_printf("CHECKED=\"CHECKED\"");
	}
	wc_printf(">");
	wc_printf(_("Time associated"));
	wc_printf("</span></TD></TR>\n");
	todoStatus = icalcomponent_get_status(vtodo);
	wc_printf("<TR><TD>\n");
	wc_printf(_("Completed:"));
	wc_printf("</TD><TD>");
	wc_printf("<INPUT TYPE=\"CHECKBOX\" NAME=\"status\" VALUE=\"COMPLETED\"");
	if (todoStatus == ICAL_STATUS_COMPLETED) {
		wc_printf(" CHECKED=\"CHECKED\"");
	} 
	wc_printf(" >");
	wc_printf("</TD></TR>");
	/* start category field */
	p = icalcomponent_get_first_property(vtodo, ICAL_CATEGORIES_PROPERTY);
	wc_printf("<TR><TD>");
	wc_printf(_("Category:"));
	wc_printf("</TD><TD>");
	wc_printf("<INPUT TYPE=\"text\" NAME=\"category\" MAXLENGTH=\"32\" SIZE=\"32\" VALUE=\"");
	if (p != NULL) {
		escputs((char *)icalproperty_get_categories(p));
	}
	wc_printf("\">");
	wc_printf("</TD></TR>\n	");
	/* end category field */
	wc_printf("<TR><TD>");
	wc_printf(_("Description:"));
	wc_printf("</TD><TD>");
	wc_printf("<TEXTAREA NAME=\"description\" "
		"ROWS=\"10\" COLS=\"80\">\n"
		);
	p = icalcomponent_get_first_property(vtodo, ICAL_DESCRIPTION_PROPERTY);
	if (p != NULL) {
		escputs((char *)icalproperty_get_comment(p));
	}
	wc_printf("</TEXTAREA></TD></TR></TABLE>\n");

	wc_printf("<SPAN STYLE=\"text-align: center;\">"
		"<INPUT TYPE=\"submit\" NAME=\"save_button\" VALUE=\"%s\">"
		"&nbsp;&nbsp;"
		"<INPUT TYPE=\"submit\" NAME=\"delete_button\" VALUE=\"%s\">\n"
		"&nbsp;&nbsp;"
		"<INPUT TYPE=\"submit\" NAME=\"cancel_button\" VALUE=\"%s\">\n"
		"</SPAN>\n",
		_("Save"),
		_("Delete"),
		_("Cancel")
		);
	wc_printf("</td></tr></table>");
	wc_printf("</FORM>\n");
	wc_printf("</div></div></div>\n");
	wc_printf("<!-- end task edit form -->");
	wDumpContent(1);

	if (created_new_vtodo) {
		icalcomponent_free(vtodo);
	}
}

/*
 * Save an edited task
 *
 * supplied_vtodo 	the task to save
 * msgnum		number of the mesage in our db
 */
void save_individual_task(icalcomponent *supplied_vtodo, long msgnum, char* from, int unread,
			  calview *calv)
{
	char buf[SIZ];
	int delete_existing = 0;
	icalproperty *prop;
	icalcomponent *vtodo, *encaps;
	int created_new_vtodo = 0;
	int i;
	int sequence = 0;
	struct icaltimetype t;

	if (supplied_vtodo != NULL) {
		vtodo = supplied_vtodo;
		/**
		 * If we're looking at a fully encapsulated VCALENDAR
		 * rather than a VTODO component, attempt to use the first
		 * relevant VTODO subcomponent.  If there is none, the
		 * NULL returned by icalcomponent_get_first_component() will
		 * tell the next iteration of this function to create a
		 * new one.
		 */
		if (icalcomponent_isa(vtodo) == ICAL_VCALENDAR_COMPONENT) {
			save_individual_task(
				icalcomponent_get_first_component(
					vtodo, ICAL_VTODO_COMPONENT), 
				msgnum, from, unread, calv
				);
			return;
		}
	}
	else {
		vtodo = icalcomponent_new(ICAL_VTODO_COMPONENT);
		created_new_vtodo = 1;
	}

	if (havebstr("save_button")) {

		/** Replace values in the component with ones from the form */

		while (prop = icalcomponent_get_first_property(vtodo,
							       ICAL_SUMMARY_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vtodo, prop);
			icalproperty_free(prop);
		}
		if (havebstr("summary")) {

			icalcomponent_add_property(vtodo,
						   icalproperty_new_summary(bstr("summary")));
		} else {
			icalcomponent_add_property(vtodo,
						   icalproperty_new_summary(_("Untitled Task")));
		}
	
		while (prop = icalcomponent_get_first_property(vtodo,
							       ICAL_DESCRIPTION_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vtodo, prop);
			icalproperty_free(prop);
		}
		if (havebstr("description")) {
			icalcomponent_add_property(vtodo,
						   icalproperty_new_description(bstr("description")));
		}
	
		while (prop = icalcomponent_get_first_property(vtodo,
							       ICAL_DTSTART_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vtodo, prop);
			icalproperty_free(prop);
		}
		if (IsEmptyStr(bstr("nodtstart"))) {
			if (yesbstr("dtstart_time")) {
				icaltime_from_webform(&t, "dtstart");
			}
			else {
				icaltime_from_webform_dateonly(&t, "dtstart");
			}
			icalcomponent_add_property(vtodo,
						   icalproperty_new_dtstart(t)
				);
		}
		while(prop = icalcomponent_get_first_property(vtodo,
							      ICAL_STATUS_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vtodo,prop);
			icalproperty_free(prop);
		}
		while(prop = icalcomponent_get_first_property(vtodo,
							      ICAL_PERCENTCOMPLETE_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vtodo,prop);
			icalproperty_free(prop);
		}

		if (havebstr("status")) {
			icalproperty_status taskStatus = icalproperty_string_to_status(bstr("status"));
			icalcomponent_set_status(vtodo, taskStatus);
			icalcomponent_add_property(vtodo,
				icalproperty_new_percentcomplete(
					(strcasecmp(bstr("status"), "completed") ? 0 : 100)
				)
			);
		}
		else {
			icalcomponent_add_property(vtodo, icalproperty_new_percentcomplete(0));
		}
		while (prop = icalcomponent_get_first_property(vtodo,
							       ICAL_CATEGORIES_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vtodo,prop);
			icalproperty_free(prop);
		}
		if (!IsEmptyStr(bstr("category"))) {
			prop = icalproperty_new_categories(bstr("category"));
			icalcomponent_add_property(vtodo,prop);
		}
		while (prop = icalcomponent_get_first_property(vtodo,
							       ICAL_DUE_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vtodo, prop);
			icalproperty_free(prop);
		}
		if (IsEmptyStr(bstr("nodue"))) {
			if (yesbstr("due_time")) {
				icaltime_from_webform(&t, "due");
			}
			else {
				icaltime_from_webform_dateonly(&t, "due");
			}
			icalcomponent_add_property(vtodo,
						   icalproperty_new_due(t)
				);
		}
		/** Give this task a UID if it doesn't have one. */
		syslog(LOG_DEBUG, "Give this task a UID if it doesn't have one.\n");
		if (icalcomponent_get_first_property(vtodo,
						     ICAL_UID_PROPERTY) == NULL) {
			generate_uuid(buf);
			icalcomponent_add_property(vtodo,
						   icalproperty_new_uid(buf)
				);
		}

		/* Increment the sequence ID */
		syslog(LOG_DEBUG, "Increment the sequence ID\n");
		while (prop = icalcomponent_get_first_property(vtodo,
							       ICAL_SEQUENCE_PROPERTY), (prop != NULL) ) {
			i = icalproperty_get_sequence(prop);
			syslog(LOG_DEBUG, "Sequence was %d\n", i);
			if (i > sequence) sequence = i;
			icalcomponent_remove_property(vtodo, prop);
			icalproperty_free(prop);
		}
		++sequence;
		syslog(LOG_DEBUG, "New sequence is %d.  Adding...\n", sequence);
		icalcomponent_add_property(vtodo,
					   icalproperty_new_sequence(sequence)
			);

		/*
		 * Encapsulate event into full VCALENDAR component.  Clone it first,
		 * for two reasons: one, it's easier to just free the whole thing
		 * when we're done instead of unbundling, but more importantly, we
		 * can't encapsulate something that may already be encapsulated
		 * somewhere else.
		 */
		syslog(LOG_DEBUG, "Encapsulating into a full VCALENDAR component\n");
		encaps = ical_encapsulate_subcomponent(icalcomponent_new_clone(vtodo));

		/* Serialize it and save it to the message base */
		serv_puts("ENT0 1|||4");
		serv_getln(buf, sizeof buf);
		if (buf[0] == '4') {
			serv_puts("Content-type: text/calendar");
			serv_puts("");
			serv_puts(icalcomponent_as_ical_string(encaps));
			serv_puts("000");

			/*
			 * Probably not necessary; the server will see the UID
			 * of the object and delete the old one anyway, but
			 * just in case...
			 */
			delete_existing = 1;
		}
		icalcomponent_free(encaps);
	}

	/**
	 * If the user clicked 'Delete' then explicitly delete the message.
	 */
	if (havebstr("delete_button")) {
		delete_existing = 1;
	}

	if ( (delete_existing) && (msgnum > 0L) ) {
		serv_printf("DELE %ld", lbstr("msgnum"));
		serv_getln(buf, sizeof buf);
	}

	if (created_new_vtodo) {
		icalcomponent_free(vtodo);
	}

	/* Go back to wherever we came from */
	if (ibstr("return_to_summary") == 1) {
		display_summary_page();
	}
	else {
		readloop(readfwd, eUseDefault);
	}
}


/*
 * free memory allocated using libical
 */
void delete_task(void *vCal)
{
        disp_cal *Cal = (disp_cal*) vCal;
        icalcomponent_free(Cal->cal);
        free(Cal->from);
        free(Cal);
}


/*
 * Load a Task into a hash table for later display.
 */
void load_task(icalcomponent *event, long msgnum, char *from, int unread, calview *calv)
{
	icalproperty *ps = NULL;
	struct icaltimetype dtstart, dtend;
	wcsession *WCC = WC;
	disp_cal *Cal;
	size_t len;
	icalcomponent *cptr = NULL;

	dtstart = icaltime_null_time();
	dtend = icaltime_null_time();
	
	if (WCC->disp_cal_items == NULL) {
		WCC->disp_cal_items = NewHash(0, Flathash);
	}

	Cal = (disp_cal*) malloc(sizeof(disp_cal));
	memset(Cal, 0, sizeof(disp_cal));
	Cal->cal = icalcomponent_new_clone(event);

	/* Dezonify and decapsulate at the very last moment */
	ical_dezonify(Cal->cal);
	if (icalcomponent_isa(Cal->cal) != ICAL_VTODO_COMPONENT) {
		cptr = icalcomponent_get_first_component(Cal->cal, ICAL_VTODO_COMPONENT);
		if (cptr) {
			cptr = icalcomponent_new_clone(cptr);
			icalcomponent_free(Cal->cal);
			Cal->cal = cptr;
		}
	}

	Cal->unread = unread;
	len = strlen(from);
	Cal->from = (char*)malloc(len+ 1);
	memcpy(Cal->from, from, len + 1);
	Cal->cal_msgnum = msgnum;

	/* Precalculate the starting date and time of this event, and store it in our top-level
	 * structure.  Later, when we are rendering the calendar, we can just peek at these values
	 * without having to break apart every calendar item.
	 */
	ps = icalcomponent_get_first_property(Cal->cal, ICAL_DTSTART_PROPERTY);
	if (ps != NULL) {
		dtstart = icalproperty_get_dtstart(ps);
		Cal->event_start = icaltime_as_timet(dtstart);
	}

	/* Do the same for the ending date and time.  It makes the day view much easier to render. */
	ps = icalcomponent_get_first_property(Cal->cal, ICAL_DTEND_PROPERTY);
	if (ps != NULL) {
		dtend = icalproperty_get_dtend(ps);
		Cal->event_end = icaltime_as_timet(dtend);
	}

	/* Store it in the hash list. */
	/* syslog(LOG_DEBUG, "INITIAL: %s", ctime(&Cal->event_start)); */
	Put(WCC->disp_cal_items, 
	    (char*) &Cal->event_start,
	    sizeof(Cal->event_start), 
	    Cal, 
	    delete_task
	);
}



/*
 * Display task view
 */
int tasks_LoadMsgFromServer(SharedMessageStatus *Stat, 
			    void **ViewSpecific, 
			    message_summary* Msg, 
			    int is_new, 
			    int i)
{
	/* Not (yet?) needed here? calview *c = (calview *) *ViewSpecific; */

	load_ical_object(Msg->msgnum, is_new, ICAL_VTODO_COMPONENT, load_task, NULL, 0);
	return 0;
}

/*
 * Display the editor component for a task
 */
void display_edit_task(void) {
	long msgnum = 0L;
			
	/* Force change the room if we have to */
	if (havebstr("taskrm")) {
		gotoroom(sbstr("taskrm"));
	}

	msgnum = lbstr("msgnum");
	if (msgnum > 0L) {
		/* existing task */
		load_ical_object(msgnum, 0,
				 ICAL_VTODO_COMPONENT,
				 display_edit_individual_task,
				 NULL, 0
		);
	}
	else {
		/* new task */
		display_edit_individual_task(NULL, 0L, "", 0, NULL);
	}
}

/*
 * save an edited task
 */
void save_task(void) {
	long msgnum = 0L;
	msgnum = lbstr("msgnum");
	if (msgnum > 0L) {
		load_ical_object(msgnum, 0, ICAL_VTODO_COMPONENT, save_individual_task, NULL, 0);
	}
	else {
		save_individual_task(NULL, 0L, "", 0, NULL);
	}
}



int tasks_GetParamsGetServerCall(SharedMessageStatus *Stat, 
				 void **ViewSpecific, 
				 long oper, 
				 char *cmd, 
				 long len,
				 char *filter,
				 long flen)
{
	strcpy(cmd, "MSGS ALL");
	Stat->maxmsgs = 32767;
	return 200;
}


int tasks_Cleanup(void **ViewSpecific)
{
	wDumpContent(1);
/* Tasks doesn't need the calview struct... 
	free (*ViewSpecific);
	*ViewSpecific = NULL;
	*/
	return 0;
}

void 
InitModule_TASKS
(void)
{
	RegisterReadLoopHandlerset(
		VIEW_TASKS,
		tasks_GetParamsGetServerCall,
		NULL,
		NULL,
		NULL,
		tasks_LoadMsgFromServer,
		tasks_RenderView_or_Tail,
		tasks_Cleanup);
	WebcitAddUrlHandler(HKEY("save_task"), "", 0, save_task, 0);
}
