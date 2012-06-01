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

#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

#include "context.h"
#include "netconfig.h"
#include "netspool.h"
#include "ctdl_module.h"


/*
 * Load or refresh the Citadel network (IGnet) configuration for this node.
 */
char* load_working_ignetcfg(void) {
	return CtdlGetSysConfig(IGNETCFG);
}


/* 
 * Read the network map from its configuration file into memory.
 */
NetMap *read_network_map(void) {
	char *serialized_map = NULL;
	int i;
	char buf[SIZ];
	NetMap *nmptr, *the_netmap;

	the_netmap = NULL;
	serialized_map = CtdlGetSysConfig(IGNETMAP);
	if (serialized_map == NULL) return NULL;	/* if null, no entries */

	/* Use the string tokenizer to grab one line at a time */
	for (i=0; i<num_tokens(serialized_map, '\n'); ++i) {
		extract_token(buf, serialized_map, i, '\n', sizeof buf);

		nmptr = (NetMap *) malloc(sizeof(NetMap));

		extract_token(nmptr->nodename, buf, 0, '|', sizeof nmptr->nodename);
		nmptr->lastcontact = extract_long(buf, 1);
		extract_token(nmptr->nexthop, buf, 2, '|', sizeof nmptr->nexthop);

		nmptr->next = the_netmap;
		the_netmap = nmptr;
	}

	free(serialized_map);
	return the_netmap;
}


/*
 * Write the network map from memory back to the configuration file.
 */
void write_and_free_network_map(NetMap **the_netmap, int netmap_changed)
{
	char *serialized_map = NULL;
	NetMap *nmptr;

	if (netmap_changed) {
		serialized_map = strdup("");
	
		if (*the_netmap != NULL) {
			for (nmptr = *the_netmap; nmptr != NULL; nmptr = nmptr->next) {
				serialized_map = realloc(serialized_map,
							(strlen(serialized_map)+SIZ) );
				if (!IsEmptyStr(nmptr->nodename)) {
					snprintf(&serialized_map[strlen(serialized_map)],
						SIZ,
						"%s|%ld|%s\n",
						nmptr->nodename,
						(long)nmptr->lastcontact,
						nmptr->nexthop);
				}
			}
		}

		CtdlPutSysConfig(IGNETMAP, serialized_map);
		free(serialized_map);
	}

	/* Now free the list */
	while (*the_netmap != NULL) {
		nmptr = (*the_netmap)->next;
		free(*the_netmap);
		*the_netmap = nmptr;
	}
}



/* 
 * Check the network map and determine whether the supplied node name is
 * valid.  If it is not a neighbor node, supply the name of a neighbor node
 * which is the next hop.  If it *is* a neighbor node, we also fill in the
 * shared secret.
 */
int is_valid_node(char *nexthop, 
		  char *secret, 
		  char *node, 
		  char *working_ignetcfg, 
		  NetMap *the_netmap)
{
	int i;
	char linebuf[SIZ];
	char buf[SIZ];
	int retval;
	NetMap *nmptr;

	if (node == NULL) {
		return(-1);
	}

	/*
	 * First try the neighbor nodes
	 */
	if ((working_ignetcfg == NULL) || (*working_ignetcfg == '\0')) {
		syslog(LOG_ERR, "working_ignetcfg is empty!\n");
		if (nexthop != NULL) {
			strcpy(nexthop, "");
		}
		return(-1);
	}

	retval = (-1);
	if (nexthop != NULL) {
		strcpy(nexthop, "");
	}

	/* Use the string tokenizer to grab one line at a time */
	for (i=0; i<num_tokens(working_ignetcfg, '\n'); ++i) {
		extract_token(linebuf, working_ignetcfg, i, '\n', sizeof linebuf);
		extract_token(buf, linebuf, 0, '|', sizeof buf);
		if (!strcasecmp(buf, node)) {
			if (nexthop != NULL) {
				strcpy(nexthop, "");
			}
			if (secret != NULL) {
				extract_token(secret, linebuf, 1, '|', 256);
			}
			retval = 0;
		}
	}

	if (retval == 0) {
		return(retval);		/* yup, it's a direct neighbor */
	}

	/*	
	 * If we get to this point we have to see if we know the next hop
	 */
	if (the_netmap != NULL) {
		for (nmptr = the_netmap; nmptr != NULL; nmptr = nmptr->next) {
			if (!strcasecmp(nmptr->nodename, node)) {
				if (nexthop != NULL) {
					strcpy(nexthop, nmptr->nexthop);
				}
				return(0);
			}
		}
	}

	/*
	 * If we get to this point, the supplied node name is bogus.
	 */
	syslog(LOG_ERR, "Invalid node name <%s>\n", node);
	return(-1);
}



void cmd_gnet(char *argbuf) {
	char filename[PATH_MAX];
	char buf[SIZ];
	FILE *fp;

	if ( (CC->room.QRflags & QR_MAILBOX) && (CC->user.usernum == atol(CC->room.QRname)) ) {
		/* users can edit the netconfigs for their own mailbox rooms */
	}
	else if (CtdlAccessCheck(ac_room_aide)) return;

	assoc_file_name(filename, sizeof filename, &CC->room, ctdl_netcfg_dir);
	cprintf("%d Network settings for room #%ld <%s>\n",
		LISTING_FOLLOWS,
		CC->room.QRnumber, CC->room.QRname);

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


void cmd_snet(char *argbuf) {
	char tempfilename[PATH_MAX];
	char filename[PATH_MAX];
	int TmpFD;
	StrBuf *Line;
	struct stat StatBuf;
	long len;
	int rc;

	unbuffer_output();

	if ( (CC->room.QRflags & QR_MAILBOX) && (CC->user.usernum == atol(CC->room.QRname)) ) {
		/* users can edit the netconfigs for their own mailbox rooms */
	}
	else if (CtdlAccessCheck(ac_room_aide)) return;

	len = assoc_file_name(filename, sizeof filename, &CC->room, ctdl_netcfg_dir);
	memcpy(tempfilename, filename, len + 1);

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
		StrBufAppendBufPlain(Line, HKEY("\n"), 0);
		write(TmpFD, ChrPtr(Line), StrLength(Line));
		len += StrLength(Line);
	}
	FreeStrBuf(&Line);
	ftruncate(TmpFD, len);
	close(TmpFD);

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
	char *working_ignetcfg;
	char node[256];
	long nodelen;
	char pass[256];
	int v;

	char secret[256] = "";
	char nexthop[256] = "";
	char err_buf[SIZ] = "";

	/* Authenticate */
	nodelen = extract_token(node, cmdbuf, 0, '|', sizeof node);
	extract_token(pass, cmdbuf, 1, '|', sizeof pass);

	/* load the IGnet Configuration to check node validity */
	working_ignetcfg = load_working_ignetcfg();
	v = is_valid_node(nexthop, secret, node, working_ignetcfg, NULL); //// TODO do we need the netmap?

	if (v != 0) {
		snprintf(err_buf, sizeof err_buf,
			"An unknown Citadel server called \"%s\" attempted to connect from %s [%s].\n",
			node, CC->cs_host, CC->cs_addr
		);
		syslog(LOG_WARNING, "%s", err_buf);
		cprintf("%d authentication failed\n", ERROR + PASSWORD_REQUIRED);
		CtdlAideMessage(err_buf, "IGNet Networking.");
		free(working_ignetcfg);
		return;
	}

	if (strcasecmp(pass, secret)) {
		snprintf(err_buf, sizeof err_buf,
			"A Citadel server at %s [%s] failed to authenticate as network node \"%s\".\n",
			CC->cs_host, CC->cs_addr, node
		);
		syslog(LOG_WARNING, "%s", err_buf);
		cprintf("%d authentication failed\n", ERROR + PASSWORD_REQUIRED);
		CtdlAideMessage(err_buf, "IGNet Networking.");
		free(working_ignetcfg);
		return;
	}

	if (network_talking_to(node, nodelen, NTT_CHECK)) {
		syslog(LOG_WARNING, "Duplicate session for network node <%s>", node);
		cprintf("%d Already talking to %s right now\n", ERROR + RESOURCE_BUSY, node);
		free(working_ignetcfg);
		return;
	}

	safestrncpy(CC->net_node, node, sizeof CC->net_node);
	network_talking_to(node, nodelen, NTT_ADD);
	syslog(LOG_NOTICE, "Network node <%s> logged in from %s [%s]\n",
		CC->net_node, CC->cs_host, CC->cs_addr
	);
	cprintf("%d authenticated as network node '%s'\n", CIT_OK, CC->net_node);
	free(working_ignetcfg);
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
