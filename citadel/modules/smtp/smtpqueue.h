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
	/**<
	 * 0 = No delivery has yet been attempted
	 * 2 = Delivery was successful
	 * 4 = A transient error was experienced ... try again later
	 * 5 = Delivery to this address failed permanently.  The error message
         *     should be placed in the fourth field so that a bounce message may
	 *     be generated.
	 */

	int n;
	int Active;
}MailQEntry;

typedef struct queueitem {
	long MessageID;
	long QueMsgID;
	long Submitted;
	int FailNow;
	HashList *MailQEntries;
	MailQEntry *Current; /* copy of the currently parsed item in the MailQEntries list; if null add a new one. */
	DeliveryAttempt LastAttempt;
	long ActiveDeliveries;
	StrBuf *EnvelopeFrom;
	StrBuf *BounceTo;
	ParsedURL *URL;
	ParsedURL *FallBackHost;
} OneQueItem;
typedef void (*QItemHandler)(OneQueItem *Item, StrBuf *Line, const char **Pos);

int DecreaseQReference(OneQueItem *MyQItem);
void RemoveQItem(OneQueItem *MyQItem);
int CountActiveQueueEntries(OneQueItem *MyQItem);
StrBuf *SerializeQueueItem(OneQueItem *MyQItem);

void smtpq_do_bounce(OneQueItem *MyQItem, StrBuf *OMsgTxt);
