/*
 * File:   extnotify.h
 * Author: Mathew McBride <matt@mcbridematt.dhs.org> / <matt@comalies>
 * Copyright (c) 2008-2009
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *  
 *  
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  
 *  
 *  
 */

#include "../eventclient/serv_curl.h"
#define PAGER_CONFIG_MESSAGE "__ Push email settings __"
#define FUNAMBOL_CONFIG_TEXT "funambol"
#define PAGER_CONFIG_SYSTEM  "textmessage"
#define PAGER_CONFIG_HTTP  "httpmessage"
typedef enum _eNotifyType {
	eNone,
	eFunambol,
	eHttpMessages,
	eTextMessage
}eNotifyType;


#define FUNAMBOL_WS "/funambol/services/admin"

typedef struct _NotifyContext {
	StrBuf **NotifyHostList;
	int nNotifyHosts;
	HashList *NotifyErrors;
	AsyncIO IO;
} NotifyContext;

int notify_http_server(char *remoteurl,
		       const char* template,
		       long tlen,
		       char *user,
		       char *msgid,
		       long MsgNum,
		       NotifyContext *Ctx);

void ExtNotify_PutErrorMessage(NotifyContext *Ctx, StrBuf *ErrMsg);

///void process_notify(long msgnum, void *usrdata);
