// userlogin is the frame for logging in.
class UserLogin : public wxFrame {
public:
	UserLogin(CitClient *sock);
	int do_login();
private:
	wxPanel *panel;
	wxTextCtrl *username;
	wxTextCtrl *password;
	wxButton *login_button;
	wxButton *newuser_button;
	wxButton *exit_button;
	void OnButtonPressed(wxCommandEvent& whichbutton);
	DECLARE_EVENT_TABLE()
};
