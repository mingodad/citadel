#include <wx/socket.h>

#include "tcp_sockets.hpp"

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
	void download_express_messages(void);
	TCPsocket sock;
	void CitClient::initialize_session(void);
};




// This is a timer that does keepalives...
class keepalive : public wxTimer {
public:
        keepalive(CitClient *sock);
private:
	CitClient *which_sock;
        void Notify(void);
};



