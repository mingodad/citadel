#include <wx/wx.h>		// General-purpose wxWin header
#include <wx/listctrl.h>
#include <wx/socket.h>		// TCP socket client
#include <wx/log.h>
#include <wx/imaglist.h>
#include <wx/treectrl.h>
#include <wx/toolbar.h>
#include <wx/tokenzr.h>		// For the string tokenizer
#include <wx/thread.h>		// No threads, but we need wxCriticalSection
#include <wx/wxhtml.h>		// Vaclav Slavik's HTML display widget

#define MAXFLOORS	128
#define DEFAULT_HOST	"uncnsrd.mt-kisco.ny.us"
#define DEFAULT_PORT	"504"


// Room flags (from ipcdef.h in the main Citadel distribution)

#define QR_PERMANENT	1		/* Room does not purge              */
#define QR_INUSE	2		/* Set if in use, clear if avail    */
#define QR_PRIVATE	4		/* Set for any type of private room */
#define QR_PASSWORDED	8		/* Set if there's a password too    */
#define QR_GUESSNAME	16		/* Set if it's a guessname room     */
#define QR_DIRECTORY	32		/* Directory room                   */
#define QR_UPLOAD	64		/* Allowed to upload                */
#define QR_DOWNLOAD	128		/* Allowed to download              */
#define QR_VISDIR	256		/* Visible directory                */
#define QR_ANONONLY	512		/* Anonymous-Only room              */
#define QR_ANONOPT	1024		/* Anonymous-Option room            */
#define QR_NETWORK	2048		/* Shared network room              */
#define QR_PREFONLY	4096		/* Preferred status needed to enter */
#define QR_READONLY	8192		/* Aide status required to post     */
#define QR_MAILBOX	16384		/* Set if this is a private mailbox */




// CitClient represents an application-level connection to a Citadel server.
class CitClient {
public:
	CitClient(void);
	~CitClient(void);

	// Citadel session-layer commands
	int attach(wxString host, wxString port);
	void detach(void);
	bool IsConnected(void);
	int CitClient::serv_trans(
			wxString& command,
			wxString& response,
			wxString& xferbuf,
			wxString desired_room
                        );
	int CitClient::serv_trans(
			wxString& command,
			wxString& response,
			wxString& xferbuf
                        );
	int CitClient::serv_trans(wxString& command, wxString& response);
	int CitClient::serv_trans(wxString& command);

	// Citadel presentation-layer commands
	bool CitClient::GotoRoom(wxString roomname, wxString password,
				wxString& server_response);

	// Various things we learn about the server ...
	int SessionID;
	wxString NodeName;
	wxString HumanNode;
	wxString FQDN;
	wxString ServerSoftware;
	int ServerRev;
	wxString GeoLocation;
	wxString SysAdmin;
	int ServerType;
	wxString MorePrompt;
	bool UseFloors;
	int PagingLevel;

	// ... and about the user ...
	wxString curr_user;
	wxString curr_pass;
	int axlevel;
	
	// Stuff we have to keep track of ...
	wxString CurrentRoom;

private:
	wxSocketClient *sock;				// transport layer
	wxCriticalSection Critter;
	void serv_gets(wxString& buf);			// session layer
	void serv_puts(wxString buf);			// session layer
	void reconnect_session(void);			// session layer
	void download_express_messages(void);		// presentation layer
	void initialize_session(void);			// presentation layer
	wxString curr_host;
	wxString curr_port;
};




// This is a timer that does keepalives...
class keepalive : public wxTimer {
public:
        keepalive(CitClient *sock);
private:
	CitClient *which_sock;
        void Notify(void);
};


// Receive an express message (page)
class express_message : public wxFrame {
public:
	express_message(CitClient *sock, wxString sender,
		wxString sendsys, wxString msg);
private:
	void OnButtonPressed(wxCommandEvent& whichbutton);
	CitClient *citsock;
	wxString reply_to;
	DECLARE_EVENT_TABLE()
};




// SendExpress is the screen for sending an express message (page).

class SendExpress : public wxMDIChildFrame {
public:
	SendExpress(	CitClient *sock,
			wxMDIParentFrame *MyMDI, 
			wxString touser);
private:
	void OnButtonPressed(wxCommandEvent& whichbutton);
	CitClient *citsock;
	wxMDIParentFrame *citMyMDI;
	wxListBox *ToWhom;
	wxTextCtrl *TheMessage;
	DECLARE_EVENT_TABLE()
};


// Preferences for the application.

class Preferences : public wxMDIChildFrame {
public:
	Preferences(	CitClient *sock,
			wxMDIParentFrame *MyMDI);
private:
	void OnButtonPressed(wxCommandEvent& whichbutton);
	CitClient *citsock;
	wxMDIParentFrame *citMyMDI;
	wxTextCtrl *server_host, *server_port;
	wxCheckBox *server_autoconnect;
	DECLARE_EVENT_TABLE()
};



// Just testing...
class TestWindow : public wxMDIChildFrame {
public:
	TestWindow(CitClient *sock, wxMDIParentFrame *MyMDI);
private:
	wxPanel *panel;
	wxTextCtrl *sendcmd;
	wxTextCtrl *recvcmd;
	wxTextCtrl *xfercmd;
	wxButton *cmd_button;
	wxButton *close_button;
	void OnButtonPressed(wxCommandEvent& whichbutton);
	CitClient *citsock;
	DECLARE_EVENT_TABLE()
};

// Just testing...
class DoChat : public wxMDIChildFrame {
public:
        ChatWindow(CitClient *sock, wxMDIParentFrame *MyMDI);
private:
        wxPanel *panel;
        wxTextCtrl *sendcmd;
        wxTextCtrl *recvcmd;
        wxTextCtrl *xfercmd;
        wxButton *cmd_button;
        wxButton *close_button;
        void OnButtonPressed(wxCommandEvent& whichbutton);
        CitClient *citsock;
        DECLARE_EVENT_TABLE()
};



// userlogin is the frame for logging in.
class UserLogin : public wxMDIChildFrame {
public:
	UserLogin(CitClient *sock, wxMDIParentFrame *MyMDI);
	int do_login();
private:
	wxPanel *panel;
	wxTextCtrl *username;
	wxTextCtrl *password;
	wxButton *login_button;
	wxButton *newuser_button;
	wxButton *exit_button;
	void OnButtonPressed(wxCommandEvent& whichbutton);
	void UserLogin::BeginSession(wxString serv_response);
	CitClient *citsock;
	wxMDIParentFrame *citMyMDI;
	DECLARE_EVENT_TABLE()
};


// Who is online?
class who : public wxMDIChildFrame {
public:
	who(CitClient *sock, wxMDIParentFrame *MyMDI);
	int do_login();
	void LoadWholist();
private:
	void OnButtonPressed(wxCommandEvent& whichbutton);
	CitClient *citsock;
	wxListCtrl *wholist;
	DECLARE_EVENT_TABLE()
};


// This is a timer that periodically refreshes the wholist.
class who_refresh : public wxTimer {
public:
	who_refresh(who *parent_who);
private:
	who *which_who;
	void Notify(void);
};



// Global server properties

class ServProps : public wxMDIChildFrame {
public:
	ServProps(	CitClient *sock,
			wxMDIParentFrame *MyMDI,
			wxString WhichPanel);
	void ChangePanel(wxString WhichPanel);
private:
	void OnButtonPressed(wxCommandEvent& whichbutton);
	CitClient *citsock;
	wxMDIParentFrame *citMyMDI;
	wxPanel *identity_panel, *network_panel, *security_panel;
	wxString ServerConfigStrings[20];
	void LoadServerConfigStrings(void);
	void SaveServerConfigStrings(void);
	DECLARE_EVENT_TABLE()
};



// The ever-present tree of floors and rooms

class RoomTree : public wxTreeCtrl {
public:
	RoomTree(wxMDIParentFrame *parent, CitClient *sock);
	void LoadRoomList(void);
	wxString GetNextRoom(void);
private:
	wxTreeItemId GetNextRoomId(void);
	void InitTreeIcons(void);
	void OnDoubleClick(wxTreeEvent& evt);
	CitClient *citsock;
	wxMDIParentFrame *citMyMDI;
	wxTreeItemId floorboards[MAXFLOORS];
	wxImageList *TreeIcons;
	wxTreeItemId march_next;
	ServProps *CurrServProps;
	DECLARE_EVENT_TABLE()
};



// A window representing an open room.
class RoomView : public wxMDIChildFrame {
public:
	RoomView(CitClient *sock, wxMDIParentFrame *MyMDI, wxString roomname);
private:
	void OnButtonPressed(wxCommandEvent& whichbutton);
	CitClient *citsock;
	void do_readloop(wxString readcmd);
	wxMDIParentFrame *citMyMDI;
	wxHtmlWindow *message_window;
        wxPanel *banner;
        wxButton *close_button;
	wxString ThisRoom;
	unsigned int RoomFlags;
	unsigned int new_messages;
	unsigned int total_messages;
	bool is_roomaide;
	DECLARE_EVENT_TABLE()
};


class SelectUser : public wxPanel {
public:
	SelectUser(CitClient *, wxWindow *, wxString,
		wxString, unsigned int, wxTextCtrl *);
private:
	void OnButtonPressed(wxCommandEvent& whichbutton);
	void AddLocalUsers(wxTreeCtrl *, CitClient *);
	wxButton select_button;
	wxButton cancel_button;
	CitClient *citsock;
	wxTextCtrl *target_textctrl;
	wxTreeCtrl *TheTree;
	DECLARE_EVENT_TABLE()
};



class EnterMessage : public wxMDIChildFrame {
public:
	EnterMessage(CitClient *sock, wxMDIParentFrame *MyMDI,
		wxString roomname, unsigned int roomflags);
private:
	void OnCancel(wxCommandEvent& whichbutton);
	void OnSave(wxCommandEvent& whichbutton);
	void OnFind(wxCommandEvent& whichbutton);
	CitClient *citsock;
	wxMDIParentFrame *citMyMDI;
	wxString ThisRoom;
	wxChoice *fromname;
	wxTextCtrl *toname;
	wxTextCtrl *TheMessage;
	SelectUser *finduser_panel;
	DECLARE_EVENT_TABLE()
};





	
// A message.  (This is not a GUI class; it's used for internal
// representation.)
class CitMessage {
public:
	CitMessage(CitClient *sock, wxString getmsg_cmd, wxString inRoom);
	wxString author;
	wxString recipient;
	long timestamp;
	wxString room;
	wxString msgtext;
	wxString nodename;
private:
	int format_type;
};



// Stuff from utils.cpp

void extract(wxString& outputbuf, wxString inputbuf, int parmnum);
int extract_int(wxString inputbuf, int parmnum);
void load_roomlist(RoomTree *tree, CitClient *citsock);
void variformat_to_html(wxString& outputbuf,
                        wxString inputbuf,
                        bool add_header_and_footer);
wxString generate_html_header(CitMessage *, wxString, wxString);



void cleanup(int);



// Globals

extern wxMDIParentFrame *BigMDI;
extern RoomTree *RoomList;
extern CitClient *citadel;
extern wxConfig *ini;
