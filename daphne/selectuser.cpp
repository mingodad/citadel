// ============================================================================
// declarations
// ============================================================================

#include "includes.hpp"

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------



class ReturnedUser : public wxTreeItemData {
public:
	ReturnedUser(wxString);
	wxString emailaddr;
};

ReturnedUser::ReturnedUser(wxString e) {
	emailaddr = e;
}



// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// IDs for the controls and the menu commands
enum
{
	BUTTON_SENDCMD,
	BUTTON_CLOSE,
	THE_TREE
};


// ----------------------------------------------------------------------------
// event tables and other macros for wxWindows
// ----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(	SelectUser, wxPanel)
	EVT_BUTTON(	BUTTON_SENDCMD,		SelectUser::OnButtonPressed)
	EVT_BUTTON(	BUTTON_CLOSE,		SelectUser::OnButtonPressed)
END_EVENT_TABLE()

//BEGIN_EVENT_TABLE(thisclassname, wxTreeCtrl)
//	EVT_TREE_ITEM_ACTIVATED(THE_TREE, SelectUser::OnTreeClick)
//END_EVENT_TABLE()

// ============================================================================
// implementation
// ============================================================================


// ----------------------------------------------------------------------------
// the application class
// ----------------------------------------------------------------------------

// frame constructor
SelectUser::SelectUser(CitClient *sock, wxWindow *the_parent,
		wxString caption,
		wxString rootlabel,
		unsigned int flags,
		wxTextCtrl *PlaceToPutTheSelection)
       : wxPanel(the_parent, -1) {

	citsock = sock;
	target_textctrl = PlaceToPutTheSelection;

	TheTree = new wxTreeCtrl(
		this,
		THE_TREE,
        	wxDefaultPosition, wxDefaultSize,
        	wxTR_HAS_BUTTONS | wxSUNKEN_BORDER
		);

	wxStaticText *caption_ctrl = new wxStaticText(this, -1, caption);
	wxLayoutConstraints *c0 = new wxLayoutConstraints;
	c0->left.SameAs(this, wxLeft, 5);
	c0->top.SameAs(this, wxTop, 5);
	c0->width.AsIs();
	c0->height.AsIs();
	caption_ctrl->SetConstraints(c0);

	wxButton *select_button = new wxButton(
		this,
		BUTTON_SENDCMD,
		" Select "
		);

	wxButton *cancel_button = new wxButton(
		this,
		BUTTON_CLOSE,
		" Cancel "
		);

	wxLayoutConstraints *c5 = new wxLayoutConstraints;
	c5->left.SameAs(this, wxLeft, 5);
	c5->bottom.SameAs(this, wxBottom, 5);
	c5->width.AsIs();
	c5->height.AsIs();
	select_button->SetConstraints(c5);

	wxLayoutConstraints *c6 = new wxLayoutConstraints;
	c6->right.SameAs(this, wxRight, 5);
	c6->bottom.SameAs(this, wxBottom, 5);
	c6->width.AsIs();
	c6->height.AsIs();
	cancel_button->SetConstraints(c6);

	wxLayoutConstraints *c1 = new wxLayoutConstraints;
	c1->top.Below(caption_ctrl, 5);
	c1->bottom.Above(select_button, -5);
	c1->left.SameAs(this, wxLeft, 5);
	c1->right.SameAs(this, wxRight, 5);
	TheTree->SetConstraints(c1);

        SetAutoLayout(TRUE);
        Show(TRUE);
        Layout();
	wxYield();

	// Load up the tree with some STUFF
	TheTree->AddRoot(
		rootlabel,
		-1,		// FIX put an image here
		-1,		// FIX same image
		NULL		// No data here, it's only a heading
		);

	// Add local users to the tree (this is probably always going to be
	// desired, so there's no need for a flag)
	AddLocalUsers(TheTree, citsock);
}



void SelectUser::OnButtonPressed(wxCommandEvent& whichbutton) {

	if (whichbutton.GetId()==BUTTON_CLOSE) {
		delete this;
	}
	else if (whichbutton.GetId()==BUTTON_SENDCMD) {
		ReturnedUser *u = (ReturnedUser *)
			TheTree->GetItemData(TheTree->GetSelection());
		if ( (u != NULL) && (target_textctrl != NULL) ) {
			target_textctrl->SetValue(u->emailaddr);
			delete this;
		}
	}
}


void SelectUser::AddLocalUsers(wxTreeCtrl *tree, CitClient *cit) {
	wxString sendcmd;
	wxString recvcmd;
	wxString xferbuf;
	wxStringTokenizer *ul;
	wxString buf, username;

	sendcmd = "LIST";
	if (citsock->serv_trans(sendcmd, recvcmd, xferbuf) != 1) return;

	wxTreeItemId localusers = tree->AppendItem(
		tree->GetRootItem(),
		cit->HumanNode,
		-1,	// FIX put an image here
		-1,	// FIX and here
		NULL	// No data here either.  It's a heading.
		);

	ul = new wxStringTokenizer(xferbuf, "\n", FALSE);
	while (ul->HasMoreToken()) {
		buf = ul->NextToken();
		extract(username, buf, 0);
		tree->AppendItem(
			localusers,
			username,
			-1,		// FIX img
			-1,		// FIX img
			new ReturnedUser(username)
			);
	}
}


