// utility functions not belonging to any particular class

#include "includes.hpp"


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
