class express_message : public wxFrame {
public:
	express_message(CitClient *sock, wxString sender,
		wxString sendsys, wxStringList msg);
private:
	void OnButtonPressed(wxCommandEvent& whichbutton);
	CitClient *citsock;
	DECLARE_EVENT_TABLE()
};
