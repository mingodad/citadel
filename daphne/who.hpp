// userlogin is the frame for logging in.
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
