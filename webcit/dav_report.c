/*
 * Handles GroupDAV REPORT requests.
 *
 * Copyright (c) 2005-2012 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


/* SAMPLE QUERIES TO WORK WITH *

REPORT /groupdav/calendar/ HTTP/1.1
Content-type: application/xml
Content-length: 349

<C:calendar-query xmlns:C="urn:ietf:params:xml:ns:caldav" xmlns:D="DAV:">
  <D:prop>
    <D:getetag/>
  </D:prop>
  <C:filter>
    <C:comp-filter name="VCALENDAR">
      <C:comp-filter name="VEVENT">
        <C:time-range start="20111129T231445Z" end="20120207T231445Z"/>
      </C:comp-filter>
    </C:comp-filter>
  </C:filter>
</C:calendar-query>


REPORT /groupdav/calendar/ HTTP/1.1
Content-type: application/xml
Content-length: 255

<C:calendar-query xmlns:C="urn:ietf:params:xml:ns:caldav" xmlns:D="DAV:">
  <D:prop>
    <D:getetag/>
  </D:prop>
  <C:filter>
    <C:comp-filter name="VCALENDAR">
      <C:comp-filter name="VEVENT"/>
    </C:comp-filter>
  </C:filter>
</C:calendar-query>

*/

#include "webcit.h"
#include "webserver.h"
#include "dav.h"


/*
 * The pathname is always going to be /groupdav/room_name/msg_num
 */
void dav_report(void) 
{
	char datestring[256];
	time_t now = time(NULL);

	http_datestring(datestring, sizeof datestring, now);
	const char *req = ChrPtr(WC->upload);

	syslog(LOG_DEBUG, "REPORT: \033[31m%s\033[0m", req);

	hprintf("HTTP/1.1 500 Internal Server Error\r\n");
	dav_common_headers();
	hprintf("Date: %s\r\n", datestring);
	hprintf("Content-Type: text/plain\r\n");
	wc_printf("An internal error has occurred at %s:%d.\r\n", __FILE__ , __LINE__ );
	end_burst();
	return;
}



extern int ParseMessageListHeaders_EUID(StrBuf *Line, 
				 const char **pos, 
				 message_summary *Msg, 
				 StrBuf *ConversionBuffer);

extern int DavUIDL_GetParamsGetServerCall(SharedMessageStatus *Stat, 
					  void **ViewSpecific, 
					  long oper, 
					  char *cmd, 
					  long len,
					  char *filter,
					  long flen);

extern int DavUIDL_RenderView_or_Tail(SharedMessageStatus *Stat, 
				void **ViewSpecific, 
				long oper);

extern int DavUIDL_Cleanup(void **ViewSpecific);



void 
InitModule_REPORT
(void)
{
	RegisterReadLoopHandlerset(
		eReadEUIDS,
		DavUIDL_GetParamsGetServerCall,
		NULL,
		NULL,
		ParseMessageListHeaders_EUID,
		NULL,
		DavUIDL_RenderView_or_Tail,
		DavUIDL_Cleanup
	);

}
