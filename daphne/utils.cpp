// utility functions not belonging to any particular class

#include "includes.hpp"
#include <time.h>


// Extract a field from a string returned by the server
//
void extract(wxString& outputbuf, wxString inputbuf, int parmnum) {
	int i;
	wxStringTokenizer *tok = new wxStringTokenizer(inputbuf, "|", FALSE);

	for (i=0; tok->HasMoreToken(); ++i) {
		outputbuf = tok->NextToken();
		if (i == parmnum) {
			return;
		}
	}
	outputbuf = "";
}

int extract_int(wxString inputbuf, int parmnum) {
	wxString buf;

	extract(buf, inputbuf, parmnum);
	return atoi((const char *)buf);
}



// Convert traditional Citadel variformat text to HTML
void variformat_to_html(wxString& outputbuf,
			wxString inputbuf,
			bool add_header_and_footer) {

	wxString buf;
	int pos;

	outputbuf.Empty();
	buf = inputbuf;

	if (add_header_and_footer) {
		outputbuf.Append("<HTML><BODY>");
	}

	while (buf.Length() > 0) {
		pos = buf.Find('\n', FALSE);
		if (pos < 0) {
			buf = buf + "\n";
			pos = buf.Find('\n', FALSE);
		}
		if ( (buf.Left(1) == " ") && (outputbuf.Length() > 0) ) {
			outputbuf.Append("<BR>\n");
		}
		outputbuf.Append(buf.Left(pos));
		buf = buf.Mid(pos+1);
	}

	if (add_header_and_footer) {
		outputbuf.Append("</BODY></HTML>\n");
	}
}


wxString generate_html_header(CitMessage *message,
			wxString ThisRoom,
			wxString ThisNode) {

	wxString ret;
	int verbosity = 3;

	switch(verbosity) {

	case 2:
		ret = "&nbsp;&nbsp;&nbsp;<H3>";
		ret += asctime(localtime(&message->timestamp));
		ret += " from " + message->author;
		if (message->room.CmpNoCase(ThisRoom))
			ret += " in " + message->room + "> ";
		if (message->nodename.CmpNoCase(ThisNode))
			ret += " @ " + message->nodename;
		if (message->recipient.Length() > 0)
			ret += " to " + message->recipient;
		ret += "</h3><br>";
		return ret;

	case 3:
		ret = "" ;
		ret += "<TT>Date: </TT>";
		ret += asctime(localtime(&message->timestamp));
		ret += "<BR>";
		ret += "<TT>From: </TT>" + message->author;
		ret += " @ " + message->nodename + "<BR>";
		if (message->recipient.Length() > 0)
			ret += "<TT>To:   </TT>" + message->recipient + "<BR>";
		ret += "<BR>\n";
		return ret;
		
	}
}






// Generic exit stuff
void cleanup(int exitcode) {
	delete ini;		// Write configuration to disk
	exit(exitcode);		// Go away.
}
