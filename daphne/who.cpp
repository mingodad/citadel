// ============================================================================
// declarations
// ============================================================================

#include "includes.hpp"

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// IDs for the controls and the menu commands
enum
{
	BUTTON_REFRESH,
	BUTTON_CLOSE,
};

// ----------------------------------------------------------------------------
// event tables and other macros for wxWindows
// ----------------------------------------------------------------------------

// the event tables connect the wxWindows events with the functions (event
// handlers) which process them. It can be also done at run-time, but for the
// simple menu events like this the static method is much simpler.
BEGIN_EVENT_TABLE(	who, wxMDIChildFrame)
	EVT_BUTTON(	BUTTON_REFRESH,		who::OnButtonPressed)
	EVT_BUTTON(	BUTTON_CLOSE,		who::OnButtonPressed)
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
			wxDEFAULT_FRAME_STYLE | wxTAB_TRAVERSAL,
			"who"
			) {


	citsock = sock;


	/*who_refresh *ref = new who_refresh(this);*/

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


	wxButton *refresh_button = new wxButton(
		this,
		BUTTON_REFRESH,
		"Refresh",
		wxPoint(100,100),
		wxSize(100,30),
		0L,
		wxDefaultValidator,
		"refresh_button"
		);

	wxButton *close_button = new wxButton(
		this,
		BUTTON_CLOSE,
		"Close",
		wxPoint(200,200),
		wxSize(100,30),
		0L,
		wxDefaultValidator,
		"close_button"
		);

	
	wxLayoutConstraints *c1 = new wxLayoutConstraints;
	c1->top.SameAs(this, wxTop, 10);		// 10 from the top
	c1->bottom.SameAs(this, wxBottom, 10);
	c1->left.SameAs(this, wxLeft, 10); 
	c1->right.SameAs(this, wxRight, 10);
	wholist->SetConstraints(c1);

	wholist->InsertColumn(0, "Session", wxLIST_FORMAT_CENTER, 60);
	wholist->InsertColumn(1, "User name", wxLIST_FORMAT_CENTER, 150);
	wholist->InsertColumn(2, "Room", wxLIST_FORMAT_CENTER, 150);
	wholist->InsertColumn(3, "From host", wxLIST_FORMAT_CENTER, 150);

	wxLayoutConstraints *c2 = new wxLayoutConstraints;
	c2->bottom.SameAs(this, wxBottom, 10);
	c2->centreX.SameAs(this, wxCentreX);
	c2->height.AsIs(); c2->width.AsIs();

	wxLayoutConstraints *b1 = new wxLayoutConstraints;
	b1->bottom.SameAs(this, wxBottom, 2);
	b1->height.AsIs(); 
	b1->width.AsIs();
	b1->right.SameAs(this, wxRight, 10);
	close_button->SetConstraints(b1);

	wxLayoutConstraints *b2 = new wxLayoutConstraints;
	b2->bottom.SameAs(close_button, wxBottom);
	b2->left.SameAs(this, wxLeft, 10);
	b2->height.AsIs(); 
	b2->width.AsIs();
	refresh_button->SetConstraints(b2);

	SetAutoLayout(TRUE);
	Show(TRUE);
	LoadWholist();
        Layout();
}


void who::OnButtonPressed(wxCommandEvent& whichbutton) {


	if (whichbutton.GetId() == BUTTON_CLOSE) {
	wholist->DeleteAllItems();	
	delete this;
}
	if (whichbutton.GetId() == BUTTON_REFRESH) {
	LoadWholist(); 
}
}
// Load up the control
void who::LoadWholist(void) {


	wxString sendcmd, recvcmd, buf;
	wxString rwho;
	int i = 0;
	wxString sess, user, room, host;
	wxStringTokenizer *wl;

		if (wholist==NULL) {
		return; }
                if (citadel->IsConnected()==FALSE) { 
                wxMessageBox("You are not connected to a BBS."); 
		return; 
        } else 


	sendcmd = "RWHO";
	if (citsock->serv_trans(sendcmd, recvcmd, rwho) != 1) return;
	wholist->DeleteAllItems();

	wl = new wxStringTokenizer(rwho, "\n", FALSE);
	while (wl->HasMoreTokens()) {
		buf = wl->NextToken();
		extract(sess, buf, 0);
		extract(user, buf, 1);
		extract(room, buf, 2);
		extract(host, buf, 3);
		wholist->InsertItem(i, sess);
		wholist->SetItem(i, 1, user);
		wholist->SetItem(i, 2, room);
		wholist->SetItem(i, 3, host);
		++i;
	}
   }



/*
who_refresh::who_refresh(who *parent_who)
	: wxTimer() {

	        if (citadel->IsConnected()==FALSE) {
		Stop();
		delete this;
        } else

	which_who = parent_who;		// Know which instance to refresh

	Start(30000, FALSE);		// Call every 30 seconds
} */


/*void who_refresh::Notify(void)  {

	which_who->LoadWholist();
} */

