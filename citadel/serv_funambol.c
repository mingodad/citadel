/*
 * This module implements a notifier for Funambol push email.
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
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "control.h"
#include "serv_extensions.h"
#include "room_ops.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "tools.h"
#include "internet_addressing.h"
#include "domain.h"
#include "clientsocket.h"



/*
 * Connect to the Funambol server and scan a message.
 */
int notify_funambol(struct CtdlMessage *msg) {
	int sock = (-1);
	char buf[SIZ];
	char SOAPHeader[SIZ];
	char SOAPData[SIZ];
	char port[SIZ];
	/* W means 'Wireless'... */
	if ( msg->cm_fields['W'] == NULL) {
		return(0);
	}
	/* Are we allowed to push? */
	if ( strlen(config.c_funambol_host) == 0) {
		return (0);
	} else {
		lprintf(CTDL_INFO, "Push enabled\n");
	}
	sprintf(port, "%d", config.c_funambol_port);
                lprintf(CTDL_INFO, "Connecting to Funambol at <%s>\n", config.c_funambol_host);
                sock = sock_connect(config.c_funambol_host, port, "tcp");
                if (sock >= 0) lprintf(CTDL_DEBUG, "Connected!\n");

	if (sock < 0) {
		/* If the service isn't running, just pass the mail
		 * through.  Potentially throwing away mails isn't good.
		 */
		return(0);
	}
	
	/* Build a SOAP message, delicately, by hand */
	strcat(SOAPData, "<?xml version=\"1.0\" encoding=\"UTF-8\"?><soapenv:Envelope xmlns:soapenv=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">");
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
	sprintf(buf, "POST %s HTTP/1.0\r\nContent-type: text/xml; charset=utf-8\r\n",
		FUNAMBOL_WS);
	strcat(SOAPHeader,buf);
	strcat(SOAPHeader,"Accept: application/soap+xml, application/dime, multipart/related, text/*\r\n");
	sprintf(buf, "User-Agent: %s/%d\r\nHost: %s:%d\r\nCache-control: no-cache\r\n",
		"Citadel",
		REV_LEVEL,
		config.c_funambol_host,
		config.c_funambol_port
		);
		strcat(SOAPHeader,buf);
	strcat(SOAPHeader,"Pragma: no-cache\r\nSOAPAction: \"\"\r\n");
	sprintf(buf, "Content-Length: %d\r\n",
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
        if (sock_gets(sock, buf) < 0) {
                goto bail;
        }
        lprintf(CTDL_DEBUG, "<%s\n", buf);
	if (strncasecmp(buf, "HTTP/1.1 200 OK", strlen("HTTP/1.1 200 OK"))) {
		goto bail;
	}
	lprintf(CTDL_DEBUG, "Funambol notified\n");

bail:	close(sock);
	return(0);
}



char *serv_funambol_init(void)
{
	CtdlRegisterMessageHook(notify_funambol, EVT_AFTERSAVE);
        return "$Id: serv_funambol.c $";
}
