// utility functions not belonging to any particular class

#include "includes.hpp"


// Extract a field from a string returned by the server
//
void extract(wxString& outputbuf, wxString inputbuf, int parmnum) {
	int a;
	int p;
	
	outputbuf = inputbuf;

	for (a=0; a<parmnum; ++a) {
		p = outputbuf.First('|');
		if (p >= 0) {
			outputbuf = outputbuf.Mid(p+1);
		}
	}

	p = outputbuf.First('|');
	if (p > 0) {
		outputbuf = outputbuf.Left(p);
	}
}

int extract_int(wxString inputbuf, int parmnum) {
	wxString buf;

	extract(buf, inputbuf, parmnum);
	return atoi((const char *)buf);
}

