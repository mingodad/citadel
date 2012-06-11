/*
 *
 * Copyright (c) 1998-2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
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

/*****************************************************************************/
/*               SMTP CLIENT (Queue Management) STUFF                        */
/*****************************************************************************/

#define MaxAttempts 15
extern const unsigned short DefaultMXPort;

typedef struct _mailq_entry {
	StrBuf *Recipient;
	StrBuf *StatusMessage;
	int Status;
	/**<
	 * 0 = No delivery has yet been attempted
	 * 2 = Delivery was successful
	 * 3 = Transient error like connection problem. Try next remote if available.
	 * 4 = A transient error was experienced ... try again later
	 * 5 = Delivery to this address failed permanently.  The error message
	 *     should be placed in the fourth field so that a bounce message may
	 *     be generated.
	 */

	int n;
	int Active;
}MailQEntry;

typedef struct queueitem {
	long SendBounceMail;
	long MessageID;
	long QueMsgID;
	long Submitted;
	int FailNow;
	HashList *MailQEntries;
/* copy of the currently parsed item in the MailQEntries list;
 * if null add a new one.
 */
	MailQEntry *Current;
	time_t ReattemptWhen;
	time_t Retry;

	long ActiveDeliveries;
	long NotYetShutdownDeliveries;
	StrBuf *EnvelopeFrom;
	StrBuf *BounceTo;
	StrBuf *SenderRoom;
	ParsedURL *URL;
	ParsedURL *FallBackHost;
} OneQueItem;

typedef void (*QItemHandler)(OneQueItem *Item, StrBuf *Line, const char **Pos);


typedef struct __QItemHandlerStruct {
	QItemHandler H;
} QItemHandlerStruct;
int     DecreaseQReference(OneQueItem *MyQItem);
void DecreaseShutdownDeliveries(OneQueItem *MyQItem);
int GetShutdownDeliveries(OneQueItem *MyQItem);
void    RemoveQItem(OneQueItem *MyQItem);
int     CountActiveQueueEntries(OneQueItem *MyQItem);
StrBuf *SerializeQueueItem(OneQueItem *MyQItem);
void    smtpq_do_bounce(OneQueItem *MyQItem, StrBuf *OMsgTxt);

int CheckQEntryIsBounce(MailQEntry *ThisItem);
