// utility functions not belonging to any particular class

#include "includes.hpp"

// The following two functions convert between the wxStringList class used for
// text transfers to and from the Citadel server, and the wxString class used
// for the contents of a wxTextCtrl.


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


void MultilineToList(wxStringList& outputlist, wxString inputbuf) {
	wxString buf;
	int pos, i;
	
	buf = inputbuf;
	outputlist.Clear();

	while (buf.Length() > 0) {

		// One line with no breaks...
		if ((buf.Length() < 250) && (buf.Find('\n', FALSE)<0)) {
			outputlist.Add(buf);
			goto DONE;
		}

		// First try to locate a line break
		pos = buf.Find('\n', FALSE);
		if ( (pos >=0) && (pos < 250) ) {
			outputlist.Add(buf.Left(pos));
			buf = buf.Mid(pos+1);
		} else {
		// Otherwise, try to find a space
			pos = buf.Left(250).Find(' ', TRUE);
			if ( (pos >= 0) && (pos < 250) ) {
				outputlist.Add(buf.Left(pos));
				buf = buf.Mid(pos+1);
			} else {
				outputlist.Add(buf.Left(250));
				buf = buf.Mid(250);
			}
		}
	}
DONE:	for (i=0; i<outputlist.Number(); ++i) {
		buf.Printf("%s", (wxString *)outputlist.Nth(i)->GetData());
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

