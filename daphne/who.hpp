// userlogin is the frame for logging in.
class who : public wxMDIChildFrame {
public:
	who(CitClient *sock, wxMDIParentFrame *MyMDI);
	int do_login();
private:
	void OnButtonPressed(wxCommandEvent& whichbutton);
	CitClient *citsock;
	wxListCtrl *wholist;
	void LoadWholist();
	DECLARE_EVENT_TABLE()
};
