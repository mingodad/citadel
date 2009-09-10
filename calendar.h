
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
				    long len);

int calendar_Cleanup(void **ViewSpecific);

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
#endif
