// =========================================================================
// declarations
// =========================================================================

#include "includes.hpp"

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// IDs for the controls and the menu commands
enum {
	BUTTON_SAVE,
	BUTTON_CANCEL,
};

// ----------------------------------------------------------------------------
// event tables and other macros for wxWindows
// ----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(ServProps, wxMDIChildFrame)
	EVT_BUTTON(BUTTON_SAVE,		ServProps::OnButtonPressed)
	EVT_BUTTON(BUTTON_CANCEL,	ServProps::OnButtonPressed)
END_EVENT_TABLE()

// ============================================================================
// implementation
// ============================================================================

// frame constructor
ServProps::ServProps(CitClient * sock,
	  wxMDIParentFrame * MyMDI,
	  wxString WhichPanel)
	:wxMDIChildFrame(MyMDI,	//parent
		 -1,		//window id
		 " Server properties ",
		wxDefaultPosition,
		wxDefaultSize,
		wxDEFAULT_FRAME_STYLE | wxTAB_TRAVERSAL,
		"ServProps"
)
{

	wxString buf;

	citsock = sock;
	citMyMDI = MyMDI;

	// set the frame icon
	/* SetIcon(wxICON(mondrian)); */

	wxButton *save_button = new wxButton(
				this,
				BUTTON_SAVE,
				"Save",
				wxPoint(200, 200),
				wxSize(100, 30),
				0L,
				wxDefaultValidator,
				"save_button"
	);

	wxButton *cancel_button = new wxButton(
				this,
				BUTTON_CANCEL,
				"Cancel",
				wxPoint(300, 300),
				wxSize(100, 30),
				0L,
				wxDefaultValidator,
				"cancel_button"
	);

	wxLayoutConstraints *c1 = new wxLayoutConstraints;
	c1->bottom.SameAs(this, wxBottom, 5);
	c1->left.SameAs(this, wxLeft, 10);
	c1->height.AsIs();
	c1->width.AsIs();
	save_button->SetConstraints(c1);

	wxLayoutConstraints *c3 = new wxLayoutConstraints;
	c3->bottom.SameAs(save_button, wxBottom);
	c3->right.SameAs(this, wxRight, 10);
	c3->height.AsIs();
	c3->width.AsIs();
	cancel_button->SetConstraints(c3);

	identity_panel = new wxPanel(this, -1);
	network_panel = new wxPanel(this, -1);
	security_panel = new wxPanel(this, -1);

	wxLayoutConstraints *c4 = new wxLayoutConstraints;
	c4->top.SameAs(this, wxTop);
	c4->left.SameAs(this, wxLeft);
	c4->right.SameAs(this, wxRight);
	c4->bottom.Above(cancel_button, -5);

	wxLayoutConstraints *c41 = new wxLayoutConstraints;
	memcpy(c41, c4, sizeof(wxLayoutConstraints));

	wxLayoutConstraints *c42 = new wxLayoutConstraints;
	memcpy(c42, c4, sizeof(wxLayoutConstraints));

	identity_panel->SetConstraints(c4);
	network_panel->SetConstraints(c41);
	security_panel->SetConstraints(c42);

	wxStaticText *ip_label = new wxStaticText(
				  identity_panel, -1, "Server identity");
	wxStaticText *np_label = new wxStaticText(
				  network_panel, -1, "Network presence");
	wxStaticText *sp_label = new wxStaticText(
			 security_panel, -1, "Global security settings");

	wxLayoutConstraints *c5 = new wxLayoutConstraints;
	c5->top.SameAs(network_panel, wxTop, 3);
	c5->left.SameAs(network_panel, wxLeft, 3);
	c5->right.SameAs(network_panel, wxRight, 3);
	c5->height.AsIs();

	ip_label->SetConstraints(c5);
	//np_label->SetConstraints(c5);
	//sp_label->SetConstraints(c5);

	SetAutoLayout(TRUE);
	Show(TRUE);
	LoadServerConfigStrings();
	Layout();
	wxYield();
	ChangePanel(WhichPanel);
}


void ServProps::ChangePanel(wxString WhichPanel)
{
	identity_panel->Show(!WhichPanel.CmpNoCase("Identity") ? TRUE : FALSE);
	network_panel->Show(!WhichPanel.CmpNoCase("Network") ? TRUE : FALSE);
	security_panel->Show(!WhichPanel.CmpNoCase("Security") ? TRUE : FALSE);
	Layout();
	wxYield();
}


void ServProps::OnButtonPressed(wxCommandEvent & whichbutton)
{
	switch (whichbutton.GetId()) {
	case BUTTON_CANCEL:
		delete this;
		break;
	case BUTTON_SAVE:
		SaveServerConfigStrings();
		delete this;
		break;
	}
}


void ServProps::LoadServerConfigStrings()
{
	int i;
	wxString sendcmd, recvcmd, xferbuf;

	for (i=0; i<20; ++i)
		ServerConfigStrings[i].Empty();

	// Make the window go away if this command fails.
	// Most likely we mistakenly got here without first checking for
	// the proper access level.
	sendcmd = "CONF get";
	if (citsock->serv_trans(sendcmd, recvcmd, xferbuf) != 1)
		delete this;

	wxStringTokenizer *cl = new wxStringTokenizer(xferbuf, "\n", FALSE);
	i = 0;
	while ((i<20) && (cl->HasMoreTokens())) {
		ServerConfigStrings[i++] = cl->NextToken();
	}
}


void ServProps::SaveServerConfigStrings() 
{
	int i;
	wxString sendcmd, recvcmd, xferbuf;

	xferbuf = "";
	for (i=0; i<20; ++i) {
		xferbuf += ServerConfigStrings[i];
		xferbuf += "\n";
	}

	sendcmd = "CONF set";
	if (citsock->serv_trans(sendcmd, recvcmd, xferbuf) != 4) {
		wxMessageDialog errmsg(this,
			recvcmd.Mid(4),
			"Error",
			wxOK | wxCENTRE | wxICON_INFORMATION,
			wxDefaultPosition);
		errmsg.ShowModal();
	}
}

