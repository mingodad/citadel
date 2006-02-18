/* interface.h
 * Gaim Citadel plugin.
 * 
 * © 2006 David Given.
 * This code is licensed under the GPL v2. See the file COPYING in this
 * directory for the full license text.
 *
 * $Id: auth.c 4258 2006-01-29 13:34:44 +0000 (Sun, 29 Jan 2006) dothebart $
 */

#ifndef INTERFACE_H
#define INTERFACE_H

extern char* interface_readdata(int fd, GaimSslConnection* gsc);
extern int interface_writedata(int fd, GaimSslConnection* gsc, char* data);
extern int interface_connect(GaimAccount* ga, GaimConnection* gc, char* server, int port);
extern void interface_disconnect(int fd, GaimSslConnection* gsc);
extern void interface_tlson(GaimConnection* gc, GaimAccount* ga, int fd);
extern int interface_timeron(GaimConnection* gc, time_t timeout);
extern void interface_timeroff(GaimConnection* gc, int timerhandle);
	
extern const char* gaim_group_get_name(GaimGroup* group);

#endif
