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
