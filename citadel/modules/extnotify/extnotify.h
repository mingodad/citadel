/* 
 * File:   extnotify.h
 * Author: Mathew McBride <matt@mcbridematt.dhs.org> / <matt@comalies>
 * Copyright (c) 2008-2009
 *
 *  This program is free software; you can redistribute it and/or modify
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
 */

#include "../curl/serv_curl.h"

#define FUNAMBOL_CONFIG_TEXT "funambol"
#define PAGER_CONFIG_MESSAGE "__ Push email settings __"
#define PAGER_CONFIG_SYSTEM  "textmessage"    
#define PAGER_CONFIG_HTTP  "httpmessage"    

#define FUNAMBOL_WS "/funambol/services/admin"

typedef struct _NotifyContext {
	StrBuf **NotifyHostList;
	HashList *NotifyErrors;
	evcurl_request_data HTTPData;
} NotifyContext;

int notify_http_server(char *remoteurl, 
		       const char* template, 
		       long tlen, 
		       char *user,
		       char *msgid, 
		       long MsgNum, 
		       NotifyContext *Ctx);

void ExtNotify_PutErrorMessage(NotifyContext *Ctx, StrBuf *ErrMsg);

void extNotify_getPrefs(long configMsgNum, char *configMsg);
long extNotify_getConfigMessage(char *username);
void process_notify(long msgnum, void *usrdata);



