#include <wx/socket.h>

class CitClient {
public:
	CitClient(void);
	~CitClient(void);
	int attach(const wxString& host, const wxString& port);
	void detach(void);
	bool IsConnected(void);
	void serv_gets(wxString& buf);
	void serv_puts(wxString buf);
	int CitClient::serv_trans(
			wxString& command,
			wxString& response,
			wxStringList& xferbuf
                        );
	int CitClient::serv_trans(wxString& command, wxString& response);
	int CitClient::serv_trans(wxString& command);
private:
	wxSocketClient sock;
	wxIPV4address addr;
};


