#include <wx/socket.h>

class CitClient {
public:
	CitClient(void);
	~CitClient(void);

	// High-level Citadel IPC methods
	int attach(const wxString& host, const wxString& port);
	void detach(void);
	bool IsConnected(void);
	int CitClient::serv_trans(
			wxString& command,
			wxString& response,
			wxStringList& xferbuf
                        );
	int CitClient::serv_trans(wxString& command, wxString& response);
	int CitClient::serv_trans(wxString& command);

	// Various things we learn about the server
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

private:
	void serv_gets(wxString& buf);
	void serv_puts(wxString buf);
	wxSocketClient sock;
	wxIPV4address addr;
	void CitClient::initialize_session(void);
};


