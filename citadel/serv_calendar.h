/*
 * $Id$
 *
 * iCalendar implementation for Citadel
 *
 */

/* 
 * "server_generated_invitations" tells the Citadel server that the
 * client wants invitations to be generated and sent out by the
 * server.  Set to 1 to enable this functionality.
 *
 * "avoid_sending_invitations" is a server-internal variable.  It is
 * set internally during certain transactions and cleared
 * automatically.
 */
struct cit_ical {
	int server_generated_invitations;
        int avoid_sending_invitations;
};

#define CIT_ICAL CC->CIT_ICAL

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
