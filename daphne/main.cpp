/////////////////////////////////////////////////////////////////////////////
// Name:        main.cpp
// Purpose:     Main screen type thing
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

#include "includes.hpp"

#ifdef __WXMOTIF__
#include "bitmaps/globe.xpm"
#include "bitmaps/mail.xpm"
#include "bitmaps/who.xpm"
#include "bitmaps/chat.xpm"
#endif


// Globals
wxMDIParentFrame *BigMDI;
RoomTree *RoomList;
wxConfig *ini;


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
	void OnGotoMail(wxCommandEvent& event);
	void OnTestWin(wxCommandEvent& event);
	void OnEditMenu(wxCommandEvent& cmd);
	void OnUsersMenu(wxCommandEvent& cmd);
	void OnRoomsMenu(wxCommandEvent& cmd);
        void MyFrame::OnSize(wxSizeEvent& event);
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
	EMENU_PREFS,
	UMENU_WHO,
	UMENU_SEND_EXPRESS,
	RMENU_GOTO,
	BUTTON_DO_CMD,
	ROOMTREE_DOUBLECLICK,
	GOTO_MAIL
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
	EVT_MENU(	EMENU_PREFS,		MyFrame::OnEditMenu)
	EVT_MENU(	GOTO_MAIL,		MyFrame::OnGotoMail)
	EVT_MENU(	MENU_TESTWIN,		MyFrame::OnTestWin)
	EVT_MENU(	UMENU_WHO,		MyFrame::OnUsersMenu)
	EVT_MENU(	UMENU_SEND_EXPRESS,	MyFrame::OnUsersMenu)
	EVT_MENU(	RMENU_GOTO,		MyFrame::OnRoomsMenu)
	EVT_BUTTON(	BUTTON_DO_CMD,		MyFrame::OnDoCmd)
        EVT_SIZE(                               MyFrame::OnSize)
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
	int w, h;
	wxString sizestr;

	// Read the configuration file
	ini = new wxConfig("daphne");
	ini->SetRecordDefaults(TRUE);
	ini->Read("/Window Sizes/Main", &sizestr, "600 450");
	sscanf((const char *)sizestr, "%d %d", &w, &h);

	// Connect to the server
	citadel = new CitClient();

	// Create the main application window
	MyFrame *frame = new MyFrame("Daphne",
                                 wxPoint(10, 10), wxSize(w, h));
	BigMDI = frame;

	// Show it and tell the application that it's our main window
	// @@@ what does it do exactly, in fact? is it necessary here?
	SetTopWindow(frame);

	// success: wxApp::OnRun() will be called which will enter the main
	// message loop and the application will run. If we returned FALSE
	// here, the application would exit immediately.
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
		title, pos, size, wxDEFAULT_FRAME_STYLE | wxTAB_TRAVERSAL
		) {
	wxString buf;

	TheWholist = NULL;


	// Set up the left-side thingie

	RoomList = new RoomTree(this, citadel);

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
	menuEdit->Append(EMENU_PREFS, "&Preferences...");

	wxMenu *menuUsers = new wxMenu;
	menuUsers->Append(UMENU_WHO, "&Who is online?");
	menuUsers->Append(UMENU_SEND_EXPRESS, "&Page another user");

	wxMenu *menuRooms = new wxMenu;
	menuRooms->Append(RMENU_GOTO, "&Goto next room with unread messages");

	wxMenu *menuHelp = new wxMenu;
	menuHelp->Append(IG_About, "&About...");

	// now append the freshly created menu to the menu bar...
	wxMenuBar *menuBar = new wxMenuBar;
	menuBar->Append(menuFile, "&File");
	menuBar->Append(menuEdit, "&Edit");
	menuBar->Append(menuUsers, "&Users");
	menuBar->Append(menuRooms, "&Rooms");
	menuBar->Append(menuHelp, "&Help");

	// ... and attach this menu bar to the frame
	SetMenuBar(menuBar);

	// Create the status bar
	CreateStatusBar(3);
	SetStatusText("Not connected", 0);

        Show(TRUE);

	wxYield();
	ini->Read("/Citadel Server/ConnectOnStartup", &buf, "no");
	if (!buf.CmpNoCase("yes")) {
		wxCommandEvent cjunk;
		OnConnect(cjunk);
	}
}



// The toolbar for this application.
void MyFrame::InitToolBar(wxToolBar* toolBar) {
	int i;
	wxBitmap* bitmaps[4];

// wxGTK seems to do the right thing by itself, while wxMSW wants to be
// told how big the toolbar icons are going to be, otherwise it defaults to
// 16x16.  Strangely, wxToolBar::SetToolBitmapSize() doesn't seem to be
// available at all in wxGTK, hence the ifdef...
#ifndef __WXGTK__
	toolBar->SetToolBitmapSize(wxSize(32, 32));
#endif

        // Set up the toolbar icons (BMP is available on both GTK and MSW) 
#ifndef __WXMOTIF__
	bitmaps[0] = new wxBitmap("bitmaps/globe.bmp",	wxBITMAP_TYPE_BMP);
	bitmaps[1] = new wxBitmap("bitmaps/mail.bmp",	wxBITMAP_TYPE_BMP);
	bitmaps[2] = new wxBitmap("bitmaps/who.bmp",	wxBITMAP_TYPE_BMP);
	bitmaps[3] = new wxBitmap("bitmaps/chat.bmp",	wxBITMAP_TYPE_BMP);
#else
	bitmaps[0] = new wxBitmap(globe_xpm);
	bitmaps[1] = new wxBitmap(mail_xpm);
	bitmaps[2] = new wxBitmap(who_xpm);
	bitmaps[3] = new wxBitmap(chat_xpm);
#endif

	toolBar->AddTool(MENU_CONNECT,
			*bitmaps[0],
			wxNullBitmap,
			FALSE,
			-1, -1,
			(wxObject *)NULL,
			"The WORLD!!");
			
	toolBar->AddSeparator();

	toolBar->AddTool(GOTO_MAIL,
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

	cleanup(0);
}


// Edit menu handler
void MyFrame::OnEditMenu(wxCommandEvent& cmd) {
	int id;
	id = cmd.GetId();
	if (id == EMENU_PREFS) {
		new Preferences(citadel, this);
	}
}



// User menu handler
void MyFrame::OnUsersMenu(wxCommandEvent& cmd) {
	int id;
	
	id = cmd.GetId();
	if (id == UMENU_WHO) {
                //if (TheWholist == NULL)
			TheWholist = new who(citadel, this);
                //else
                        //TheWholist->Activate();
	}
	else if (id == UMENU_SEND_EXPRESS)
		new SendExpress(citadel, this, "");
}


// Rooms menu handler
void MyFrame::OnRoomsMenu(wxCommandEvent& cmd) {
	int id;
	
	id = cmd.GetId();
	if (id == RMENU_GOTO) {
		new RoomView(citadel, this, RoomList->GetNextRoom());
	}
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

	ini->Read("/Citadel Server/Host", &DefaultHost, DEFAULT_HOST);
	ini->Read("/Citadel Server/Port", &DefaultPort, DEFAULT_PORT);

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

void MyFrame::OnGotoMail(wxCommandEvent& unused) {
	new RoomView(citadel, this, "_MAIL_");
	}

void MyFrame::OnTestWin(wxCommandEvent& unused) {
	new TestWindow(citadel, this);
}

void MyFrame::OnSize(wxSizeEvent& WXUNUSED(event) )
{
	int w, h;
	wxString sw, sh;

	// Handle the MDI and roomlist crap
	GetClientSize(&w, &h);
	RoomList->SetSize(0, 0, 200, h);
	GetClientWindow()->SetSize(200, 0, w - 200, h);

	// Remember the size of the window from session to session
	GetSize(&w, &h);
	sw.Printf("%d %d", w, h);
	ini->Write("/Window Sizes/Main", sw);
}
