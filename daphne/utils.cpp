// utility functions not belonging to any particular class

#include <wx/wx.h>
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
	int pos;
	
	buf = inputbuf;
	outputlist.Clear();

	while (buf.Length() > 0) {
		// First try to locate a line break
		pos = buf.Find('\n', FALSE);
		if ( (pos >=0) && (pos < 256) ) {
			outputlist.Add(buf.Mid(0, pos-1));
			buf = buf.Mid(pos+1);
		} else {
		// Otherwise, try to find a space
			pos = buf.Mid(0, 256).Find(' ', TRUE);
			if ( (pos >=0) && (pos < 256) ) {
				outputlist.Add(buf.Mid(0, pos-1));
				buf = buf.Mid(pos+1);
			} else {
				pos = 255;
				outputlist.Add(buf.Mid(0, pos-1));
				buf = buf.Mid(pos);
			}
		}
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
			outputbuf = outputbuf.Mid(p+1, 32767);
		}
	}

	p = outputbuf.First('|');
	if (p > 0) {
		outputbuf = outputbuf.Mid(0, p);
	}
}



// Load a tree with a room list
//
void load_roomlist(wxTreeCtrl *tree, CitClient *citsock) {

	// First, clear it out.
	tree->DeleteAllItems();

	// Set the root with the name of the Citadel server.
	tree->AddRoot(
		citsock->HumanNode,
		-1,	// FIX use an "earth" pixmap here
		-1,
		NULL);

}
