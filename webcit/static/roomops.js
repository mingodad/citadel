/*
 * Copyright 1998 - 2009 The Citadel Team
 * Licensed under the GPL V3
 */

// ROOM list vars:
var rooms = null;

// FLOOR list
var floors = null;

var roomsForFloors = [];
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

function fillRooms(callback) {
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
function getMailboxRooms() {
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

function GenericTreeRoomList(roomlist) {
  var currentExpanded = ctdlLocalPrefs.readPref("rooms_expanded");
  var curRoomName = "";
  if (document.getElementById("rmname")) {
    curRoomName = getTextContent(document.getElementById("rmname"));
  }
  currentDropTargets = [];
  var iconbar = document.getElementById("iconbar");
  var ul = document.createElement("ul");
  roomlist.appendChild(ul);
  // Add mailbox, because they are special
  var mailboxLI = document.createElement("li");
  ul.appendChild(mailboxLI);
  var mailboxSPAN = document.createElement("span");
  var _mailbox = getTextContent(document.getElementById("mbox_template"));
  mailboxSPAN.appendChild(document.createTextNode(_mailbox));
  $(mailboxSPAN).observe('click', expandFloorEvent);
  mailboxLI.appendChild(mailboxSPAN);
  mailboxLI.className = "floor";
  var mailboxUL = document.createElement("ul");
  mailboxLI.appendChild(mailboxUL);
  var mailboxRooms = getMailboxRooms();
  for(var i=0; i<mailboxRooms.length; i++) {
	  var room = mailboxRooms[i];
	  currentDropTargets.push(addRoomToList(mailboxUL, room, curRoomName));
  }
  if (currentExpanded !== null && currentExpanded === _mailbox ) {
	  expandFloor(mailboxSPAN);
  }
    for(var a=0; a<floors.length; a++) {
	var floor = floors[a];
	var floornum = floor[0];
	
	if (floornum !== -1)
	{

	    var name = floor[1];
	    var floorLI = document.createElement("li");
	    ul.appendChild(floorLI);
	    var floorSPAN = document.createElement("span");
	    floorSPAN.appendChild(document.createTextNode(name));
	    $(floorSPAN).observe('click', expandFloorEvent);
	    floorLI.appendChild(floorSPAN);
	    floorLI.className = "floor";
	    var floorUL = document.createElement("ul");
	    floorLI.appendChild(floorUL);
	    var roomsForFloor = GetRoomsByFloorNum(floornum);
	    for(var b=0; b<roomsForFloor.length; b++) {
		var oneRoom = roomsForFloor[b];
		currentDropTargets.push(addRoomToList(floorUL, oneRoom, curRoomName));
	    }
	    if (currentExpanded !== null && currentExpanded === name) {
		expandFloor(floorSPAN);
	    }
	}
    }
}
function iconBarRoomList() {
  roomlist = document.getElementById("roomlist");
  GenericTreeRoomList(roomlist);
}
function knRoomsRoomList() {
  roomlist = document.getElementById("roomlist_knrooms");
  GenericTreeRoomList(roomlist);
}

function addRoomToList(floorUL,room, roomToEmphasize) {
  var roomName = room[RN_ROOM_NAME];
  var flag = room[RN_ROOM_FLAG];
  var curView = room[RN_CUR_VIEW];
  var view = room[RN_DEF_VIEW];
  var raflags = room[RN_RAFLAGS];
  var isMailBox = ((flag & QR_MAILBOX) === QR_MAILBOX);
  var hasNewMsgs = ((raflags & UA_HASNEWMSGS) === UA_HASNEWMSGS);
  var roomLI = document.createElement("li");
  var roomA = document.createElement("a");
  roomA.setAttribute("href","dotgoto?room="+encodeURIComponent(roomName));
  roomA.appendChild(document.createTextNode(roomName));
  roomLI.appendChild(roomA);
  floorUL.appendChild(roomLI);
  var className = "room ";
  if (view === VIEW_MAILBOX) {
    className += "room-private";
  } else if (view === VIEW_ADDRESSBOOK) {
    className += "room-addr";
  } else if (view === VIEW_CALENDAR || view === VIEW_CALBRIEF) {
    className += "room-cal";
  } else if (view === VIEW_TASKS) {
    className += "room-tasks";
  } else if (view === VIEW_NOTES) {
    className += "room-notes";
  } else {
    className += "room-chat";
  }
  if (hasNewMsgs) {
    className += " room-newmsgs";
  }
  if (roomName === roomToEmphasize) {
    className += " room-emphasized";
  }
  roomLI.setAttribute("class", className);
  roomA.dropTarget = true;
  roomA.dropHandler = roomListDropHandler;
  return roomLI;
}

function roomListDropHandler(target, dropped) {
  var mvCommand;
  if (dropped.getAttribute("citadel:msgid")) {
      var room = getTextContent(target);
      var msgIds = "";
      for(var msgId in currentlyMarkedRows) {
	  if (currentlyMarkedRows.hasOwnProperty(msgId)) { //defined in summaryview.js
	      msgIds += ","+msgId;
	      if (msgIds.length > 800) {
		  mvCommand = "g_cmd=MOVE%20" + msgIds + "|"+encodeURIComponent(room)+"|0";
		  new Ajax.Request("ajax_servcmd", {
		      parameters: mvCommand,
		      method: 'post',
		  });
		  msgIds = "";
	      }
	  }

      }
      mvCommand = "g_cmd=MOVE%20" + msgIds + "|"+encodeURIComponent(room)+"|0";
      new Ajax.Request('ajax_servcmd', {
	  method: 'post',
	  parameters: mvCommand,
	  onComplete: deleteAllMarkedRows()});
  }
}
function expandFloorEvent(event) {
  expandFloor(event.target);
}
function expandFloor(target) {
  if (target.nodeName.toLowerCase() !== "span") {
    return; // ignore clicks on child UL
  }
  ctdlLocalPrefs.setPref("rooms_expanded", target.firstChild.nodeValue);
  var parentUL = target.parentNode;
  if (currentlyExpandedFloor !== null) {
    currentlyExpandedFloor.className = currentlyExpandedFloor.className.replace("floor-expanded","");
  }
  parentUL.className = parentUL.className + " floor-expanded";
  currentlyExpandedFloor = parentUL;
}
