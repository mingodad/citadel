/*
 * Copyright (c) 1996-2012 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 * 
 * 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * 
 * 
 * 
 */

#ifndef __CALENDAR_H__
#define __CALENDAR_H__
/*
 * calview contains data passed back and forth between the message fetching loop
 * and the calendar view renderer.
 */
enum {
	calview_month,
	calview_day,
	calview_week,
	calview_brief,
	calview_summary
};

typedef struct _calview {
	int view;
	int year;
	int month;
	int day;
	time_t lower_bound;
	time_t upper_bound;
}calview;

typedef void (*IcalCallbackFunc)(icalcomponent *, long, char*, int, calview *);

void display_individual_cal(icalcomponent *cal, long msgnum, char *from, int unread, calview *calv);
void load_ical_object(long msgnum, int unread,
		      icalcomponent_kind which_kind,
		      IcalCallbackFunc CallBack,
		      calview *calv,
		      int RenderAsync
	);

int calendar_LoadMsgFromServer(SharedMessageStatus *Stat, 
			       void **ViewSpecific, 
			       message_summary* Msg, 
			       int is_new, 
			       int i);
int calendar_RenderView_or_Tail(SharedMessageStatus *Stat, 
				void **ViewSpecific, 
				long oper);

int calendar_GetParamsGetServerCall(SharedMessageStatus *Stat, 
				    void **ViewSpecific, 
				    long oper, 
				    char *cmd, 
				    long len,
				    char *filter,
				    long flen);

int calendar_Cleanup(void **ViewSpecific);
int __calendar_Cleanup(void **ViewSpecific);

void render_calendar_view(calview *c);
void display_edit_individual_event(icalcomponent *supplied_vtodo, long msgnum, char *from,
	int unread, calview *calv);
void save_individual_event(icalcomponent *supplied_vtodo, long msgnum, char *from,
	int unread, calview *calv);
void ical_dezonify(icalcomponent *cal);

int tasks_LoadMsgFromServer(SharedMessageStatus *Stat, 
			    void **ViewSpecific, 
			    message_summary* Msg, 
			    int is_new, 
			    int i);

void display_edit_task(void);
void display_edit_event(void);

icaltimezone *get_default_icaltimezone(void);
void display_icaltimetype_as_webform(struct icaltimetype *, char *, int);
void icaltime_from_webform(struct icaltimetype *result, char *prefix);
void icaltime_from_webform_dateonly(struct icaltimetype *result, char *prefix);
void partstat_as_string(char *buf, icalproperty *attendee);
icalcomponent *ical_encapsulate_subcomponent(icalcomponent *subcomp);
void check_attendee_availability(icalcomponent *supplied_vevent);
int ical_ctdl_is_overlap(
                        struct icaltimetype t1start,
                        struct icaltimetype t1end,
                        struct icaltimetype t2start,
                        struct icaltimetype t2end
);


#endif
