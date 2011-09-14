/*
 * This module handles shared rooms, inter-Citadel mail, and outbound
 * mailing list processing.
 *
 * Copyright (c) 2000-2011 by the citadel.org team
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
 *
 * ** NOTE **   A word on the S_NETCONFIGS semaphore:
 * This is a fairly high-level type of critical section.  It ensures that no
 * two threads work on the netconfigs files at the same time.  Since we do
 * so many things inside these, here are the rules:
 *  1. begin_critical_section(S_NETCONFIGS) *before* begin_ any others.
 *  2. Do *not* perform any I/O with the client during these sections.
 *
 */


#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#ifdef HAVE_SYSCALL_H
# include <syscall.h>
#else 
# if HAVE_SYS_SYSCALL_H
#  include <sys/syscall.h>
# endif
#endif

#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "user_ops.h"
#include "database.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "serv_network.h"
#include "clientsocket.h"
#include "file_ops.h"
#include "citadel_dirs.h"
#include "threads.h"

#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

#include "context.h"

#include "netconfig.h"
#include "ctdl_module.h"

struct CitContext networker_client_CC;

typedef enum _eNWCState {
	eeGreating,
	eAuth,
	eNDOP,
	eREAD,
	eReadBLOB,
	eCLOS,
	eNUOP,
	eWRIT,
	eWriteBLOB,
	eUCLS,
	eQUIT
}eNWCState;


typedef struct _async_networker {
        AsyncIO IO;
	DNSQueryParts HostLookup;
	eNWCState State;
	int fd;
	long n;
        FILE *TmpFile;
        long bytes_received;
        StrBuf *SpoolFileName;
        StrBuf *tempFileName;
	StrBuf *node;
	StrBuf *host;
	StrBuf *port;
	StrBuf *secret;
	StrBuf		*Url;

	long download_len;
	long BlobReadSize;
	long bytes_written;
	long bytes_to_write;
} AsyncNetworker;

typedef eNextState(*NWClientHandler)(AsyncNetworker* NW);
eNextState nwc_get_one_host_ip(AsyncIO *IO);

eNextState nwc_connect_ip(AsyncIO *IO);

eNextState NWC_SendQUIT(AsyncNetworker *NW);



void DestroyNetworker(AsyncNetworker *NW)
{
}

#define NWC_DBG_SEND() syslog(LOG_DEBUG, "NW client[%ld]: > %s", NW->n, ChrPtr(NW->IO.SendBuf.Buf))
#define NWC_DBG_READ() syslog(LOG_DEBUG, "NW client[%ld]: < %s\n", NW->n, ChrPtr(NW->IO.IOBuf))
#define NWC_OK (strncasecmp(ChrPtr(NW->IO.IOBuf), "+OK", 3) == 0)

eNextState NWC_ReadGreeting(AsyncNetworker *NW)
{
	char connected_to[SIZ];
	NWC_DBG_READ();
	/* Read the server greeting */
	/* Check that the remote is who we think it is and warn the Aide if not */
	extract_token (connected_to, ChrPtr(NW->IO.IOBuf), 1, ' ', sizeof connected_to);
	if (strcmp(connected_to, ChrPtr(NW->node)) != 0)
	{
		StrBufPrintf(NW->IO.ErrMsg,
			     "Connected to node \"%s\" but I was expecting to connect to node \"%s\".",
			     connected_to, ChrPtr(NW->node));
		syslog(LOG_ERR, "%s\n", ChrPtr(NW->IO.ErrMsg));
		CtdlAideMessage(ChrPtr(NW->IO.ErrMsg), "Network error");
		return eAbort;/// todo: aide message in anderer queue speichern
	}
	return eSendReply;
}

eNextState NWC_SendAuth(AsyncNetworker *NW)
{
	/* We're talking to the correct node.  Now identify ourselves. */
	StrBufPrintf(NW->IO.SendBuf.Buf, "NETP %s|%s\n", 
		     config.c_nodename, 
		     ChrPtr(NW->secret));
	NWC_DBG_SEND();
	return eSendReply;
}

eNextState NWC_ReadAuthReply(AsyncNetworker *NW)
{
	NWC_DBG_READ();
	if (ChrPtr(NW->IO.IOBuf)[0] == '2')
	{
		return eSendReply;
	}
	else
	{
		StrBufPrintf(NW->IO.ErrMsg,
			     "Connected to node \"%s\" but my secret wasn't accurate.",
			     ChrPtr(NW->node));
		syslog(LOG_ERR, "%s\n", ChrPtr(NW->IO.ErrMsg));
		CtdlAideMessage(ChrPtr(NW->IO.ErrMsg), "Network error");
		
		return eAbort;
	}
}

eNextState NWC_SendNDOP(AsyncNetworker *NW)
{
	NW->tempFileName = NewStrBuf();
	NW->SpoolFileName = NewStrBuf();
	StrBufPrintf(NW->tempFileName, 
		     "%s/%s.%lx%x",
		     ctdl_netin_dir,
		     ChrPtr(NW->node),
		     time(NULL),// TODO: get time from libev
		     rand());
	StrBufPrintf(NW->SpoolFileName, 
		     "%s/%s.%lx%x",
		     ctdl_nettmp_dir,
		     ChrPtr(NW->node),
		     time(NULL),// TODO: get time from libev
		     rand());

	/* We're talking to the correct node.  Now identify ourselves. */
	StrBufPlain(NW->IO.SendBuf.Buf, HKEY("NDOP\n"));
	NWC_DBG_SEND();
	return eSendReply;
}

eNextState NWC_ReadNDOPReply(AsyncNetworker *NW)
{
	NWC_DBG_READ();
	if (ChrPtr(NW->IO.IOBuf)[0] == '2')
	{
		NW->download_len = atol (ChrPtr(NW->IO.IOBuf) + 4);
		syslog(LOG_DEBUG, "Expecting to transfer %ld bytes\n", NW->download_len);
		if (NW->download_len <= 0)
			NW->State = eNUOP - 1;
		return eSendReply;
	}
	else
	{
		return eAbort;
	}
}

eNextState NWC_SendREAD(AsyncNetworker *NW)
{
	if (NW->bytes_received < NW->download_len)
	{
		/*
		 * If shutting down we can exit here and unlink the temp file.
		 * this shouldn't loose us any messages.
		 */
		if (server_shutting_down)
		{
			fclose(NW->TmpFile);
			unlink(ChrPtr(NW->tempFileName));
			return eAbort;
		}
		StrBufPrintf(NW->IO.SendBuf.Buf, "READ %ld|%ld\n",
			     NW->bytes_received,
			     ((NW->download_len - NW->bytes_received > IGNET_PACKET_SIZE)
			      ? IGNET_PACKET_SIZE : 
			      (NW->download_len - NW->bytes_received))
			);
		return eSendReply;



	}
	else {} // continue sending
	return eSendReply;
}

eNextState NWC_ReadREADState(AsyncNetworker *NW)
{
	NWC_DBG_READ();
	if (ChrPtr(NW->IO.IOBuf)[0] == '6')
	{
		NW->BlobReadSize = atol(ChrPtr(NW->IO.IOBuf)+4);
/// TODO		StrBufReadjustIOBuffer(NW->IO.RecvBuf, NW->BlobReadSize);
		return eReadPayload;
	}
	return eAbort;
}
eNextState NWC_ReadREADBlob(AsyncNetworker *NW)
{
	fwrite(ChrPtr(NW->IO.RecvBuf.Buf), NW->BlobReadSize, 1, NW->TmpFile);
	NW->bytes_received += NW->BlobReadSize;
	/// FlushIOBuffer(NW->IO.RecvBuf); /// TODO
	if (NW->bytes_received < NW->download_len)
	{
		return eSendReply;/* now fetch next chunk*/
	}
	else 
	{
		fclose(NW->TmpFile);
		
		if (link(ChrPtr(NW->tempFileName), ChrPtr(NW->SpoolFileName)) != 0) {
			syslog(LOG_ALERT, 
			       "Could not link %s to %s: %s\n",
			       ChrPtr(NW->tempFileName), 
			       ChrPtr(NW->SpoolFileName), 
			       strerror(errno));
		}
	
		unlink(ChrPtr(NW->tempFileName));
		return eSendReply; //// TODO: step forward.
	}
}


eNextState NWC_SendCLOS(AsyncNetworker *NW)
{
	StrBufPlain(NW->IO.SendBuf.Buf, HKEY("CLOS\n"));
	unlink(ChrPtr(NW->tempFileName));
	return eReadMessage;
}

eNextState NWC_ReadCLOSReply(AsyncNetworker *NW)
{
/// todo
	return eTerminateConnection;
}


eNextState NWC_SendNUOP(AsyncNetworker *NW)
{
	struct stat statbuf;

	StrBufPrintf(NW->tempFileName,
		     "%s/%s",
		     ctdl_netout_dir,
		     ChrPtr(NW->node));
	NW->fd = open(ChrPtr(NW->tempFileName), O_RDONLY);
	if (NW->fd < 0) {
		if (errno != ENOENT) {
			syslog(LOG_CRIT,
			       "cannot open %s: %s\n", 
			       ChrPtr(NW->tempFileName), 
			       strerror(errno));
		}
		NW->State = eQUIT;
		return NWC_SendQUIT(NW);
	}

	if (fstat(NW->fd, &statbuf) == -1) {
		syslog(9, "FSTAT FAILED %s [%s]--\n", 
		       ChrPtr(NW->tempFileName), 
		       strerror(errno));
		if (NW->fd > 0) close(NW->fd);
		return eAbort;
	}
	
	NW->download_len = statbuf.st_size;
	if (NW->download_len == 0) {
		syslog(LOG_DEBUG,
		       "Nothing to send.\n");
		NW->State = eQUIT;
		return NWC_SendQUIT(NW);
	}

	NW->bytes_written = 0;

	StrBufPlain(NW->IO.SendBuf.Buf, HKEY("NUOP\n"));
	return eSendReply;

}
eNextState NWC_ReadNUOPReply(AsyncNetworker *NW)
{
	NWC_DBG_READ();
///	if (ChrPtr(NW->IO.IOBuf)[0] == '2');;;; //// todo
	return eReadMessage;
}

eNextState NWC_SendWRIT(AsyncNetworker *NW)
{
	StrBufPrintf(NW->IO.SendBuf.Buf, "WRIT %ld\n", NW->bytes_to_write);

	return eSendReply;
}
eNextState NWC_ReadWRITReply(AsyncNetworker *NW)
{
	NWC_DBG_READ();
	if (ChrPtr(NW->IO.IOBuf)[0] != '7')
	{
		return eAbort;
	}

	NW->BlobReadSize = atol(ChrPtr(NW->IO.IOBuf)+4);
	return eSendMore;
}

eNextState NWC_SendBlob(AsyncNetworker *NW)
{

	///			bytes_to_write -= thisblock;
	///			bytes_written += thisblock;

	return eReadMessage;
}

eNextState NWC_SendUCLS(AsyncNetworker *NW)
{
	StrBufPlain(NW->IO.SendBuf.Buf, HKEY("UCLS 1\n"));
	return eSendReply;

}
eNextState NWC_ReadUCLS(AsyncNetworker *NW)
{
	NWC_DBG_READ();

	syslog(LOG_NOTICE, "Sent %ld octets to <%s>\n", NW->bytes_written, ChrPtr(NW->node));
///	syslog(LOG_DEBUG, "<%s\n", buf);
	if (ChrPtr(NW->IO.IOBuf)[0] == '2') {
		syslog(LOG_DEBUG, "Removing <%s>\n", ChrPtr(NW->tempFileName));
		unlink(ChrPtr(NW->tempFileName));
	}
	return eSendReply;
}

eNextState NWC_SendQUIT(AsyncNetworker *NW)
{
	StrBufPlain(NW->IO.SendBuf.Buf, HKEY("QUIT\n"));

	network_talking_to(ChrPtr(NW->node), NTT_REMOVE);
	return eSendReply;
}

eNextState NWC_ReadQUIT(AsyncNetworker *NW)
{
	NWC_DBG_READ();

	return eTerminateConnection;
}


NWClientHandler NWC_ReadHandlers[] = {
	NWC_ReadGreeting,
	NWC_ReadAuthReply,
	NWC_ReadNDOPReply,
	NWC_ReadREADState,
	NWC_ReadREADBlob,
	NWC_ReadCLOSReply,
	NWC_ReadNUOPReply,
	NWC_ReadWRITReply,
	NULL,
	NWC_ReadUCLS,
	NWC_ReadQUIT
};

const long NWC_SendTimeouts[] = {
	100,
	100,
	100,
	100,
	100,
	100,
	100,
	100
};
const ConstStr NWC[] = {
	{HKEY("Connection broken during ")},
	{HKEY("Connection broken during ")},
	{HKEY("Connection broken during ")},
	{HKEY("Connection broken during ")},
	{HKEY("Connection broken during ")},
	{HKEY("Connection broken during ")},
	{HKEY("Connection broken during ")},
	{HKEY("Connection broken during ")}
};

NWClientHandler NWC_SendHandlers[] = {
	NULL,
	NWC_SendAuth,
	NWC_SendNDOP,
	NWC_SendREAD,
	NULL,
	NWC_SendCLOS,
	NWC_SendNUOP,
	NWC_SendWRIT,
	NWC_SendBlob,
	NWC_SendUCLS,
	NWC_SendQUIT
};

const long NWC_ReadTimeouts[] = {
	100,
	100,
	100,
	100,
	100,
	100,
	100,
	100,
	100,
	100
};




eNextState nwc_get_one_host_ip_done(AsyncIO *IO)
{
	AsyncNetworker *NW = IO->Data;
	struct hostent *hostent;

	QueryCbDone(IO);

	hostent = NW->HostLookup.VParsedDNSReply;
	if ((NW->HostLookup.DNSStatus == ARES_SUCCESS) && 
	    (hostent != NULL) ) {
		memset(&NW->IO.ConnectMe->Addr, 0, sizeof(struct in6_addr));
		if (NW->IO.ConnectMe->IPv6) {
			memcpy(&NW->IO.ConnectMe->Addr.sin6_addr.s6_addr, 
			       &hostent->h_addr_list[0],
			       sizeof(struct in6_addr));
			
			NW->IO.ConnectMe->Addr.sin6_family = hostent->h_addrtype;
			NW->IO.ConnectMe->Addr.sin6_port   = htons(atol(ChrPtr(NW->port)));//// TODO use the one from the URL.
		}
		else {
			struct sockaddr_in *addr = (struct sockaddr_in*) &NW->IO.ConnectMe->Addr;
			/* Bypass the ns lookup result like this: IO->Addr.sin_addr.s_addr = inet_addr("127.0.0.1"); */
//			addr->sin_addr.s_addr = htonl((uint32_t)&hostent->h_addr_list[0]);
			memcpy(&addr->sin_addr.s_addr, 
			       hostent->h_addr_list[0], 
			       sizeof(uint32_t));
			
			addr->sin_family = hostent->h_addrtype;
			addr->sin_port   = htons(504);/// default citadel port
			
		}
		return nwc_connect_ip(IO);
	}
	else
		return eAbort;
}


eNextState nwc_get_one_host_ip(AsyncIO *IO)
{
	AsyncNetworker *NW = IO->Data;
	/* 
	 * here we start with the lookup of one host. it might be...
	 * - the relay host *sigh*
	 * - the direct hostname if there was no mx record
	 * - one of the mx'es
	 */ 

	InitC_ares_dns(IO);

	syslog(LOG_DEBUG, "NWC: %s\n", __FUNCTION__);

	syslog(LOG_DEBUG, 
		      "NWC client[%ld]: looking up %s-Record %s : %d ...\n", 
		      NW->n, 
		      (NW->IO.ConnectMe->IPv6)? "aaaa": "a",
		      NW->IO.ConnectMe->Host, 
		      NW->IO.ConnectMe->Port);

	QueueQuery((NW->IO.ConnectMe->IPv6)? ns_t_aaaa : ns_t_a, 
		   NW->IO.ConnectMe->Host, 
		   &NW->IO, 
		   &NW->HostLookup, 
		   nwc_get_one_host_ip_done);
	IO->NextState = eReadDNSReply;
	return IO->NextState;
}
/**
 * @brief lineread Handler; understands when to read more POP3 lines, and when this is a one-lined reply.
 */
eReadState NWC_ReadServerStatus(AsyncIO *IO)
{
	eReadState Finished = eBufferNotEmpty; 

	switch (IO->NextState) {
	case eSendDNSQuery:
	case eReadDNSReply:
	case eDBQuery:
	case eConnect:
	case eTerminateConnection:
	case eAbort:
		Finished = eReadFail;
		break;
	case eSendReply: 
	case eSendMore:
	case eReadMore:
	case eReadMessage: 
		Finished = StrBufChunkSipLine(IO->IOBuf, &IO->RecvBuf);
		break;
	case eReadPayload:
		break;
	}
	return Finished;
}



eNextState NWC_FailNetworkConnection(AsyncIO *IO)
{
	return eTerminateConnection;
}


eNextState NWC_DispatchReadDone(AsyncIO *IO)
{
	syslog(LOG_DEBUG, "NWC: %s\n", __FUNCTION__);
	AsyncNetworker *NW = IO->Data;
	eNextState rc;

	rc = NWC_ReadHandlers[NW->State](NW);
	if (rc != eReadMore)
		NW->State++;
	////NWCSetTimeout(rc, NW);
	return rc;
}
eNextState NWC_DispatchWriteDone(AsyncIO *IO)
{
	syslog(LOG_DEBUG, "NWC: %s\n", __FUNCTION__);
	AsyncNetworker *NW = IO->Data;
	eNextState rc;

	rc = NWC_SendHandlers[NW->State](NW);
	////NWCSetTimeout(rc, NW);
	return rc;
}

/*****************************************************************************/
/*                     Networker CLIENT ERROR CATCHERS                       */
/*****************************************************************************/
eNextState NWC_Terminate(AsyncIO *IO)
{
	syslog(LOG_DEBUG, "Nw: %s\n", __FUNCTION__);
///	FinalizeNetworker(IO); TODO
	return eAbort;
}

eNextState NWC_Timeout(AsyncIO *IO)
{
//	AsyncNetworker *NW = IO->Data;

	syslog(LOG_DEBUG, "NW: %s\n", __FUNCTION__);
//	StrBufPlain(IO->ErrMsg, CKEY(POP3C_ReadErrors[pMsg->State])); todo
	return NWC_FailNetworkConnection(IO);
}
eNextState NWC_ConnFail(AsyncIO *IO)
{
///	AsyncNetworker *NW = IO->Data;

	syslog(LOG_DEBUG, "NW: %s\n", __FUNCTION__);
////	StrBufPlain(IO->ErrMsg, CKEY(POP3C_ReadErrors[pMsg->State])); todo
	return NWC_FailNetworkConnection(IO);
}
eNextState NWC_Shutdown(AsyncIO *IO)
{
	syslog(LOG_DEBUG, "NW: %s\n", __FUNCTION__);
////	pop3aggr *pMsg = IO->Data;

	////pMsg->MyQEntry->Status = 3;
	///StrBufPlain(pMsg->MyQEntry->StatusMessage, HKEY("server shutdown during message retrieval."));
///	FinalizePOP3AggrRun(IO); todo
	return eAbort;
}


eNextState nwc_connect_ip(AsyncIO *IO)
{
	AsyncNetworker *NW = IO->Data;

	syslog(LOG_DEBUG, "NW: %s\n", __FUNCTION__);
	syslog(LOG_DEBUG, "network: polling <%s>\n", ChrPtr(NW->node));
	syslog(LOG_NOTICE, "Connecting to <%s> at %s:%s\n", 
	       ChrPtr(NW->node), 
	       ChrPtr(NW->host),
	       ChrPtr(NW->port));
	
////	IO->ConnectMe = &NW->Pop3Host;
	/*  Bypass the ns lookup result like this: IO->Addr.sin_addr.s_addr = inet_addr("127.0.0.1"); */

	/////// SetConnectStatus(IO);

	return InitEventIO(IO, NW, 100, 100, 1); /*
						 NWC_ConnTimeout, 
						 NWC_ReadTimeouts[0],
						 1);*/
}

void RunNetworker(AsyncNetworker *NW)
{
	CitContext *SubC;

	ParseURL(&NW->IO.ConnectMe, NW->Url, 504);

	NW->IO.Data          = NW;
	NW->IO.SendDone      = NWC_DispatchWriteDone;
	NW->IO.ReadDone      = NWC_DispatchReadDone;
	NW->IO.Terminate     = NWC_Terminate;
	NW->IO.LineReader    = NWC_ReadServerStatus;
	NW->IO.ConnFail      = NWC_ConnFail;
	NW->IO.Timeout       = NWC_Timeout;
	NW->IO.ShutdownAbort = NWC_Shutdown;
	
	NW->IO.SendBuf.Buf   = NewStrBufPlain(NULL, 1024);
	NW->IO.RecvBuf.Buf   = NewStrBufPlain(NULL, 1024);
	NW->IO.IOBuf         = NewStrBuf();
	
	NW->IO.NextState     = eReadMessage;
	SubC = CloneContext (&networker_client_CC);
	SubC->session_specific_data = (char*) NW;
	NW->IO.CitContext = SubC;

	if (NW->IO.ConnectMe->IsIP) {
		QueueEventContext(&NW->IO,
				  nwc_connect_ip);
	}
	else { /* uneducated admin has chosen to add DNS to the equation... */
		QueueEventContext(&NW->IO,
				  nwc_get_one_host_ip);
	}
}

/*
 * Poll other Citadel nodes and transfer inbound/outbound network data.
 * Set "full" to nonzero to force a poll of every node, or to zero to poll
 * only nodes to which we have data to send.
 */
void network_poll_other_citadel_nodes(int full_poll, char *working_ignetcfg)
{
	AsyncNetworker *NW;
	StrBuf *CfgData;
	StrBuf *Line;
	const char *lptr;
	const char *CfgPtr;
	int Done;
	
	int poll = 0;
	
	if ((working_ignetcfg == NULL) || (*working_ignetcfg == '\0')) {
		syslog(LOG_DEBUG, "network: no neighbor nodes are configured - not polling.\n");
		return;
	}
	CfgData = NewStrBufPlain(working_ignetcfg, -1);
	Line = NewStrBufPlain(NULL, StrLength(CfgData));
	Done = 0;
	CfgPtr = NULL;
	while (!Done)
	{
		/* Use the string tokenizer to grab one line at a time */
		StrBufSipLine(Line, CfgData, &CfgPtr);
		Done = CfgPtr == StrBufNOTNULL;
		if (StrLength(Line) > 0)
		{
			if(server_shutting_down)
				return;/* TODO free stuff*/
			lptr = NULL;
			poll = 0;
			NW = (AsyncNetworker*)malloc(sizeof(AsyncNetworker));
			memset(NW, 0, sizeof(AsyncNetworker));
			
			NW->node = NewStrBufPlain(NULL, StrLength(Line));
			NW->host = NewStrBufPlain(NULL, StrLength(Line));
			NW->port = NewStrBufPlain(NULL, StrLength(Line));
			NW->secret = NewStrBufPlain(NULL, StrLength(Line));
			
			StrBufExtract_NextToken(NW->node, Line, &lptr, '|');
			StrBufExtract_NextToken(NW->secret, Line, &lptr, '|');
			StrBufExtract_NextToken(NW->host, Line, &lptr, '|');
			StrBufExtract_NextToken(NW->port, Line, &lptr, '|');
			if ( (StrLength(NW->node) != 0) && 
			     (StrLength(NW->secret) != 0) &&
			     (StrLength(NW->host) != 0) &&
			     (StrLength(NW->port) != 0))
			{
				poll = full_poll;
				if (poll == 0)
				{
					NW->SpoolFileName = NewStrBufPlain(ctdl_netout_dir, -1);
					StrBufAppendBufPlain(NW->SpoolFileName, HKEY("/"), 0);
					StrBufAppendBuf(NW->SpoolFileName, NW->node, 0);
					if (access(ChrPtr(NW->SpoolFileName), R_OK) == 0) {
						poll = 1;
					}
				}
			}
			if (poll) {
				NW->Url = NewStrBufPlain(NULL, StrLength(Line));
				StrBufPrintf(NW->Url, "citadel://:%s@%s:%s", 
					     ChrPtr(NW->secret),
					     ChrPtr(NW->host),
					     ChrPtr(NW->port));
				if (!network_talking_to(ChrPtr(NW->node), NTT_CHECK))
				{
					network_talking_to(ChrPtr(NW->node), NTT_ADD);
					RunNetworker(NW);
					continue;
				}
			}
			DestroyNetworker(NW);
		}
	}

}


void network_do_clientqueue(void)
{
	char *working_ignetcfg;
	int full_processing = 1;
	static time_t last_run = 0L;

	/*
	 * Run the full set of processing tasks no more frequently
	 * than once every n seconds
	 */
	if ( (time(NULL) - last_run) < config.c_net_freq ) {
		full_processing = 0;
		syslog(LOG_DEBUG, "Network full processing in %ld seconds.\n",
			config.c_net_freq - (time(NULL)- last_run)
		);
	}

	working_ignetcfg = load_working_ignetcfg();
	/*
	 * Poll other Citadel nodes.  Maybe.  If "full_processing" is set
	 * then we poll everyone.  Otherwise we only poll nodes we have stuff
	 * to send to.
	 */
	network_poll_other_citadel_nodes(full_processing, working_ignetcfg);
	if (working_ignetcfg)
		free(working_ignetcfg);
}



/*
 * Module entry point
 */
CTDL_MODULE_INIT(network_client)
{
	if (!threading)
	{
		CtdlFillSystemContext(&networker_client_CC, "CitNetworker");
		
		CtdlRegisterSessionHook(network_do_clientqueue, EVT_TIMER);
	}
	return "network_client";
}
