/* $Id$ */

struct jnlq {
	struct jnlq *next;
	struct recptypes recps;
	char *from;
	char *node;
	char *rfca;
	char *subj;
	char *msgn;
	char *rfc822;
};

void JournalBackgroundSubmit(struct CtdlMessage *msg,
                        char *saved_rfc822_version,
                        struct recptypes *recps);
void JournalRunQueueMsg(struct jnlq *jmsg);
void JournalRunQueue(void);
