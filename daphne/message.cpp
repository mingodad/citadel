#include "includes.hpp"
#include <time.h>

CitMessage::CitMessage(CitClient *sock, wxString getmsg_cmd, wxString inRoom) {

	wxString sendcmd, recvcmd, buf, key;
	wxStringList xferbuf;
	int i;
	bool in_text = FALSE;		// true if reading the message body

	room.Empty();
	author.Empty();
	msgtext.Empty();
	timestamp = time(NULL);		// nb. this is Unix-specific
	format_type = 0;
	nodename = sock->NodeName;

	// Fetch the message from the server
	if (sock->serv_trans(getmsg_cmd, recvcmd, xferbuf, inRoom) != 1) {
		msgtext = "<EM>Error: " + recvcmd.Mid(4) + "</EM>" ;
                return;
	}

        for (i=0; i<xferbuf.Number(); ++i) {
                buf.Printf("%s", (wxString *)xferbuf.Nth(i)->GetData());

		// Break out important information if in the header
		if (!in_text) {
			key = buf.Left(4);
			if (!key.CmpNoCase("text"))
				in_text = TRUE;
			else if (!key.CmpNoCase("from"))
				author = buf.Mid(5);
			else if (!key.CmpNoCase("time"))
				timestamp = atol(buf.Mid(5));
			else if (!key.CmpNoCase("room"))
				room = buf.Mid(5);
			else if (!key.CmpNoCase("type")) {
				format_type = atoi(buf.Mid(5));
				if (format_type == 1) {
					msgtext.Append("<PRE>\n");
				}
			}
			else if (!key.CmpNoCase("node"))
				nodename = buf.Mid(5);

		// Otherwise, process message text
		} else {
			if (format_type == 1) {
				msgtext.Append(buf);
				msgtext.Append("\n");
			} else {
				if ( (buf.Left(1) == " ")
				   && (msgtext.Len() > 0) )
					msgtext.Append("<BR>");
				msgtext.Append(buf);
			}
		}
	}
	if (format_type == 1) 
		msgtext.Append("</PRE>\n");
}
