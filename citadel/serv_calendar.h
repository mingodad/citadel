/*
 * $Id$
 *
 * iCalendar implementation for Citadel/UX
 *
 */

extern long SYM_CIT_ICAL;

struct cit_ical {
        int avoid_sending_invitations;
};

#define CIT_ICAL ((struct cit_ical *)CtdlGetUserData(SYM_CIT_ICAL))

/*
 * When saving a message containing calendar information, we keep track of
 * some components in the calendar object that need to be inserted into
 * message fields.
 */
struct icalmessagemod {
	char subject[SIZ];
	char uid[SIZ];
	time_t dtstart;
};
