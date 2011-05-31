/*
 * XML sitemap generator
 *
 * Copyright (c) 2010-2011 by the citadel.org team
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
 * XML sitemap generator -- go through the message list for a BBS room
 */
void sitemap_do_bbs(void) {
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
			wc_printf("?go=");
			urlescputs(ChrPtr(WC->CurRoom.name));
			wc_printf("?start_reading_at=%ld", Msg->msgnum);
			wc_printf("</loc></url>\r\n");
		}
	}
}


/*
 * XML sitemap generator -- go through the message list for a Blog room
 */
void sitemap_do_blog(void) {
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

	for (i=0; i<num_msgs; ++i) {
		Msg = GetMessagePtrAt(i, WCC->summ);
		if (Msg != NULL) {
			struct bltr bltr = blogview_learn_thread_references(Msg->msgnum);
			/* Show only top level posts, not comments */
			if ((bltr.id != 0) && (bltr.refs == 0)) {
				WC->bptlid = bltr.id;
				wc_printf("<url><loc>%s", ChrPtr(site_prefix));
				tmplput_blog_permalink(NULL, NULL);
				wc_printf("</loc></url>\r\n");
			}
		}
	}
}


/*
 * XML sitemap generator -- go through the message list for a wiki room
 */
void sitemap_do_wiki(void) {
	wcsession *WCC = WC;
	int num_msgs = 0;
	int i;
	SharedMessageStatus Stat;
	message_summary *Msg = NULL;
	char buf[256];

	memset(&Stat, 0, sizeof Stat);
	Stat.maxload = INT_MAX;
	Stat.lowest_found = (-1);
	Stat.highest_found = (-1);
	num_msgs = load_msg_ptrs("MSGS ALL", &Stat, NULL);
	if (num_msgs < 1) return;

	for (i=0; i<num_msgs; ++i) {
		Msg = GetMessagePtrAt(i, WCC->summ);
		if (Msg != NULL) {

			serv_printf("MSG0 %ld|3", Msg->msgnum);
			serv_getln(buf, sizeof buf);
			if (buf[0] == '1') while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
				if (	(!strncasecmp(buf, "exti=", 5))
					&& (!bmstrcasestr(buf, "_HISTORY_"))
				) {
					wc_printf("<url><loc>%s/wiki", ChrPtr(site_prefix));
					wc_printf("?go=");
					urlescputs(ChrPtr(WC->CurRoom.name));
					wc_printf("?page=%s", &buf[5]);
					wc_printf("</loc></url>\r\n");
				}
			}
		}
	}
}


/*
 * Entry point for RSS feed generator
 */
void sitemap(void) {
	HashList *roomlist;
	HashPos *it;
	long HKlen;
	const char *HashKey;
	folder *room;

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

	roomlist = GetRoomListHashLKRA(NULL, NULL);
	it = GetNewHashPos(roomlist, 0);

	while (GetNextHashPos(roomlist, it, &HKlen, &HashKey, (void *)&room))
	{
		gotoroom(room->name);

		/* Output the messages in this room only if it's a room type we can make sense of */
		switch(room->defview) {
			case VIEW_BBS:
				sitemap_do_bbs();
				break;
			case VIEW_WIKI:
				sitemap_do_wiki();
				break;
			case VIEW_BLOG:
				sitemap_do_blog();
				break;
			default:
				break;
		}
	}

	DeleteHashPos(&it);
	/* No need to DeleteHash(&roomlist) -- it will be freed when the session closes */

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
