// userlogin is the frame for logging in.
class TestWindow : public wxMDIChildFrame {
public:
	TestWindow(CitClient *sock, wxMDIParentFrame *MyMDI);
private:
	wxPanel *panel;
	wxTextCtrl *sendcmd;
	wxTextCtrl *recvcmd;
	wxButton *cmd_button;
	wxButton *close_button;
	void OnButtonPressed(wxCommandEvent& whichbutton);
	CitClient *citsock;
	DECLARE_EVENT_TABLE()
};
