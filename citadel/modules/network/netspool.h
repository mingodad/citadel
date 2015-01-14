/*
 * This module handles shared rooms, inter-Citadel mail, and outbound
 * mailing list processing.
 *
 * Copyright (c) 2000-2012 by the citadel.org team
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

typedef struct SpoolControl SpoolControl;

struct SpoolControl {
	OneRoomNetCfg *RNCfg;
	struct ctdlroom room;
	StrBuf *Users[maxRoomNetCfg];
	StrBuf *RoomInfo;
	StrBuf *ListID;
	FILE *digestfp;
	int haveDigest;
	int num_msgs_spooled;
	long lastsent;

	HashList *working_ignetcfg;
	HashList *the_netmap;

	SpoolControl *next;
};


void network_spoolout_room(SpoolControl *sc);

void InspectQueuedRoom(SpoolControl **pSC,
		       RoomProcList *room_to_spool,     
		       HashList *working_ignetcfg,
		       HashList *the_netmap);

int HaveSpoolConfig(OneRoomNetCfg* RNCfg);

void Netmap_AddMe(struct CtdlMessage *msg, const char *defl, long defllen);
void network_do_spoolin(HashList *working_ignetcfg, HashList *the_netmap, int *netmap_changed);
void network_consolidate_spoolout(HashList *working_ignetcfg, HashList *the_netmap);
void free_spoolcontrol_struct(SpoolControl **scc);
void free_spoolcontrol_struct_members(SpoolControl *scc);
int writenfree_spoolcontrol_file(SpoolControl **scc, char *filename);
int read_spoolcontrol_file(SpoolControl **scc, char *filename);

void aggregate_recipients(StrBuf **recps, RoomNetCfg Which, OneRoomNetCfg *OneRNCfg, long nSegments);

void CalcListID(SpoolControl *sc);
