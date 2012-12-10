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
 * ** NOTE **   A word on the S_NETCONFIGS semaphore:
 * This is a fairly high-level type of critical section.  It ensures that no
 * two threads work on the netconfigs files at the same time.  Since we do
 * so many things inside these, here are the rules:
 *  1. begin_critical_section(S_NETCONFIGS) *before* begin_ any others.
 *  2. Do *not* perform any I/O with the client during these sections.
 *
 */

/*
 * Duration of time (in seconds) after which pending list subscribe/unsubscribe
 * requests that have not been confirmed will be deleted.
 */
#define EXP	259200	/* three days */

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
#include "context.h"
#include "netconfig.h"
#include "netspool.h"
#include "ctdl_module.h"



void DeleteNodeConf(void *vNode)
{
	NodeConf *Node = (NodeConf*) vNode;
	FreeStrBuf(&Node->NodeName);
	FreeStrBuf(&Node->Secret);
	FreeStrBuf(&Node->Host);
	FreeStrBuf(&Node->Port);
	free(Node);
}

NodeConf *NewNode(StrBuf *SerializedNode)
{
	const char *Pos = NULL;
	NodeConf *Node;

	/* we need at least 4 pipes and some other text so its invalid. */
	if (StrLength(SerializedNode) < 8)
		return NULL;
	Node = (NodeConf *) malloc(sizeof(NodeConf));

	Node->DeleteMe = 0;

	Node->NodeName=NewStrBuf();
	StrBufExtract_NextToken(Node->NodeName, SerializedNode, &Pos, '|');

	Node->Secret=NewStrBuf();
	StrBufExtract_NextToken(Node->Secret, SerializedNode, &Pos, '|');

	Node->Host=NewStrBuf();
	StrBufExtract_NextToken(Node->Host, SerializedNode, &Pos, '|');

	Node->Port=NewStrBuf();
	StrBufExtract_NextToken(Node->Port, SerializedNode, &Pos, '|');
	return Node;
}


/*
 * Load or refresh the Citadel network (IGnet) configuration for this node.
 */
HashList* load_ignetcfg(void)
{
	const char *LinePos;
	char       *Cfg;
	StrBuf     *Buf;
	StrBuf     *LineBuf;
	HashList   *Hash;
	NodeConf   *Node;

	Cfg =  CtdlGetSysConfig(IGNETCFG);
	if ((Cfg == NULL) || IsEmptyStr(Cfg)) {
		if (Cfg != NULL)
			free(Cfg);
		return NULL;
	}

	Hash = NewHash(1, NULL);
	Buf = NewStrBufPlain(Cfg, -1);
	free(Cfg);
	LineBuf = NewStrBufPlain(NULL, StrLength(Buf));
	LinePos = NULL;
	do
	{
		StrBufSipLine(LineBuf, Buf, &LinePos);
		if (StrLength(LineBuf) != 0) {
			Node = NewNode(LineBuf);
			if (Node != NULL) {
				Put(Hash, SKEY(Node->NodeName), Node, DeleteNodeConf);
			}
		}
	} while (LinePos != StrBufNOTNULL);
	FreeStrBuf(&Buf);
	FreeStrBuf(&LineBuf);
	return Hash;
}

void DeleteNetMap(void *vNetMap)
{
	NetMap *TheNetMap = (NetMap*) vNetMap;
	FreeStrBuf(&TheNetMap->NodeName);
	FreeStrBuf(&TheNetMap->NextHop);
	free(TheNetMap);
}

NetMap *NewNetMap(StrBuf *SerializedNetMap)
{
	const char *Pos = NULL;
	NetMap *NM;

	/* we need at least 3 pipes and some other text so its invalid. */
	if (StrLength(SerializedNetMap) < 6)
		return NULL;
	NM = (NetMap *) malloc(sizeof(NetMap));

	NM->NodeName=NewStrBuf();
	StrBufExtract_NextToken(NM->NodeName, SerializedNetMap, &Pos, '|');

	NM->lastcontact = StrBufExtractNext_long(SerializedNetMap, &Pos, '|');

	NM->NextHop=NewStrBuf();
	StrBufExtract_NextToken(NM->NextHop, SerializedNetMap, &Pos, '|');

	return NM;
}

HashList* read_network_map(void)
{
	const char *LinePos;
	char       *Cfg;
	StrBuf     *Buf;
	StrBuf     *LineBuf;
	HashList   *Hash;
	NetMap     *TheNetMap;

	Cfg =  CtdlGetSysConfig(IGNETMAP);
	if ((Cfg == NULL) || IsEmptyStr(Cfg)) {
		if (Cfg != NULL)
			free(Cfg);
		return NULL;
	}

	Hash = NewHash(1, NULL);
	Buf = NewStrBufPlain(Cfg, -1);
	free(Cfg);
	LineBuf = NewStrBufPlain(NULL, StrLength(Buf));
	LinePos = NULL;
	while (StrBufSipLine(Buf, LineBuf, &LinePos))
	{
		TheNetMap = NewNetMap(LineBuf);
		if (TheNetMap != NULL) { /* TODO: is the NodeName Uniq? */
			Put(Hash, SKEY(TheNetMap->NodeName), TheNetMap, DeleteNetMap);
		}
	}
	FreeStrBuf(&Buf);
	FreeStrBuf(&LineBuf);
	return Hash;
}

StrBuf *SerializeNetworkMap(HashList *Map)
{
	void *vMap;
	const char *key;
	long len;
	StrBuf *Ret = NewStrBuf();
	HashPos *Pos = GetNewHashPos(Map, 0);

	while (GetNextHashPos(Map, Pos, &len, &key, &vMap))
	{
		NetMap *pMap = (NetMap*) vMap;
		StrBufAppendBuf(Ret, pMap->NodeName, 0);
		StrBufAppendBufPlain(Ret, HKEY("|"), 0);

		StrBufAppendPrintf(Ret, "%ld", pMap->lastcontact, 0);
		StrBufAppendBufPlain(Ret, HKEY("|"), 0);

		StrBufAppendBuf(Ret, pMap->NextHop, 0);
		StrBufAppendBufPlain(Ret, HKEY("\n"), 0);
	}
	DeleteHashPos(&Pos);
	return Ret;
}


/*
 * Learn topology from path fields
 */
void network_learn_topology(char *node, char *path, HashList *the_netmap, int *netmap_changed)
{
	NetMap *pNM = NULL;
	void *vptr;
	char nexthop[256];
	NetMap *nmptr;

	if (GetHash(the_netmap, node, strlen(node), &vptr) && 
	    (vptr != NULL))/* TODO: is the NodeName Uniq? */
	{
		pNM = (NetMap*)vptr;
		extract_token(nexthop, path, 0, '!', sizeof nexthop);
		if (!strcmp(nexthop, ChrPtr(pNM->NextHop))) {
			pNM->lastcontact = time(NULL);
			(*netmap_changed) ++;
			return;
		}
	}

	/* If we got here then it's not in the map, so add it. */
	nmptr = (NetMap *) malloc(sizeof (NetMap));
	nmptr->NodeName = NewStrBufPlain(node, -1);
	nmptr->lastcontact = time(NULL);
	nmptr->NextHop = NewStrBuf ();
	StrBufExtract_tokenFromStr(nmptr->NextHop, path, strlen(path), 0, '!');
	/* TODO: is the NodeName Uniq? */
	Put(the_netmap, SKEY(nmptr->NodeName), nmptr, DeleteNetMap);
	(*netmap_changed) ++;
}


/*
 * Check the network map and determine whether the supplied node name is
 * valid.  If it is not a neighbor node, supply the name of a neighbor node
 * which is the next hop.  If it *is* a neighbor node, we also fill in the
 * shared secret.
 */
int is_valid_node(const StrBuf **nexthop,
		  const StrBuf **secret,
		  StrBuf *node,
		  HashList *IgnetCfg,
		  HashList *the_netmap)
{
	void *vNetMap;
	void *vNodeConf;
	NodeConf *TheNode;
	NetMap *TheNetMap;

	if (StrLength(node) == 0) {
		return(-1);
	}

	/*
	 * First try the neighbor nodes
	 */
	if (GetCount(IgnetCfg) == 0) {
		syslog(LOG_INFO, "IgnetCfg is empty!\n");
		if (nexthop != NULL) {
			*nexthop = NULL;
		}
		return(-1);
	}

	/* try to find a neigbour with the name 'node' */
	if (GetHash(IgnetCfg, SKEY(node), &vNodeConf) && 
	    (vNodeConf != NULL))
	{
		TheNode = (NodeConf*)vNodeConf;
		if (secret != NULL)
			*secret = TheNode->Secret;
		return 0;		/* yup, it's a direct neighbor */
	}

	/*
	 * If we get to this point we have to see if we know the next hop
	 *//* TODO: is the NodeName Uniq? */
	if ((GetCount(the_netmap) > 0) &&
	    (GetHash(the_netmap, SKEY(node), &vNetMap)))
	{
		TheNetMap = (NetMap*)vNetMap;
		if (nexthop != NULL)
			*nexthop = TheNetMap->NextHop;
		return(0);
	}

	/*
	 * If we get to this point, the supplied node name is bogus.
	 */
	syslog(LOG_ERR, "Invalid node name <%s>\n", ChrPtr(node));
	return(-1);
}


void cmd_gnet(char *argbuf)
{
	char filename[PATH_MAX];
	char buf[SIZ];
	FILE *fp;


	if (!IsEmptyStr(argbuf))
	{
		if (CtdlAccessCheck(ac_aide)) return;
		if (strcmp(argbuf, FILE_MAILALIAS))
		{
			cprintf("%d No such file or directory\n",
				ERROR + INTERNAL_ERROR);
			return;
		}
		safestrncpy(filename, file_mail_aliases, sizeof(filename));
		cprintf("%d Settings for <%s>\n",
			LISTING_FOLLOWS,
			filename);
	}
	else
	{
		if ( (CC->room.QRflags & QR_MAILBOX) && (CC->user.usernum == atol(CC->room.QRname)) ) {
			/* users can edit the netconfigs for their own mailbox rooms */
		}
		else if (CtdlAccessCheck(ac_room_aide)) return;
		
		assoc_file_name(filename, sizeof filename, &CC->room, ctdl_netcfg_dir);
		cprintf("%d Network settings for room #%ld <%s>\n",
			LISTING_FOLLOWS,
			CC->room.QRnumber, CC->room.QRname);
	}

	fp = fopen(filename, "r");
	if (fp != NULL) {
		while (fgets(buf, sizeof buf, fp) != NULL) {
			buf[strlen(buf)-1] = 0;
			cprintf("%s\n", buf);
		}
		fclose(fp);
	}

	cprintf("000\n");
}

#define nForceAliases 5
const ConstStr ForceAliases[nForceAliases] = {
	{HKEY("bbs,")},
	{HKEY("root,")},
	{HKEY("Auto,")},
	{HKEY("postmaster,")},
	{HKEY("abuse,")}
};

void cmd_snet(char *argbuf) {
	char tempfilename[PATH_MAX];
	char filename[PATH_MAX];
	int TmpFD;
	StrBuf *Line;
	struct stat StatBuf;
	long len;
	int rc;
	int IsMailAlias = 0;
	int MailAliasesFound[nForceAliases];

	unbuffer_output();

	if (!IsEmptyStr(argbuf))
	{
		if (CtdlAccessCheck(ac_aide)) return;
		if (strcmp(argbuf, FILE_MAILALIAS))
		{
			cprintf("%d No such file or directory\n",
				ERROR + INTERNAL_ERROR);
			return;
		}
		safestrncpy(filename, file_mail_aliases, sizeof(filename));
		memset(MailAliasesFound, 0, sizeof(MailAliasesFound));
		IsMailAlias = 1;
	}
	else
	{
		if ( (CC->room.QRflags & QR_MAILBOX) && (CC->user.usernum == atol(CC->room.QRname)) ) {
			/* users can edit the netconfigs for their own mailbox rooms */
		}
		else if (CtdlAccessCheck(ac_room_aide)) return;
		
		len = assoc_file_name(filename, sizeof filename, &CC->room, ctdl_netcfg_dir);
		memcpy(tempfilename, filename, len + 1);
	}
	memset(&StatBuf, 0, sizeof(struct stat));
	if ((stat(filename, &StatBuf)  == -1) || (StatBuf.st_size == 0))
		StatBuf.st_size = 80; /* Not there or empty? guess 80 chars line. */

	sprintf(tempfilename + len, ".%d", CC->cs_pid);
	errno = 0;
	TmpFD = open(tempfilename, O_CREAT|O_EXCL|O_RDWR, S_IRUSR|S_IWUSR);

	if ((TmpFD > 0) && (errno == 0))
	{
		char *tmp = malloc(StatBuf.st_size * 2);
		memset(tmp, ' ', StatBuf.st_size * 2);
		rc = write(TmpFD, tmp, StatBuf.st_size * 2);
		free(tmp);
		if ((rc <= 0) || (rc != StatBuf.st_size * 2))
		{
			close(TmpFD);
			cprintf("%d Unable to allocate the space required for %s: %s\n",
				ERROR + INTERNAL_ERROR,
				tempfilename,
				strerror(errno));
			unlink(tempfilename);
			return;
		}	
		lseek(TmpFD, SEEK_SET, 0);
	}
	else {
		cprintf("%d Unable to allocate the space required for %s: %s\n",
			ERROR + INTERNAL_ERROR,
			tempfilename,
			strerror(errno));
		unlink(tempfilename);
		return;
	}
	Line = NewStrBuf();

	cprintf("%d %s\n", SEND_LISTING, tempfilename);

	len = 0;
	while (rc = CtdlClientGetLine(Line), 
	       (rc >= 0))
	{
		if ((rc == 3) && (strcmp(ChrPtr(Line), "000") == 0))
			break;
		if (IsMailAlias)
		{
			int i;

			for (i = 0; i < nForceAliases; i++)
			{
				if ((!MailAliasesFound[i]) && 
				    (strncmp(ForceAliases[i].Key, 
					     ChrPtr(Line),
					     ForceAliases[i].len) == 0)
					)
				    {
					    MailAliasesFound[i] = 1;
					    break;
				    }
			}
		}

		StrBufAppendBufPlain(Line, HKEY("\n"), 0);
		write(TmpFD, ChrPtr(Line), StrLength(Line));
		len += StrLength(Line);
	}
	FreeStrBuf(&Line);
	ftruncate(TmpFD, len);
	close(TmpFD);

	if (IsMailAlias)
	{
		int i, state;
		/*
		 * Sanity check whether all aliases required by the RFCs were set
		 * else bail out.
		 */
		state = 1;
		for (i = 0; i < nForceAliases; i++)
		{
			if (!MailAliasesFound[i]) 
				state = 0;
		}
		if (state == 0)
		{
			cprintf("%d won't do this - you're missing an RFC required alias.\n",
				ERROR + INTERNAL_ERROR);
			unlink(tempfilename);
			return;
		}
	}

	/* Now copy the temp file to its permanent location.
	 * (We copy instead of link because they may be on different filesystems)
	 */
	begin_critical_section(S_NETCONFIGS);
	rename(tempfilename, filename);
	end_critical_section(S_NETCONFIGS);
}

/*
 * cmd_netp() - authenticate to the server as another Citadel node polling
 *	      for network traffic
 */
void cmd_netp(char *cmdbuf)
{
	struct CitContext *CCC = CC;
	HashList *working_ignetcfg;
	char *node;
	StrBuf *NodeStr;
	long nodelen;
	int v;
	long lens[2];
	const char *strs[2];

	const StrBuf *secret = NULL;
	const StrBuf *nexthop = NULL;
	char err_buf[SIZ] = "";

	/* Authenticate */
	node = CCC->curr_user;
	nodelen = extract_token(CCC->curr_user, cmdbuf, 0, '|', sizeof CCC->curr_user);
	NodeStr = NewStrBufPlain(node, nodelen);
	/* load the IGnet Configuration to check node validity */
	working_ignetcfg = load_ignetcfg();
	v = is_valid_node(&nexthop, &secret, NodeStr, working_ignetcfg, NULL);
	if (v != 0) {
		snprintf(err_buf, sizeof err_buf,
			"An unknown Citadel server called \"%s\" attempted to connect from %s [%s].\n",
			node, CCC->cs_host, CCC->cs_addr
		);
		syslog(LOG_WARNING, "%s", err_buf);
		cprintf("%d authentication failed\n", ERROR + PASSWORD_REQUIRED);

		strs[0] = CCC->cs_addr;
		lens[0] = strlen(CCC->cs_addr);
		
		strs[1] = "SRV_UNKNOWN";
		lens[1] = sizeof("SRV_UNKNOWN" - 1);

		CtdlAideFPMessage(
			err_buf,
			"IGNet Networking.",
			2, strs, (long*) &lens);

		DeleteHash(&working_ignetcfg);
		FreeStrBuf(&NodeStr);
		return;
	}

	extract_token(CCC->user.password, cmdbuf, 1, '|', sizeof CCC->user.password);
	if (strcasecmp(CCC->user.password, ChrPtr(secret))) {
		snprintf(err_buf, sizeof err_buf,
			"A Citadel server at %s [%s] failed to authenticate as network node \"%s\".\n",
			CCC->cs_host, CCC->cs_addr, node
		);
		syslog(LOG_WARNING, "%s", err_buf);
		cprintf("%d authentication failed\n", ERROR + PASSWORD_REQUIRED);

		strs[0] = CCC->cs_addr;
		lens[0] = strlen(CCC->cs_addr);
		
		strs[1] = "SRV_PW";
		lens[1] = sizeof("SRV_PW" - 1);

		CtdlAideFPMessage(
			err_buf,
			"IGNet Networking.",
			2, strs, (long*) &lens);

		DeleteHash(&working_ignetcfg);
		FreeStrBuf(&NodeStr);
		return;
	}

	if (network_talking_to(node, nodelen, NTT_CHECK)) {
		syslog(LOG_WARNING, "Duplicate session for network node <%s>", node);
		cprintf("%d Already talking to %s right now\n", ERROR + RESOURCE_BUSY, node);
		DeleteHash(&working_ignetcfg);
		FreeStrBuf(&NodeStr);
		return;
	}
	nodelen = safestrncpy(CCC->net_node, node, sizeof CCC->net_node);
	network_talking_to(CCC->net_node, nodelen, NTT_ADD);
	syslog(LOG_NOTICE, "Network node <%s> logged in from %s [%s]\n",
		CCC->net_node, CCC->cs_host, CCC->cs_addr
	);
	cprintf("%d authenticated as network node '%s'\n", CIT_OK, CCC->net_node);
	DeleteHash(&working_ignetcfg);
	FreeStrBuf(&NodeStr);
}

int netconfig_check_roomaccess(
	char *errmsgbuf, 
	size_t n,
	const char* RemoteIdentifier)
{
	SpoolControl *sc;
	char filename[SIZ];
	int found;

	if (RemoteIdentifier == NULL)
	{
		snprintf(errmsgbuf, n, "Need sender to permit access.");
		return (ERROR + USERNAME_REQUIRED);
	}

	assoc_file_name(filename, sizeof filename, &CC->room, ctdl_netcfg_dir);
	begin_critical_section(S_NETCONFIGS);
	if (!read_spoolcontrol_file(&sc, filename))
	{
		end_critical_section(S_NETCONFIGS);
		snprintf(errmsgbuf, n,
			 "This mailing list only accepts posts from subscribers.");
		return (ERROR + NO_SUCH_USER);
	}
	end_critical_section(S_NETCONFIGS);
	found = is_recipient (sc, RemoteIdentifier);
	free_spoolcontrol_struct(&sc);
	if (found) {
		return (0);
	}
	else {
		snprintf(errmsgbuf, n,
			 "This mailing list only accepts posts from subscribers.");
		return (ERROR + NO_SUCH_USER);
	}
}
/*
 * Module entry point
 */
CTDL_MODULE_INIT(netconfig)
{
	if (!threading)
	{
		CtdlRegisterProtoHook(cmd_gnet, "GNET", "Get network config");
		CtdlRegisterProtoHook(cmd_snet, "SNET", "Set network config");
		CtdlRegisterProtoHook(cmd_netp, "NETP", "Identify as network poller");
	}
	return "netconfig";
}
