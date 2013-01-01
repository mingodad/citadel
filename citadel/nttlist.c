/*
 * This module handles shared rooms, inter-Citadel mail, and outbound
 * mailing list processing.
 *
 * Copyright (c) 2000-2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include "sysdep.h"
#include <stdio.h>
#include <libcitadel.h>
#include "ctdl_module.h"
#include "serv_extensions.h"

/*-----------------------------------------------------------------------------*
 *                 Network maps: evaluate other nodes                          *
 *-----------------------------------------------------------------------------*/
int NTTDebugEnabled = 0;

/*
 * network_talking_to()  --  concurrency checker
 */
static HashList *nttlist = NULL;
int CtdlNetworkTalkingTo(const char *nodename, long len, int operation)
{

	int retval = 0;
	HashPos *Pos = NULL;
	void *vdata;

	begin_critical_section(S_NTTLIST);

	switch(operation) {

		case NTT_ADD:
			if (nttlist == NULL) 
				nttlist = NewHash(1, NULL);
			Put(nttlist, nodename, len, NewStrBufPlain(nodename, len), HFreeStrBuf);
			if (NTTDebugEnabled) syslog(LOG_DEBUG, "nttlist: added <%s>\n", nodename);
			break;
		case NTT_REMOVE:
			if ((nttlist == NULL) ||
			    (GetCount(nttlist) == 0))
				break;
			Pos = GetNewHashPos(nttlist, 1);
			if (GetHashPosFromKey (nttlist, nodename, len, Pos))
				DeleteEntryFromHash(nttlist, Pos);
			DeleteHashPos(&Pos);
			if (NTTDebugEnabled) syslog(LOG_DEBUG, "nttlist: removed <%s>\n", nodename);

			break;

		case NTT_CHECK:
			if ((nttlist == NULL) ||
			    (GetCount(nttlist) == 0))
				break;
			if (GetHash(nttlist, nodename, len, &vdata))
				retval ++;
			if (NTTDebugEnabled) syslog(LOG_DEBUG, "nttlist: have [%d] <%s>\n", retval, nodename);
			break;
	}

	end_critical_section(S_NTTLIST);
	return(retval);
}

void cleanup_nttlist(void)
{
        begin_critical_section(S_NTTLIST);
	DeleteHash(&nttlist);
        end_critical_section(S_NTTLIST);
}



/*
 * Module entry point
 */
void SetNTTDebugEnabled(const int n)
{
	NTTDebugEnabled = n;
}



/*
 * Module entry point
 */
CTDL_MODULE_INIT(nttlist)
{
	if (!threading)
	{
		CtdlRegisterDebugFlagHook(HKEY("networktalkingto"), SetNTTDebugEnabled, &NTTDebugEnabled);

		CtdlRegisterCleanupHook(cleanup_nttlist);
	}
	return "nttlist";
}
