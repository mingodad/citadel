#include "includes.hpp"

#ifdef __WXMOTIF__
#include "bitmaps/root.xpm"
#include "bitmaps/floor.xpm"
#include "bitmaps/newroom.xpm"
#include "bitmaps/oldroom.xpm"
#include "bitmaps/mailroom.xpm"
#endif


wxTreeItemId null_item;


class RoomItem : public wxTreeItemData {
public:
	RoomItem(wxString name, bool newmsgs);
	wxString RoomName;
	bool HasNewMessages;
	wxTreeItemId nextroom;
};

RoomItem::RoomItem(wxString name, bool newmsgs)
	: wxTreeItemData() {

	RoomName = name;
	HasNewMessages = newmsgs;
	nextroom = null_item;
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
#ifdef __WXMOTIF__
	TreeIcons->Add(*new wxBitmap(root_xpm));
	TreeIcons->Add(*new wxBitmap(floor_xpm));
	TreeIcons->Add(*new wxBitmap(newroom_xpm));
	TreeIcons->Add(*new wxBitmap(oldroom_xpm));
	TreeIcons->Add(*new wxBitmap(mailroom_xpm));
#else
	TreeIcons->Add(*new wxBitmap("bitmaps/root.bmp", wxBITMAP_TYPE_BMP));
	TreeIcons->Add(*new wxBitmap("bitmaps/floor.bmp", wxBITMAP_TYPE_BMP));
	TreeIcons->Add(*new wxBitmap("bitmaps/newroom.bmp", wxBITMAP_TYPE_BMP));
	TreeIcons->Add(*new wxBitmap("bitmaps/oldroom.bmp", wxBITMAP_TYPE_BMP));
	TreeIcons->Add(*new wxBitmap("bitmaps/mailroom.bmp", wxBITMAP_TYPE_BMP));
#endif
	SetImageList(TreeIcons);
}


// Load a tree with a room list
//
void RoomTree::LoadRoomList(void) {
	wxString sendcmd, recvcmd, buf, floorname, roomname, transbuf;
	wxTreeItemId item;
	wxTreeItemId prev;
	int i, pos, floornum;

	prev = null_item;

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
	while (pos = transbuf.Find('\n', FALSE), (pos >= 0)) {
		buf = transbuf.Left(pos);
		transbuf = transbuf.Mid(pos+1);
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
	while (pos = transbuf.Find('\n', FALSE), (pos >= 0)) {
		buf = transbuf.Left(pos);
		transbuf = transbuf.Mid(pos+1);
		extract(roomname, buf, 0);
		floornum = extract_int(buf, 2);
		item = AppendItem(
			floorboards[floornum],
			roomname,
			2,
			-1,
			new RoomItem(roomname, TRUE)
			);
		SetItemBold(item, TRUE);
		SetItemBold(floorboards[floornum], TRUE);
		if (prev == null_item)
			march_next = item;
		else
			((RoomItem *)GetItemData(prev))->nextroom = item;
		prev = item;
	}

	// Load the rooms with new messages into the tree
	sendcmd = "LKRO";
	if (citsock->serv_trans(sendcmd, recvcmd, transbuf) != 1) return;
	while (pos = transbuf.Find('\n', FALSE), (pos >= 0)) {
		buf = transbuf.Left(pos);
		transbuf = transbuf.Mid(pos+1);
		extract(roomname, buf, 0);
		floornum = extract_int(buf, 2);
		AppendItem(
			floorboards[floornum],
			roomname,
			3,
			-1,
			new RoomItem(roomname, FALSE)
			);
	}

	// FIX demo of traversal
	while (march_next != null_item) {
		wxTreeItemId foo = GetNextRoom();
		cout << ((RoomItem *)GetItemData(foo))->RoomName << "\n";
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



wxTreeItemId RoomTree::GetNextRoom(void) {

	wxTreeItemId ret;

	ret = march_next;

	if (march_next == null_item)
		LoadRoomList();
	else
		march_next = ((RoomItem *)GetItemData(march_next))->nextroom;

	return ret;
}


