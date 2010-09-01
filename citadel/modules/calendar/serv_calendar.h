/*
 * iCalendar implementation for Citadel
 *
 *
 * Copyright (c) 1987-2009 by the citadel.org team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
