/*
 * Bring external RSS feeds into rooms.
 *
 * Copyright (c) 2007-2012 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 * 
 * 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * 
 * 
 * 
 */

#include "internet_addressing.h"

#define RSS_UNSET       (1<<0)
#define RSS_RSS         (1<<1)
#define RSS_ATOM        (1<<2)
#define RSS_REQUIRE_BUF (1<<3)

typedef struct rss_aggregator rss_aggregator;
typedef struct rss_item rss_item;
typedef struct rss_room_counter rss_room_counter;

typedef void (*rss_handler_func)(StrBuf *CData, 
				 rss_item *ri, 
				 rss_aggregator *Cfg, 
				 const char** Attr);


typedef struct __rss_xml_handler {
	int              Flags;
	rss_handler_func Handler;
}rss_xml_handler;

struct rss_item {
	int     done_parsing;
	int     item_tag_nesting;
	time_t  pubdate;
	StrBuf *guid;
	StrBuf *title;
	StrBuf *link;
	StrBuf *linkTitle;
	StrBuf *reLink;
	StrBuf *reLinkTitle;
	StrBuf *description;
	StrBuf *channel_title;
	StrBuf *author_or_creator;
	StrBuf *author_url;
	StrBuf *author_email;
};
void flush_rss_item(rss_item *ri);

struct rss_room_counter {
	int count;
	long QRnumber;
};

typedef struct __networker_save_message {
	struct CtdlMessage Msg;
	StrBuf *MsgGUID;
	StrBuf *Message;

	StrBuf *author_email;
	StrBuf *author_or_creator;
	StrBuf *title;
	StrBuf *description;

	StrBuf *link;
	StrBuf *linkTitle;

	StrBuf *reLink;
	StrBuf *reLinkTitle;
} networker_save_message;

typedef struct RSSCfgLine RSSCfgLine;
struct RSSCfgLine {
	RSSCfgLine *next;
	StrBuf *Url;
	time_t last_known_good;
};

typedef struct __pRSSConfig {
	const RSSCfgLine *pCfg;
	long		 QRnumber;
}pRSSConfig;

struct rss_aggregator {
	AsyncIO    	 IO;
	XML_Parser 	 xp;

	int		 ItemType;
	int		 roomlist_parts;

	time_t		 last_error_when;
	time_t		 next_poll;
	StrBuf		*Url;
	StrBuf          *RedirectUrl;
	StrBuf		*rooms;
	pRSSConfig       Cfg;
	HashList	*OtherQRnumbers;
		   	
	StrBuf		*CData;
	StrBuf		*Key;

	rss_item   	*Item;
	recptypes	 recp;
	HashPos         *Pos;
	HashList        *Messages;
	networker_save_message *ThisMsg;
};



eNextState RSSAggregator_ParseReply(AsyncIO *IO);

eNextState RSS_FetchNetworkUsetableEntry(AsyncIO *IO);
