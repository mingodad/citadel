#include "includes.hpp"



class RoomItem : public wxTreeItemData {
public:
	RoomItem(wxString name);
	wxString RoomName;
};

RoomItem::RoomItem(wxString name) 
	: wxTreeItemData() {

	RoomName = name;	
}





enum {

        ROOMTREE_CTRL
};

BEGIN_EVENT_TABLE(RoomTree, wxTreeCtrl)
	EVT_TREE_ITEM_ACTIVATED(ROOMTREE_CTRL, RoomTree::OnDoubleClick)
END_EVENT_TABLE()



RoomTree::RoomTree(wxMDIParentFrame *parent, CitClient *sock)
		: wxTreeCtrl(
                        parent,
			ROOMTREE_CTRL,
                        wxDefaultPosition, wxDefaultSize,
                        wxTR_HAS_BUTTONS | wxSUNKEN_BORDER,
                        wxDefaultValidator,
                        "RoomList") {

	citsock = sock;
	citMyMDI = parent;
	InitTreeIcons();

}


void RoomTree::InitTreeIcons(void) {
	TreeIcons = new wxImageList(16, 16);
	TreeIcons->Add(*new wxBitmap("bitmaps/root.bmp", wxBITMAP_TYPE_BMP));
	TreeIcons->Add(*new wxBitmap("bitmaps/floor.bmp", wxBITMAP_TYPE_BMP));
	TreeIcons->Add(*new wxBitmap("bitmaps/newroom.bmp", wxBITMAP_TYPE_BMP));
	TreeIcons->Add(*new wxBitmap("bitmaps/oldroom.bmp", wxBITMAP_TYPE_BMP));
	TreeIcons->Add(*new wxBitmap("bitmaps/mailroom.bmp", wxBITMAP_TYPE_BMP));
	SetImageList(TreeIcons);
}


// Load a tree with a room list
//
void RoomTree::LoadRoomList(void) {
	wxString sendcmd, recvcmd, buf, floorname, roomname;
	wxStringList transbuf;
	wxTreeItemId item;
	int i, floornum;

	// First, clear it out.
	DeleteAllItems();

	for (i=0; i<MAXFLOORS; ++i)
		floorboards[i] = (wxTreeItemId) -1;

	// Set the root with the name of the Citadel server.
	AddRoot(
		citsock->HumanNode,
		0,
		-1,
		NULL);

	sendcmd = "LFLR";
	// Bail out silently if we can't retrieve the floor list
	if (citsock->serv_trans(sendcmd, recvcmd, transbuf) != 1) return;

	// Load the floors one by one onto the tree
        for (i=0; i<transbuf.Number(); ++i) {
                buf.Printf("%s", (wxString *)transbuf.Nth(i)->GetData());
		extract(floorname, buf, 1);
		floornum = extract_int(buf, 0);
		floorboards[floornum] = AppendItem(
			GetRootItem(),
			floorname,
			1,
			-1,
			NULL);
	}

	// Load the rooms with new messages into the tree
	sendcmd = "LKRN";
	if (citsock->serv_trans(sendcmd, recvcmd, transbuf) != 1) return;
        for (i=0; i<transbuf.Number(); ++i) {
                buf.Printf("%s", (wxString *)transbuf.Nth(i)->GetData());
		extract(roomname, buf, 0);
		floornum = extract_int(buf, 2);
		item = AppendItem(
			floorboards[floornum],
			roomname,
			2,
			-1,
			new RoomItem(roomname)
			);
		SetItemBold(item, TRUE);
		SetItemBold(floorboards[floornum], TRUE);
	}

	// Load the rooms with new messages into the tree
	sendcmd = "LKRO";
	if (citsock->serv_trans(sendcmd, recvcmd, transbuf) != 1) return;
        for (i=0; i<transbuf.Number(); ++i) {
                buf.Printf("%s", (wxString *)transbuf.Nth(i)->GetData());
		extract(roomname, buf, 0);
		floornum = extract_int(buf, 2);
		AppendItem(
			floorboards[floornum],
			roomname,
			3,
			-1,
			new RoomItem(roomname)
			);
	}

}




void RoomTree::OnDoubleClick(wxTreeEvent& evt) {
	wxTreeItemId itemId;
	int i;
	RoomItem *r;

	itemId = GetSelection();

	// Don't do this unless it's a *room* the user clicked on.
	if (itemId == GetRootItem()) return;
	for (i=0; i<MAXFLOORS; ++i)
		if (itemId == floorboards[i]) return;

	// Ok, it's a room, so go there.
	r = (RoomItem *)GetItemData(itemId);

	new RoomView(citsock, citMyMDI, r->RoomName);
}

