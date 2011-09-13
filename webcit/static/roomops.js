/*
 * Copyright 1998 - 2009 The Citadel Team
 * Licensed under the GPL V3
 */

// ROOM list vars:
var rooms = null;

// FLOOR list
var floors = null;

var roomsForFloors = new Array();
/* STRUCT KEYS */
/* LKRN etc. */
var RN_ROOM_NAME = 0;
var RN_ROOM_FLAG = 1;
var RN_FLOOR_NUM = 2;
var RN_LIST_ORDER = 3;
var RN_ACCESS_CONTROL = 4;
var RN_CUR_VIEW = 5;
var RN_DEF_VIEW = 6;
var RN_LAST_CHANGE = 7;
var RN_RAFLAGS = 8;

var QR_PRIVATE = 4;
var QR_MAILBOX = 16384;

var UA_KNOWN = 2;
var UA_GOTOALLOWED = 4;
var UA_HASNEWMSGS = 8;
var UA_ZAPPED = 16;

var VIEW_BBS = 0;
var VIEW_MAILBOX = 1;
var VIEW_ADDRESSBOOK = 2;
var VIEW_CALENDAR = 3;
var VIEW_TASKS = 4;
var VIEW_NOTES = 5;
var VIEW_WIKI = 6;
var VIEW_CALBRIEF = 7;
var VIEW_JOURNAL = 8;

function FillRooms(callback) {
  var roomFlr = new Ajax.Request("json_roomflr?SortBy=byfloorroom?SortOrder=1", {method: 'get', onSuccess: function(transport) { ProcessRoomFlr(transport); callback.call(); }});
}
function ProcessRoomFlr(transport) {
  var data = eval('('+transport.responseText+')');
  floors = data["floors"];
  rooms = data["rooms"];
}
function GetRoomsByFloorNum(flnum) {
  var roomsForFloor = new Array();
  var x=0;
  for(var i=0; i<rooms.length; i++) {
    var room = rooms[i];
    var floornum = room[RN_FLOOR_NUM];
    var flag = room[RN_ROOM_FLAG];
    if (flnum == floornum && ((flag & QR_MAILBOX) != QR_MAILBOX)) {
      roomsForFloor[x] = room;
      x++;
    }
  }
  return roomsForFloor;
}
function GetMailboxRooms() {
  var roomsForFloor = new Array();
  var x=0;
  for(var i=0; i<rooms.length; i++) {
    var room = rooms[i];
    var floornum = room[RN_FLOOR_NUM];
    var flag = room[RN_ROOM_FLAG];
    if (floornum == -1) {
      roomsForFloor[x] = room;
      x++;
    }
  }
  return roomsForFloor;
}

/*
 * function to delete a comment from a blog post
 */
function DeleteBlogComment(msgnum) {
	cmd = encodeURI("g_cmd=DELE " + msgnum);
	new Ajax.Request("ajax_servcmd", { 
		parameters: cmd,
		method: 'post',
		onSuccess: function(transport) {
		Effect.BlindUp('blog_comment_' + msgnum);
		}
	});
}
