/////////////////////////////////////////////////////////////////////////////
// Name:        main.cpp
// Purpose:     Main screen type thing
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

#include "includes.hpp"


#include "bitmaps/chat.xpm"
#include "bitmaps/globe.xpm"
#include "bitmaps/mail.xpm"
#include "bitmaps/who.xpm"



// Globals
wxMDIParentFrame *BigMDI;
RoomTree *RoomList;


// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

// Define a new application type, each program should derive a class from wxApp
class Daphne : public wxApp
{
public:
    // override base class virtuals
    // ----------------------------

    // this one is called on application startup and is a good place for the app
    // initialization (doing it here and not in the ctor allows to have an error
    // return: if OnInit() returns false, the application terminates)
    virtual bool OnInit();
};

// Define a new frame type: this is going to be our main frame
class MyFrame : public wxMDIParentFrame
{
public:
    // constructor(s)
    MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size);

	// event handlers (these functions should _not_ be virtual)
	void OnQuit(wxCommandEvent& event);
	void OnAbout(wxCommandEvent& event);
	void OnDoCmd(wxCommandEvent& event);
	void GotoNewRoom(wxTreeEvent& event);
private:
	void OnConnect(wxCommandEvent& event);
	void OnTestWin(wxCommandEvent& event);
	void OnUsersMenu(wxCommandEvent& cmd);
	void OnWindowMenu(wxCommandEvent& cmd);
	wxButton *do_cmd;
	void InitToolBar(wxToolBar* toolBar);

	who *TheWholist;

	// any class wishing to process wxWindows events must use this macro
	DECLARE_EVENT_TABLE()
};

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// IDs for the controls and the menu commands
enum
{
	DO_NOTHING,
	IG_Quit,
	IG_About,
	IG_Text,
	MENU_CONNECT,
	MENU_TESTWIN,
	UMENU_WHO,
	UMENU_SEND_EXPRESS,
	WMENU_CASCADE,
	WMENU_TILE,
	WMENU_ARRANGE,
	WMENU_NEXT,
	WMENU_PREVIOUS,
	BUTTON_DO_CMD,
	ROOMTREE_DOUBLECLICK
};

// ----------------------------------------------------------------------------
// event tables and other macros for wxWindows
// ----------------------------------------------------------------------------

// the event tables connect the wxWindows events with the functions (event
// handlers) which process them. It can be also done at run-time, but for the
// simple menu events like this the static method is much simpler.
BEGIN_EVENT_TABLE(	MyFrame, wxMDIParentFrame)
	EVT_MENU(	IG_Quit,		MyFrame::OnQuit)
	EVT_MENU(	IG_About,		MyFrame::OnAbout)
	EVT_MENU(	MENU_CONNECT,		MyFrame::OnConnect)
	EVT_MENU(	MENU_TESTWIN,		MyFrame::OnTestWin)
	EVT_MENU(	UMENU_WHO,		MyFrame::OnUsersMenu)
	EVT_MENU(	UMENU_SEND_EXPRESS,	MyFrame::OnUsersMenu)
	EVT_MENU(	WMENU_CASCADE,		MyFrame::OnWindowMenu)
	EVT_MENU(	WMENU_TILE,		MyFrame::OnWindowMenu)
	EVT_MENU(	WMENU_ARRANGE,		MyFrame::OnWindowMenu)
	EVT_MENU(	WMENU_NEXT,		MyFrame::OnWindowMenu)
	EVT_MENU(	WMENU_PREVIOUS,		MyFrame::OnWindowMenu)
	EVT_BUTTON(	BUTTON_DO_CMD,		MyFrame::OnDoCmd)
END_EVENT_TABLE()

// Create a new application object: this macro will allow wxWindows to create
// the application object during program execution (it's better than using a
// static object for many reasons) and also declares the accessor function
// wxGetApp() which will return the reference of the right type (i.e. Daphne
// and not wxApp)
IMPLEMENT_APP(Daphne)

// ============================================================================
// implementation
// ============================================================================

wxRadioBox *DevSelect;
wxSlider *ndims;
CitClient *citadel;

// ----------------------------------------------------------------------------
// the application class
// ----------------------------------------------------------------------------

// `Main program' equivalent: the program execution "starts" here
bool Daphne::OnInit()
{

	citadel = new CitClient();

    // Create the main application window
    MyFrame *frame = new MyFrame("Daphne",
                                 wxPoint(10, 10), wxSize(600, 450));
    BigMDI = frame;

    // Show it and tell the application that it's our main window
    // @@@ what does it do exactly, in fact? is it necessary here?
    frame->SetAutoLayout(TRUE);
    frame->Show(TRUE);
    SetTopWindow(frame);

    // success: wxApp::OnRun() will be called which will enter the main message
    // loop and the application will run. If we returned FALSE here, the
    // application would exit immediately.
    return TRUE;
}

// ----------------------------------------------------------------------------
// main frame
// ----------------------------------------------------------------------------

// frame constructor
MyFrame::MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
       : wxMDIParentFrame(
		(wxMDIParentFrame *)NULL,
		-1,
		title, pos, size, wxDEFAULT_FRAME_STYLE
		) {

	TheWholist = NULL;


	// Set up the left-side thingie

	RoomList = new RoomTree(this, citadel);

        wxLayoutConstraints *t2 = new wxLayoutConstraints;
        t2->top.SameAs(this, wxTop, 4);
        t2->left.SameAs(this, wxLeft, 0);
	t2->right.PercentOf(this, wxWidth, 25);
        t2->bottom.SameAs(this, wxBottom, 0);
        RoomList->SetConstraints(t2);

	wxLayoutConstraints *t3 = new wxLayoutConstraints;
	t3->top.SameAs(this, wxTop, 4);
	t3->left.PercentOf(this, wxWidth, 25);
	t3->right.SameAs(this, wxRight, 0);
	t3->bottom.SameAs(this, wxBottom, 0);
	wxMDIClientWindow *children = GetClientWindow();
	children->SetConstraints(t3);


	// Set up the toolbar

	CreateToolBar(wxNO_BORDER|wxTB_FLAT|wxTB_HORIZONTAL);
	InitToolBar(GetToolBar());


	// Set up the pulldown menus

	wxMenu *menuFile = new wxMenu;
	menuFile->Append(MENU_CONNECT, "&Connect");
	menuFile->Append(MENU_TESTWIN, "Add &Test window");
	menuFile->AppendSeparator(); 
	menuFile->Append(IG_Quit, "E&xit");

	wxMenu *menuEdit = new wxMenu;

	wxMenu *menuUsers = new wxMenu;
	menuUsers->Append(UMENU_WHO, "&Who is online?");
	menuUsers->Append(UMENU_SEND_EXPRESS, "&Page another user");

	wxMenu *menuWindow = new wxMenu;
	menuWindow->Append(WMENU_CASCADE, "&Cascade");
	menuWindow->Append(WMENU_TILE, "&Tile");
	menuWindow->Append(WMENU_ARRANGE, "&Arrange icons");
	menuWindow->Append(WMENU_NEXT, "&Next window");
	menuWindow->Append(WMENU_PREVIOUS, "&Previous window");

	wxMenu *menuHelp = new wxMenu;
	menuHelp->Append(IG_About, "&About...");

	// now append the freshly created menu to the menu bar...
	wxMenuBar *menuBar = new wxMenuBar;
	menuBar->Append(menuFile, "&File");
	menuBar->Append(menuEdit, "&Edit");
	menuBar->Append(menuUsers, "&Users");
	menuBar->Append(menuWindow, "&Window");
	menuBar->Append(menuHelp, "&Help");

	// ... and attach this menu bar to the frame
	SetMenuBar(menuBar);

	// create a status bar just for fun (by default with 1 pane only)
	CreateStatusBar(3);
	SetStatusText("Not connected", 0);
}



// The toolbar for this application.
// Right now we aren't defining any toolbars yet, so 

void MyFrame::InitToolBar(wxToolBar* toolBar) {
	int i;
	wxBitmap* bitmaps[4];


	bitmaps[0] = new wxBitmap( globe_xpm );
	bitmaps[1] = new wxBitmap( mail_xpm );
	bitmaps[2] = new wxBitmap( who_xpm );
	bitmaps[3] = new wxBitmap( chat_xpm );

	toolBar->AddTool(MENU_CONNECT,
			*bitmaps[0],
			wxNullBitmap,
			FALSE,
			-1, -1,
			(wxObject *)NULL,
			"The WORLD!!");
			
	toolBar->AddSeparator();

	toolBar->AddTool(DO_NOTHING,
			*bitmaps[1],
			wxNullBitmap,
			FALSE,
			-1, -1,
			(wxObject *)NULL,
			"Open your e-mail inbox");
			
	toolBar->AddSeparator();

	toolBar->AddTool(UMENU_WHO,
			*bitmaps[2],
			wxNullBitmap,
			FALSE,
			-1, -1,
			(wxObject *)NULL,
			"Who is online?");
			
	toolBar->AddTool(DO_NOTHING,
			*bitmaps[3],
			wxNullBitmap,
			FALSE,
			-1, -1,
			(wxObject *)NULL,
			"Real-time chat");
			
	toolBar->Realize();

	for (i = 0; i < 4; i++)
		delete bitmaps[i];
}





// Event handlers.
// We really could handle all menu items in one function, but that would
// get kind of confusing, so we break it down by top-level menus.

void MyFrame::OnQuit(wxCommandEvent& WXUNUSED(event))
{
	// Kill the client connection
	citadel->detach();

	// TRUE is to force the frame to close
	Close(TRUE);
}

// User menu handler
void MyFrame::OnUsersMenu(wxCommandEvent& cmd) {
	int id;
	
	id = cmd.GetId();
	if (id == UMENU_WHO) {
		if (TheWholist == NULL)
			TheWholist = new who(citadel, this);
		else
			TheWholist->Activate();
	}
	else if (id == UMENU_SEND_EXPRESS)
		new SendExpress(citadel, this, "");
}

// Window menu handler
void MyFrame::OnWindowMenu(wxCommandEvent& cmd) {
	int id;
	
	id = cmd.GetId();
	if (id == WMENU_CASCADE)		Cascade();
	else if (id == WMENU_TILE)		Tile();
	else if (id = WMENU_ARRANGE)		ArrangeIcons();
	else if (id == WMENU_NEXT)		ActivateNext();
	else if (id == WMENU_PREVIOUS)		ActivatePrevious();
}



void MyFrame::OnAbout(wxCommandEvent& WXUNUSED(event))
{
	wxString msg;
	msg.Printf(
	  "This is the development version of a project code-named 'Daphne',\n"
	  "a GUI client for the Citadel/UX groupware system.  It is being\n"
	  "developed using the wxWindows toolkit, and therefore should be\n"
	  "easy to port to Linux, Windows, and eventually Macintosh."
	  );
	wxMessageBox(msg, "Daphne", wxOK | wxICON_INFORMATION, this);
}

void MyFrame::OnDoCmd(wxCommandEvent& whichbutton) {
}

void MyFrame::OnConnect(wxCommandEvent& unused) {
	int retval;
	wxString DefaultHost, DefaultPort;

	DefaultHost = "uncnsrd.mt-kisco.ny.us";
	DefaultPort = "citadel";

	if (citadel->IsConnected()) {
		wxMessageBox("You are currently connected to "
			+ citadel->HumanNode
			+ ".  If you wish to connect to a different Citadel "
			+ "server, you must log out first.",
			"Already connected");
	} else {
		retval = citadel->attach(DefaultHost, DefaultPort);
		if (retval == 0) {
    			SetStatusText("Connected to " + citadel->HumanNode, 0);
			new UserLogin(citadel, this);
		} else {
			wxMessageBox("Could not connect to server.", "Error");
		}
	}
}

void MyFrame::OnTestWin(wxCommandEvent& unused) {
	new TestWindow(citadel, this);
}

