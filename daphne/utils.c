
#include <wx/wx.h>
#include "utils.h"

void ListToMultiline(wxString& outputbuf, wxStringList inputlist) {
	int i;
	wxString buf;

	outputbuf.Empty();
	for (i=0; i<inputlist.Number(); ++i) {
		buf.Printf("%s", (wxString *)inputlist.Nth(i)->GetData());
		outputbuf.Append(buf);
		outputbuf.Append("\n");
	}
}

// Extract a field from a string returned by the server
//
void extract(wxString& outputbuf, wxString inputbuf, int parmnum) {
	int a;
	int p;
	
	outputbuf = inputbuf;

	for (a=0; a<parmnum; ++a) {
		p = outputbuf.First('|');
		if (p >= 0) {
			outputbuf = outputbuf.Mid(p+1, STRING_MAXLEN);
		}
	}

	p = outputbuf.First('|');
	if (p > 0) {
		outputbuf = outputbuf.Mid(0, p);
	}
}
