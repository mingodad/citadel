/*
 * XML sitemap generator
 *
 * Copyright (c) 2010 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "webcit.h"
#include "webserver.h"


/*
 * XML sitemap generator -- go through the message list
 */
void sitemap_do_messages(void) {
	wcsession *WCC = WC;
	int num_msgs = 0;
	int i;
	SharedMessageStatus Stat;
	message_summary *Msg = NULL;

	memset(&Stat, 0, sizeof Stat);
	Stat.maxload = INT_MAX;
	Stat.lowest_found = (-1);
	Stat.highest_found = (-1);
	num_msgs = load_msg_ptrs("MSGS ALL", &Stat, NULL);
	if (num_msgs < 1) return;

	for (i=0; i<num_msgs; i+=20) {
		Msg = GetMessagePtrAt(i, WCC->summ);
		if (Msg != NULL) {
			wc_printf("<url><loc>%s/readfwd", ChrPtr(site_prefix));
			wc_printf("?gotofirst=");
			urlescputs(ChrPtr(WC->CurRoom.name));
			wc_printf("?start_reading_at=%ld", Msg->msgnum);
			wc_printf("</loc></url>\r\n");
		}
	}
}


/*
 * Entry point for RSS feed generator
 */
void sitemap(void) {

	output_headers(0, 0, 0, 0, 1, 0);
	hprintf("Content-type: text/xml\r\n");
	hprintf(
		"Server: %s / %s\r\n"
		"Connection: close\r\n"
	,
		PACKAGE_STRING, ChrPtr(WC->serv_info->serv_software)
	);
	begin_burst();

	wc_printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n");
	wc_printf("<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\r\n");

	HashList *roomlist = GetRoomListHashLKRA(NULL, NULL);
	HashPos *it = GetNewHashPos(roomlist, 0);
	long HKlen;
	const char *HashKey;
	folder *room;

	while (GetNextHashPos(roomlist, it, &HKlen, &HashKey, (void *)&room))
	{
		/* Output the messages in this room only if it's a message board */
		if (room->defview == VIEW_BBS)
		{
			gotoroom(room->name);
			sitemap_do_messages();
		}
	}

	DeleteHashPos(&it);
	/* DeleteHash(&roomlist); This will be freed when the session closes */

	wc_printf("</urlset>\r\n");
	wDumpContent(0);
}


void 
InitModule_SITEMAP
(void)
{
	WebcitAddUrlHandler(HKEY("sitemap"), "", 0, sitemap, ANONYMOUS|COOKIEUNNEEDED);
	WebcitAddUrlHandler(HKEY("sitemap.xml"), "", 0, sitemap, ANONYMOUS|COOKIEUNNEEDED);
}
