/*
 * This module implements a notifier for Funambol push email.
 * Based on bits of serv_spam, serv_smtp
 */

#define FUNAMBOL_WS       "/funambol/services/admin"
#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include <sys/socket.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "control.h"
#include "room_ops.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "tools.h"
#include "internet_addressing.h"
#include "domain.h"
#include "clientsocket.h"
#include "serv_funambol.h"



#include "ctdl_module.h"


/*
 * Create the notify message queue
 */
void create_notify_queue(void) {
	struct ctdlroom qrbuf;

	create_room(FNBL_QUEUE_ROOM, 3, "", 0, 1, 0, VIEW_MAILBOX);

	/*
	 * Make sure it's set to be a "system room" so it doesn't show up
	 * in the <K>nown rooms list for Aides.
	 */
	if (lgetroom(&qrbuf, FNBL_QUEUE_ROOM) == 0) {
		qrbuf.QRflags2 |= QR2_SYSTEM;
		lputroom(&qrbuf);
	}
}
void do_notify_queue(void) {
	static int doing_queue = 0;

	/*
	 * This is a simple concurrency check to make sure only one queue run
	 * is done at a time.  We could do this with a mutex, but since we
	 * don't really require extremely fine granularity here, we'll do it
	 * with a static variable instead.
	 */
	if (doing_queue) return;
	doing_queue = 1;

	/* 
	 * Go ahead and run the queue
	 */
	lprintf(CTDL_INFO, "serv_funambol: processing notify queue\n");

	if (getroom(&CC->room, FNBL_QUEUE_ROOM) != 0) {
		lprintf(CTDL_ERR, "Cannot find room <%s>\n", FNBL_QUEUE_ROOM);
		return;
	}
	CtdlForEachMessage(MSGS_ALL, 0L, NULL,
		SPOOLMIME, NULL, notify_funambol, NULL);

	lprintf(CTDL_INFO, "serv_funambol: queue run completed\n");
	doing_queue = 0;
}

/*
 * Connect to the Funambol server and scan a message.
 */
void notify_funambol(long msgnum, void *userdata) {
	struct CtdlMessage *msg;
	int sock = (-1);
	char buf[SIZ];
	char SOAPHeader[SIZ];
	char SOAPData[SIZ];
	char port[SIZ];
	/* W means 'Wireless'... */
	msg = CtdlFetchMessage(msgnum, 1);
	if ( msg->cm_fields['W'] == NULL) {
		goto nuke;
	}
	/* Are we allowed to push? */
	if (IsEmptyStr(config.c_funambol_host)) {
		goto nuke;
	} else {
		lprintf(CTDL_INFO, "Push enabled\n");
	}
	
	sprintf(port, "%d", config.c_funambol_port);
                lprintf(CTDL_INFO, "Connecting to Funambol at <%s>\n", config.c_funambol_host);
                sock = sock_connect(config.c_funambol_host, port, "tcp");
                if (sock >= 0) lprintf(CTDL_DEBUG, "Connected!\n");

	if (sock < 0) {
		/* If the service isn't running, pass for now */
		return;
	}
	
	/* Build a SOAP message, delicately, by hand */
	sprintf(SOAPData, "<?xml version=\"1.0\" encoding=\"UTF-8\"?><soapenv:Envelope xmlns:soapenv=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">");
	strcat(SOAPData, "<soapenv:Body><sendNotificationMessages soapenv:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">");
	strcat(SOAPData, "<arg0 xsi:type=\"soapenc:string\" xmlns:soapenc=\"http://schemas.xmlsoap.org/soap/encoding/\">");
	strcat(SOAPData, msg->cm_fields['W']);
	strcat(SOAPData, "</arg0>");
	strcat(SOAPData, "<arg1 xsi:type=\"soapenc:string\" xmlns:soapenc=\"http://schemas.xmlsoap.org/soap/encoding/\">&lt;?xml version=&quot;1.0&quot; encoding=&quot;UTF-8&quot;?&gt;\r\n");
	strcat(SOAPData, "&lt;java version=&quot;1.5.0_10&quot; class=&quot;java.beans.XMLDecoder&quot;&gt; \r\n");
	strcat(SOAPData, " &lt;array class=&quot;com.funambol.framework.core.Alert&quot; length=&quot;1&quot;&gt;\r\n");
	strcat(SOAPData, "  &lt;void index=&quot;0&quot;&gt;\r\n");
	strcat(SOAPData, "   &lt;object class=&quot;com.funambol.framework.core.Alert&quot;&gt;\r\n");
	strcat(SOAPData, "    &lt;void property=&quot;cmdID&quot;>\r\n");
	strcat(SOAPData, "     &lt;object class=&quot;com.funambol.framework.core.CmdID&quot;/&gt;\r\n");
	strcat(SOAPData, "    &lt;/void&gt;");
	strcat(SOAPData, "    &lt;void property=&quot;data&quot;&gt;\r\n");
	strcat(SOAPData, "     &lt;int&gt;210&lt;/int&gt;\r\n");
	strcat(SOAPData, "    &lt;/void&gt;\r\n");
	strcat(SOAPData, "    &lt;void property=&quot;items&quot;&gt;\r\n");
        strcat(SOAPData, "     &lt;void method=&quot;add&quot;&gt;\r\n"); 
	strcat(SOAPData, "      &lt;object class=&quot;com.funambol.framework.core.Item&quot;&gt;\r\n"); 
	strcat(SOAPData, "       &lt;void property=&quot;meta&quot;&gt;\r\n"); 
	strcat(SOAPData, "        &lt;object class=&quot;com.funambol.framework.core.Meta&quot;&gt;\r\n"); 
	strcat(SOAPData, "         &lt;void property=&quot;metInf&quot;&gt;\r\n");
	strcat(SOAPData, "          &lt;void property=&quot;type&quot;&gt;\r\n");
	strcat(SOAPData, "           &lt;string&gt;application/vnd.omads-email+xml&lt;/string&gt;\r\n");
	strcat(SOAPData, "          &lt;/void&gt;\r\n"); 
	strcat(SOAPData, "         &lt;/void&gt;\r\n"); 
	strcat(SOAPData, "        &lt;/object&gt;\r\n"); 
	strcat(SOAPData, "       &lt;/void&gt;\r\n"); 
	strcat(SOAPData, "       &lt;void property=&quot;target&quot;&gt;\r\n"); 
	strcat(SOAPData, "        &lt;object class=&quot;com.funambol.framework.core.Target&quot;&gt;\r\n");
	strcat(SOAPData, "         &lt;void property=&quot;locURI&quot;&gt;\r\n");
	strcat(SOAPData, "          &lt;string&gt;");
	strcat(SOAPData, config.c_funambol_source);
	strcat(SOAPData, "&lt;/string&gt;\r\n");
	strcat(SOAPData, "         &lt;/void&gt;\r\n");
	strcat(SOAPData, "        &lt;/object&gt;\r\n");
	strcat(SOAPData, "       &lt;/void&gt;\r\n");
	strcat(SOAPData, "      &lt;/object&gt;\r\n");
	strcat(SOAPData, "     &lt;/void&gt;\r\n");
	strcat(SOAPData, "    &lt;/void&gt;\r\n");
	strcat(SOAPData, "   &lt;/object&gt;\r\n");
	strcat(SOAPData, "  &lt;/void&gt;\r\n");
	strcat(SOAPData, " &lt;/array&gt;\r\n");
	strcat(SOAPData, "&lt;/java&gt;");
	strcat(SOAPData,"</arg1><arg2 href=\"#id0\"/></sendNotificationMessages><multiRef id=\"id0\" soapenc:root=\"0\"\r\n");
	strcat(SOAPData,"soapenv:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\" xsi:type=\"soapenc:int\" xmlns:soapenc=\"http://schemas.xmlsoap.org/soap/encoding/\">1</multiRef></soapenv:Body></soapenv:Envelope>");
	
	/* Command */
	lprintf(CTDL_DEBUG, "Transmitting command\n");
	sprintf(SOAPHeader, "POST %s HTTP/1.0\r\nContent-type: text/xml; charset=utf-8\r\n",
		FUNAMBOL_WS);
	strcat(SOAPHeader,"Accept: application/soap+xml, application/dime, multipart/related, text/*\r\n");
	sprintf(buf, "User-Agent: %s/%d\r\nHost: %s:%d\r\nCache-control: no-cache\r\n",
		"Citadel",
		REV_LEVEL,
		config.c_funambol_host,
		config.c_funambol_port
		);
		strcat(SOAPHeader,buf);
	strcat(SOAPHeader,"Pragma: no-cache\r\nSOAPAction: \"\"\r\n");
	sprintf(buf, "Content-Length: " SIZE_T_FMT "\r\n",
		strlen(SOAPData));
	strcat(SOAPHeader, buf);
	sprintf(buf, "Authorization: Basic %s\r\n\r\n",
		config.c_funambol_auth);
	strcat(SOAPHeader, buf);
	
	sock_write(sock, SOAPHeader, strlen(SOAPHeader));
	sock_write(sock, SOAPData, strlen(SOAPData));
	sock_shutdown(sock, SHUT_WR);
	
	/* Response */
	lprintf(CTDL_DEBUG, "Awaiting response\n");
        if (sock_getln(sock, buf, sizeof buf) < 0) {
                goto bail;
        }
        lprintf(CTDL_DEBUG, "<%s\n", buf);
	if (strncasecmp(buf, "HTTP/1.1 200 OK", strlen("HTTP/1.1 200 OK"))) {
		
		goto bail;
	}
	lprintf(CTDL_DEBUG, "Funambol notified\n");
	/* We should allow retries here but for now purge after one go */
	bail:		
	close(sock);
	nuke:
	CtdlFreeMessage(msg);
	long todelete[1];
	todelete[0] = msgnum;
	CtdlDeleteMessages(FNBL_QUEUE_ROOM, todelete, 1, "");
}



CTDL_MODULE_INIT(funambol)
{
	create_notify_queue();
	CtdlRegisterSessionHook(do_notify_queue, EVT_TIMER);

	/* return our Subversion id for the Log */
        return "$Id$";
}
