// ============================================================================
// declarations
// ============================================================================


#include <wx/wx.h>
#include <wx/listctrl.h>
#include "citclient.hpp"
#include "who.hpp"

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// IDs for the controls and the menu commands
enum
{
	BUTTON_DISMISS
};

// ----------------------------------------------------------------------------
// event tables and other macros for wxWindows
// ----------------------------------------------------------------------------

// the event tables connect the wxWindows events with the functions (event
// handlers) which process them. It can be also done at run-time, but for the
// simple menu events like this the static method is much simpler.
BEGIN_EVENT_TABLE(	who, wxMDIChildFrame)
END_EVENT_TABLE()

// ============================================================================
// implementation
// ============================================================================


// ----------------------------------------------------------------------------
// the application class
// ----------------------------------------------------------------------------

// frame constructor
who::who(CitClient *sock, wxMDIParentFrame *MyMDI)
       : wxMDIChildFrame(MyMDI,	//parent
			-1,	//window id
			"Who is online?",
			wxDefaultPosition,
			wxDefaultSize,
			wxDEFAULT_FRAME_STYLE,
			"who"
			) {

	citsock = sock;

	// set the frame icon
	/* SetIcon(wxICON(mondrian)); */

	wholist = new wxListCtrl(
		this,
		-1,	// replace this eventually with something active
		wxDefaultPosition,
		wxDefaultSize,
		wxLC_REPORT,		// multicolumn
		wxDefaultValidator,
		"wholist");


	wxLayoutConstraints *c1 = new wxLayoutConstraints;
	c1->top.SameAs(this, wxTop, 10);		// 10 from the top
	c1->bottom.SameAs(this, wxBottom, 10);
	c1->left.SameAs(this, wxLeft, 10);
	c1->right.SameAs(this, wxRight, 10);
	wholist->SetConstraints(c1);

	wholist->InsertColumn(0, "Session", wxLIST_FORMAT_CENTER, (-1));
	wholist->InsertColumn(1, "User name", wxLIST_FORMAT_CENTER, (-1));
	wholist->InsertColumn(2, "Room", wxLIST_FORMAT_CENTER, (-1));
	wholist->InsertColumn(3, "From host", wxLIST_FORMAT_CENTER, (-1));

	SetAutoLayout(TRUE);
	Show(TRUE);

	LoadWholist();
}


// Load up the control
void who::LoadWholist(void) {
	wxString sendcmd, recvcmd, buf;
	wxStringList rwho;
	int i;

	sendcmd = "RWHO";
	if (citsock->serv_trans(sendcmd, recvcmd, rwho) != 2) return;
	wholist->DeleteAllItems();

        for (i=0; i<rwho.Number(); ++i) {
                buf.Printf("%s", (wxString *)rwho.Nth(i)->GetData());
		wholist->InsertItem((long)i, buf);
	}
}
