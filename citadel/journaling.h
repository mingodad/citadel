struct jnlq {
	struct jnlq *next;
	recptypes recps;
	char *from;
	char *node;
	char *rfca;
	char *subj;
	char *msgn;
	char *rfc822;
};

void JournalBackgroundSubmit(struct CtdlMessage *msg,
                        StrBuf *saved_rfc822_version,
                        recptypes *recps);
void JournalRunQueueMsg(struct jnlq *jmsg);
void JournalRunQueue(void);
