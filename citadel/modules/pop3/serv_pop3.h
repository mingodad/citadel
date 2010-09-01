/*
 * Copyright (c) 1998-2009 by the citadel.org team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

struct pop3msg {
	long msgnum;
	size_t rfc822_length;
	int deleted;
};

struct citpop3 {		/* Information about the current session */
	struct pop3msg *msgs;	/* Array of message pointers */
	int num_msgs;		/* Number of messages in array */
	int lastseen;		/* Offset of last-read message in array */
};
				/* Note: the "lastseen" is represented as the
				 * offset in this array (zero-based), so when
				 * displaying it to a POP3 client, it must be
				 * incremented by one.
				 */

#define POP3 ((struct citpop3 *)CC->session_specific_data)

void pop3_cleanup_function(void);
void pop3_greeting(void);
void pop3_user(char *argbuf);
void pop3_pass(char *argbuf);
void pop3_list(char *argbuf);
void pop3_command_loop(void);
void pop3_login(void);

