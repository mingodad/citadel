/*
 * iCalendar implementation for Citadel
 *
 *
 * Copyright (c) 1987-2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *  
 *  
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  
 *  
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
#define MAX_RECUR 1000
