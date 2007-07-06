/* interface.h
 * Gaim Citadel plugin.
 * 
 * © 2006 David Given.
 * This code is licensed under the GPL v3. See the file COPYING in this
 * directory for the full license text.
 *
 * $Id:interface.h 4326 2006-02-18 12:26:22Z hjalfi $
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
