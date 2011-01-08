/*****************************************************************************/
/*               SMTP CLIENT (Queue Management) STUFF                        */
/*****************************************************************************/

#define MaxAttempts 15
typedef struct _delivery_attempt {
	time_t when;
	time_t retry;
}DeliveryAttempt;

typedef struct _mailq_entry {
	DeliveryAttempt Attempts[MaxAttempts];
	int nAttempts;
	StrBuf *Recipient;
	StrBuf *StatusMessage;
	int Status;
	int n;
	int Active;
}MailQEntry;

typedef struct queueitem {
	long MessageID;
	long QueMsgID;
	int FailNow;
	HashList *MailQEntries;
	MailQEntry *Current; /* copy of the currently parsed item in the MailQEntries list; if null add a new one. */
	DeliveryAttempt LastAttempt;
	long ActiveDeliveries;
	StrBuf *EnvelopeFrom;
	StrBuf *BounceTo;
} OneQueItem;
typedef void (*QItemHandler)(OneQueItem *Item, StrBuf *Line, const char **Pos);

int DecreaseQReference(OneQueItem *MyQItem);
void RemoveQItem(OneQueItem *MyQItem);
int CountActiveQueueEntries(OneQueItem *MyQItem);
StrBuf *SerializeQueueItem(OneQueItem *MyQItem);
