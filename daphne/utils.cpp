// utility functions not belonging to any particular class

#include <wx/wx.h>
#include "includes.hpp"

#include "bitmaps/root.xpm"
#include "bitmaps/floor.xpm"
#include "bitmaps/newroom.xpm"
#include "bitmaps/oldroom.xpm"
#include "bitmaps/mailroom.xpm"

wxTreeItemId floorboards[128];
wxImageList *TreeIcons = NULL;

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

int extract_int(wxString inputbuf, int parmnum) {
	wxString buf;

	extract(buf, inputbuf, parmnum);
	return atoi(buf);
}


void InitTreeIcons(void) {
	TreeIcons = new wxImageList(16, 16);
	cout << TreeIcons->Add(wxICON(root));
	cout << TreeIcons->Add(wxICON(floor));
	cout << TreeIcons->Add(wxICON(newroom));
	cout << TreeIcons->Add(wxICON(oldroom));
	cout << TreeIcons->Add(wxICON(mailroom));
}


// Load a tree with a room list
//
void load_roomlist(wxTreeCtrl *tree, CitClient *citsock) {
	wxString sendcmd, recvcmd, buf, floorname, roomname;
	wxStringList transbuf;
	int i, floornum;

	if (TreeIcons == NULL) InitTreeIcons();

	// First, clear it out.
	tree->DeleteAllItems();
	tree->SetImageList(TreeIcons);

	// Set the root with the name of the Citadel server.
	tree->AddRoot(
		citsock->HumanNode,
		0,
		-1,
		NULL);

	sendcmd = "LFLR";
	// Bail out silently if we can't retrieve the floor list
	if (citsock->serv_trans(sendcmd, recvcmd, transbuf) != 1) return;

	// Load the floors one by one onto the tree
        for (i=0; i<transbuf.Number(); ++i) {
                buf.Printf("%s", (wxString *)transbuf.Nth(i)->GetData());
		extract(floorname, buf, 1);
		floornum = extract_int(buf, 0);
		floorboards[floornum] = tree->AppendItem(
			tree->GetRootItem(),
			floorname,
			1,
			-1,
			NULL);
	}

	// Load the rooms into the tree
	// FIX (do this in two passes, one for LKRN and one for LKRO)

	sendcmd = "LKRA";
	if (citsock->serv_trans(sendcmd, recvcmd, transbuf) != 1) return;
        for (i=0; i<transbuf.Number(); ++i) {
                buf.Printf("%s", (wxString *)transbuf.Nth(i)->GetData());
		extract(roomname, buf, 0);
		floornum = extract_int(buf, 2);
		tree->AppendItem(
			floorboards[floornum],
			roomname,
			3,
			-1,
			NULL);
	}
}
