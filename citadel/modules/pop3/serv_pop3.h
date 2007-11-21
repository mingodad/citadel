/*
 * $Id$
 *
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

