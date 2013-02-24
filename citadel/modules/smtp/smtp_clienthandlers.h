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

typedef enum _eSMTP_C_States {
	eConnectMX,
	eEHLO,
	eHELO,
	eSMTPAuth,
	eFROM,
	eRCPT,
	eDATA,
	eDATABody,
	eDATATerminateBody,
	eQUIT,
	eMaxSMTPC
} eSMTP_C_States;


typedef struct _stmp_out_msg {
	MailQEntry *MyQEntry;
	OneQueItem *MyQItem;
	long n;
	AsyncIO IO;
	long CXFlags;
	int IDestructQueItem;
	int nRemain;

	eSMTP_C_States State;

	struct ares_mx_reply *AllMX;
	struct ares_mx_reply *CurrMX;
	const char *mx_port;
	const char *mx_host;
	const char *LookupHostname;
	int iMX, nMX;
	int LookupWhich;

	DNSQueryParts MxLookup;
	DNSQueryParts HostLookup;
	struct hostent *OneMX;
	char **pIP;

	ParsedURL *Relay;
	ParsedURL *pCurrRelay;
	StrBuf *msgtext;
	StrBuf *QMsgData;
	const char *envelope_from;

	char user[1024];
	char node[1024];
	char name[1024];
	char mailfrom[1024];
	long SendLogin;
	long Flags;
} SmtpOutMsg;


typedef eNextState (*SMTPReadHandler)(SmtpOutMsg *Msg);
typedef eNextState (*SMTPSendHandler)(SmtpOutMsg *Msg);

SMTPReadHandler ReadHandlers[eMaxSMTPC];
SMTPSendHandler SendHandlers[eMaxSMTPC];
const ConstStr ReadErrors[eMaxSMTPC+1];
const double SMTP_C_ReadTimeouts[eMaxSMTPC];
const double SMTP_C_SendTimeouts[eMaxSMTPC];
const double SMTP_C_ConnTimeout;

#define F_RELAY          (1<<0) /* we have a Relay    host configuration */
#define F_HAVE_FALLBACK  (1<<1) /* we have a fallback host configuration */
#define F_FALLBACK       (1<<2)
#define F_HAVE_MX        (1<<3) /* we have a list of mx records to go through.*/
#define F_DIRECT         (1<<4) /* no mx record found, trying direct connect. */

extern int SMTPClientDebugEnabled;

int smtp_resolve_recipients(SmtpOutMsg *SendMsg);

#define QID ((SmtpOutMsg*)IO->Data)->MyQItem->MessageID
#define N ((SmtpOutMsg*)IO->Data)->n
#define DBGLOG(LEVEL) if ((LEVEL != LOG_DEBUG) || (SMTPClientDebugEnabled != 0))

#define EVS_syslog(LEVEL, FORMAT, ...) \
	DBGLOG(LEVEL) syslog(LEVEL,		  \
	       "SMTPC:IO[%ld]CC[%d]S[%ld][%ld] " FORMAT, \
	       IO->ID, CCID, QID, N, __VA_ARGS__)

#define EVSM_syslog(LEVEL, FORMAT) \
	DBGLOG(LEVEL) syslog(LEVEL, \
	       "SMTPC:IO[%ld]CC[%d]S[%ld][%ld] " FORMAT, \
	       IO->ID, CCID, QID, N)

#define EVNCS_syslog(LEVEL, FORMAT, ...) \
	DBGLOG(LEVEL) syslog(LEVEL, "SMTPC:IO[%ld]S[%ld][%ld] " FORMAT, \
	       IO->ID, QID, N, __VA_ARGS__)

#define EVNCSM_syslog(LEVEL, FORMAT) \
	DBGLOG(LEVEL) syslog(LEVEL, "SMTPC:IO[%ld]S[%ld][%ld] " FORMAT, \
	       IO->ID, QID, N)

#define SMTPC_syslog(LEVEL, FORMAT, ...)	  \
	DBGLOG(LEVEL) syslog(LEVEL,		  \
			     "SMTPCQ: " FORMAT,	  \
			     __VA_ARGS__)

#define SMTPCM_syslog(LEVEL, FORMAT)		\
	DBGLOG(LEVEL) syslog(LEVEL,		\
			     "SMTPCQ: " FORMAT)



typedef enum __smtpstate {
	eSTMPmxlookup,
	eSTMPevaluatenext,
	eSTMPalookup,
	eSTMPaaaalookup,
	eSTMPconnecting,
	eSTMPsmtp,
	eSTMPsmtpdata,
	eSTMPsmtpdone,
	eSTMPfinished,
	eSTMPfailOne,
	eSMTPFailTemporary,
	eSMTPFailTotal
} smtpstate;

void SetSMTPState(AsyncIO *IO, smtpstate State);
