
struct pop3msg {
	long msgnum;
	size_t rfc822_length;
	int deleted;
	FILE *temp;
};

struct citpop3 {		/* Information about the current session */
	struct pop3msg *msgs;
	int num_msgs;
};

#define POP3 ((struct citpop3 *)CtdlGetUserData(SYM_POP3))

void pop3_cleanup_function(void);
void pop3_greeting(void);
void pop3_user(char *argbuf);
void pop3_pass(char *argbuf);
void pop3_list(char *argbuf);
void pop3_command_loop(void);
