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


/*
 * receive network spool from the remote system
 */
void receive_spool(int *sock, char *remote_nodename) {
	int download_len = 0L;
	int bytes_received = 0L;
	char buf[SIZ];
	char tempfilename[PATH_MAX];
	char permfilename[PATH_MAX];
	int plen;
	FILE *fp;

	snprintf(tempfilename, 
		sizeof tempfilename, 
		"%s/%s.%lx%x",
		ctdl_nettmp_dir,
		remote_nodename, 
		time(NULL),
		rand()
	);

	snprintf(permfilename, 
		sizeof permfilename, 
		"%s/%s.%lx%x",
		ctdl_netin_dir,
		remote_nodename, 
		time(NULL),
		rand()
	);

	if (sock_puts(sock, "NDOP") < 0) return;
	if (sock_getln(sock, buf, sizeof buf) < 0) return;
	syslog(LOG_DEBUG, "<%s\n", buf);
	if (buf[0] != '2') {
		return;
	}

	download_len = extract_long(&buf[4], 0);
	if (download_len <= 0) {
		return;
	}

	bytes_received = 0L;
	fp = fopen(tempfilename, "w");
	if (fp == NULL) {
		syslog(LOG_CRIT, "Cannot create %s: %s\n", tempfilename, strerror(errno));
		return;
	}

	syslog(LOG_DEBUG, "Expecting to transfer %d bytes\n", download_len);
	while (bytes_received < download_len) {
		/*
		 * If shutting down we can exit here and unlink the temp file.
		 * this shouldn't loose us any messages.
		 */
		if (server_shutting_down)
		{
			fclose(fp);
			unlink(tempfilename);
			return;
		}
		snprintf(buf, sizeof buf, "READ %d|%d",
			 bytes_received,
			 ((download_len - bytes_received > IGNET_PACKET_SIZE)
			  ? IGNET_PACKET_SIZE : (download_len - bytes_received))
		);
		
		if (sock_puts(sock, buf) < 0) {
			fclose(fp);
			unlink(tempfilename);
			return;
		}
		if (sock_getln(sock, buf, sizeof buf) < 0) {
			fclose(fp);
			unlink(tempfilename);
			return;
		}
		
		if (buf[0] == '6') {
			plen = extract_int(&buf[4], 0);
			StrBuf *pbuf = NewStrBuf();
			if (socket_read_blob(sock, pbuf, plen, CLIENT_TIMEOUT) != plen) {
				syslog(LOG_INFO, "Short read from peer; aborting.\n");
				fclose(fp);
				unlink(tempfilename);
				FreeStrBuf(&pbuf);
				return;
			}
			fwrite(ChrPtr(pbuf), plen, 1, fp);
			bytes_received += plen;
			FreeStrBuf(&pbuf);
		}
	}

	fclose(fp);

	/* Last chance for shutdown exit */
	if (server_shutting_down)
	{
		unlink(tempfilename);
		return;
	}

	if (sock_puts(sock, "CLOS") < 0) {
		unlink(tempfilename);
		return;
	}

	/*
	 * From here on we must complete or messages will get lost
	 */
	if (sock_getln(sock, buf, sizeof buf) < 0) {
		unlink(tempfilename);
		return;
	}

	syslog(LOG_DEBUG, "%s\n", buf);

	/*
	 * Now move the temp file to its permanent location.
	 */
	if (link(tempfilename, permfilename) != 0) {
		syslog(LOG_ALERT, "Could not link %s to %s: %s\n",
			tempfilename, permfilename, strerror(errno)
		);
	}
	
	unlink(tempfilename);
}



/*
 * transmit network spool to the remote system
 */
void transmit_spool(int *sock, char *remote_nodename)
{
	char buf[SIZ];
	char pbuf[4096];
	long plen;
	long bytes_to_write, thisblock, bytes_written;
	int fd;
	char sfname[128];

	if (sock_puts(sock, "NUOP") < 0) return;
	if (sock_getln(sock, buf, sizeof buf) < 0) return;
	syslog(LOG_DEBUG, "<%s\n", buf);
	if (buf[0] != '2') {
		return;
	}

	snprintf(sfname, sizeof sfname, 
		"%s/%s",
		ctdl_netout_dir,
		remote_nodename
	);
	fd = open(sfname, O_RDONLY);
	if (fd < 0) {
		if (errno != ENOENT) {
			syslog(LOG_CRIT, "cannot open %s: %s\n", sfname, strerror(errno));
		}
		return;
	}
	bytes_written = 0;
	while (plen = (long) read(fd, pbuf, IGNET_PACKET_SIZE), plen > 0L) {
		bytes_to_write = plen;
		while (bytes_to_write > 0L) {
			/* Exit if shutting down */
			if (server_shutting_down)
			{
				close(fd);
				return;
			}
			
			snprintf(buf, sizeof buf, "WRIT %ld", bytes_to_write);
			if (sock_puts(sock, buf) < 0) {
				close(fd);
				return;
			}
			if (sock_getln(sock, buf, sizeof buf) < 0) {
				close(fd);
				return;
			}
			thisblock = atol(&buf[4]);
			if (buf[0] == '7') {
				if (sock_write(sock, pbuf, (int) thisblock) < 0) {
					close(fd);
					return;
				}
				bytes_to_write -= thisblock;
				bytes_written += thisblock;
			} else {
				goto ABORTUPL;
			}
		}
	}

ABORTUPL:
	close(fd);

	/* Last chance for shutdown exit */
	if(server_shutting_down)
		return;
		
	if (sock_puts(sock, "UCLS 1") < 0) return;

	/*
	 * From here on we must complete or messages will get lost
	 */
	if (sock_getln(sock, buf, sizeof buf) < 0) return;
	syslog(LOG_NOTICE, "Sent %ld octets to <%s>\n", bytes_written, remote_nodename);
	syslog(LOG_DEBUG, "<%s\n", buf);
	if (buf[0] == '2') {
		syslog(LOG_DEBUG, "Removing <%s>\n", sfname);
		unlink(sfname);
	}
}


/*
 * Poll one Citadel node (called by network_poll_other_citadel_nodes() below)
 */
void network_poll_node(char *node, char *secret, char *host, char *port) {
	int sock;
	char buf[SIZ];
	char err_buf[SIZ];
	char connected_to[SIZ];
	CitContext *CCC=CC;

	if (network_talking_to(node, NTT_CHECK)) return;
	network_talking_to(node, NTT_ADD);
	syslog(LOG_DEBUG, "network: polling <%s>\n", node);
	syslog(LOG_NOTICE, "Connecting to <%s> at %s:%s\n", node, host, port);

	sock = sock_connect(host, port);
	if (sock < 0) {
		syslog(LOG_ERR, "Could not connect: %s\n", strerror(errno));
		network_talking_to(node, NTT_REMOVE);
		return;
	}
	
	syslog(LOG_DEBUG, "Connected!\n");
	CCC->SBuf.Buf = NewStrBuf();
	CCC->sMigrateBuf = NewStrBuf();
	CCC->SBuf.ReadWritePointer = NULL;

	/* Read the server greeting */
	if (sock_getln(&sock, buf, sizeof buf) < 0) goto bail;
	syslog(LOG_DEBUG, ">%s\n", buf);

	/* Check that the remote is who we think it is and warn the Aide if not */
	extract_token (connected_to, buf, 1, ' ', sizeof connected_to);
	if (strcmp(connected_to, node))
	{
		snprintf(err_buf, sizeof(err_buf),
			"Connected to node \"%s\" but I was expecting to connect to node \"%s\".",
			connected_to, node
		);
		syslog(LOG_ERR, "%s\n", err_buf);
		CtdlAideMessage(err_buf, "Network error");
	}
	else {
		/* We're talking to the correct node.  Now identify ourselves. */
		snprintf(buf, sizeof buf, "NETP %s|%s", config.c_nodename, secret);
		syslog(LOG_DEBUG, "<%s\n", buf);
		if (sock_puts(&sock, buf) <0) goto bail;
		if (sock_getln(&sock, buf, sizeof buf) < 0) goto bail;
		syslog(LOG_DEBUG, ">%s\n", buf);
		if (buf[0] != '2') {
			goto bail;
		}
	
		/* At this point we are authenticated. */
		if (!server_shutting_down)
			receive_spool(&sock, node);
		if (!server_shutting_down)
			transmit_spool(&sock, node);
	}

	sock_puts(&sock, "QUIT");
bail:	
	FreeStrBuf(&CCC->SBuf.Buf);
	FreeStrBuf(&CCC->sMigrateBuf);
	if (sock != -1)
		sock_close(sock);
	network_talking_to(node, NTT_REMOVE);
}



/*
 * Poll other Citadel nodes and transfer inbound/outbound network data.
 * Set "full" to nonzero to force a poll of every node, or to zero to poll
 * only nodes to which we have data to send.
 */
void network_poll_other_citadel_nodes(int full_poll, char *working_ignetcfg)
{
	int i;
	char linebuf[256];
	char node[SIZ];
	char host[256];
	char port[256];
	char secret[256];
	int poll = 0;
	char spoolfile[256];

	if (working_ignetcfg == NULL) {
		syslog(LOG_DEBUG, "network: no neighbor nodes are configured - not polling.\n");
		return;
	}

	/* Use the string tokenizer to grab one line at a time */
	for (i=0; i<num_tokens(working_ignetcfg, '\n'); ++i) {
		if(server_shutting_down)
			return;
		extract_token(linebuf, working_ignetcfg, i, '\n', sizeof linebuf);
		extract_token(node, linebuf, 0, '|', sizeof node);
		extract_token(secret, linebuf, 1, '|', sizeof secret);
		extract_token(host, linebuf, 2, '|', sizeof host);
		extract_token(port, linebuf, 3, '|', sizeof port);
		if ( !IsEmptyStr(node) && !IsEmptyStr(secret) 
		   && !IsEmptyStr(host) && !IsEmptyStr(port)) {
			poll = full_poll;
			if (poll == 0) {
				snprintf(spoolfile, 
					 sizeof spoolfile,
					 "%s/%s",
					 ctdl_netout_dir, 
					 node
				);
				if (access(spoolfile, R_OK) == 0) {
					poll = 1;
				}
			}
			if (poll) {
				network_poll_node(node, secret, host, port);
			}
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
		CtdlRegisterSessionHook(network_do_clientqueue, EVT_TIMER);
	}
	return "network_client";
}
