/*
 * Bring external RSS feeds into rooms.
 *
 * Copyright (c) 2007-2010 by the citadel.org team
 *
 * This program is free software; you can redistribute it and/or modify
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


#define RSS_UNSET       (1<<0)
#define RSS_RSS         (1<<1)
#define RSS_ATOM        (1<<2)
#define RSS_REQUIRE_BUF (1<<3)

typedef struct _rss_item {
	char *roomlist;
	int done_parsing;
	StrBuf *guid;
	StrBuf *title;
	StrBuf *link;
	StrBuf *linkTitle;
	StrBuf *reLink;
	StrBuf *reLinkTitle;
	StrBuf *description;
	time_t pubdate;
	StrBuf *channel_title;
	int item_tag_nesting;
	StrBuf *author_or_creator;
	StrBuf *author_url;
	StrBuf *author_email;
}rss_item;


typedef struct rssnetcfg rssnetcfg;
struct rssnetcfg {
	rssnetcfg *next;
	StrBuf* Url;
	char *rooms;
	time_t last_error_when;
	int ItemType;
	time_t next_poll;
};

typedef void (*rss_handler_func)(StrBuf *CData, 
				 rss_item *ri, 
				 rssnetcfg *Cfg, 
				 const char** Attr);

typedef struct __rss_xml_handler {
	int Flags;
	rss_handler_func Handler;
}rss_xml_handler;

typedef struct _rsscollection {
	AsyncIO IO;
	XML_Parser xp;

	StrBuf *CData;
	StrBuf *Key;

	rss_item *Item;
	rssnetcfg *Cfg;
	
	rss_xml_handler *Current;
} rsscollection;



eNextState ParseRSSReply(AsyncIO *IO);

void rss_save_item(rss_item *ri);
