/*
 * JavaScript function library for WebCit.
 *
 * Copyright (c) 2005-2012 by the citadel.org team
 *
 * This program is open source software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3.
 * the Free Software Foundation, either version 3 of the License, or
 * 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * 
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

var browserType;
var room_is_trash = 0;

var currentlyExpandedFloor = null;
var roomlist = null;
var currentDropTarget = null;

var supportsAddEventListener = (!!document.addEventListener);
var today = new Date();

var wc_log = "";
if (document.all) {browserType = "ie"}
if (window.navigator.userAgent.toLowerCase().match("gecko")) {
	browserType= "gecko";
}
var ns6=document.getElementById&&!document.all;
Event.observe(window, 'load', ToggleTaskDateOrNoDateActivate);
Event.observe(window, 'load', taskViewActivate);
//document.observe("dom:loaded", setupPrefEngine);
document.observe("dom:loaded", setupIconBar);
function CtdlRandomString()  {
	return((Math.random()+'').substr(3));
}
function strcmp ( str1, str2 ) {
    // http://kevin.vanzonneveld.net
    // +   original by: Waldo Malqui Silva
    // +      input by: Steve Hilder
    // +   improved by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
    // +    revised by: gorthaur
    // *     example 1: strcmp( 'waldo', 'owald' );
    // *     returns 1: 1
    // *     example 2: strcmp( 'owald', 'waldo' );
    // *     returns 2: -1
 
    return ( ( str1 == str2 ) ? 0 : ( ( str1 > str2 ) ? 1 : -1 ) );
}

function CtdlMarkLog($Which, $Status)
{
    if ($Status)
	document.getElementById($Which).checked == false;
    else
	document.getElementById($Which).checked == true;
 
}
function ToggleLogEnable($Which)
{
    var p;
    var Status = !document.getElementById($Which).checked;
    if (Status)
	p= encodeURI('g_cmd=LOGS ' + $Which + '|0');
    else
	p= encodeURI('g_cmd=LOGS ' + $Which + '|1');
    new Ajax.Request('ajax_servcmd', {
	method: 'post',
	parameters: p,
	onComplete: CtdlMarkLog($Which, Status)
    });
}

function ToggleVisibility ($Which)
{
	if (document.getElementById)
	{
		if (document.getElementById($Which).style.display  == "none")
			document.getElementById($Which).style.display  = "inline";
		else
			document.getElementById($Which).style.display  = "none";
	}
}

function emptyElement(element) {
  childNodes = element.childNodes;
  for(var i=0; i<childNodes.length; i++) {
    try {
    element.removeChild(childNodes[i]);
    } catch (e) {
      WCLog(e+"|"+e.description);
    }
  }
}
/** Implements superior internet explorer 'extract all child text from element' feature'. Falls back on buggy, patent violating standardized method */
function getTextContent(element) {
  if (element.textContent == undefined) {
    return element.innerText;
  }
  return element.textContent;
}
/** Same reasons as above */
function setTextContent(element, textContent) {
  if(element.textContent == undefined) {
    element.innerText = textContent;
  } else {
  element.textContent = textContent;
  }
}

// We love string tokenizers.
function extract_token(source_string, token_num, delimiter) {
	var i = 0;
	var extracted_string = source_string;

	if (token_num > 0) {
		for (i=0; i<token_num; ++i) {
			var j = extracted_string.indexOf(delimiter);
			if (j >= 0) {
				extracted_string = extracted_string.substr(j+1);
			}
		}
	}

	j = extracted_string.indexOf(delimiter);
	if (j >= 0) {
		extracted_string = extracted_string.substr(0, j);
	}

	return extracted_string;
}

function CtdlSpawnContextMenu(event, source) {
  // remove any existing menus
  disintergrateContextMenus(null);
  var x = event.clientX-10; // cut a few pixels out so our mouseout works right
  var y = event.clientY-10;
  var contextDIV = document.createElement("div");
  contextDIV.setAttribute("id", "ctdlContextMenu");
  document.body.appendChild(contextDIV);
  var sourceChildren = source.childNodes;
  for(var j=0; j<sourceChildren.length; j++) {
    contextDIV.appendChild(sourceChildren[j].cloneNode(true));
  }
  var leftRule = "left: "+x+"px;";
  contextDIV.setAttribute("style", leftRule);
  contextDIV.setAttribute("actual", leftRule);
  contextDIV.style.top = y+"px";
  contextDIV.style.display = "block";
  $(contextDIV).observe('mouseout',disintergrateContextMenus);
}
function disintergrateContextMenus(event) {
  var contextMenu = document.getElementById("ctdlContextMenu");
  if (contextMenu) {
    contextMenu.parentNode.removeChild(contextMenu);
  }
  Event.stopObserving(document,'click',disintergrateContextMenus);
}
// This code handles the popups for important-messages.
function hide_imsg_popup() {
	if (browserType == "gecko") {
		document.poppedLayer = eval('document.getElementById(\'important_message\')');
	}
	else if (browserType == "ie") {
		document.poppedLayer = eval('document.all[\'important_message\']');
	}
	else {
		document.poppedLayer = eval('document.layers[\'`important_message\']');
	}

	document.poppedLayer.style.visibility = "hidden";
}


// This function activates the ajax-powered recipient autocompleters on the message entry screen.
function activate_entmsg_autocompleters() {
	new Ajax.Autocompleter('cc_id', 'cc_name_choices', 'cc_autocomplete', {} );
	new Ajax.Autocompleter('bcc_id', 'bcc_name_choices', 'bcc_autocomplete', {} );
	new Ajax.Autocompleter('recp_id', 'recp_name_choices', 'recp_autocomplete', {} );
}

function activate_iconbar_wholist_populat0r() 
{
	new Ajax.PeriodicalUpdater('online_users', 'do_template?template=who_iconbar', {method: 'get', frequency: 30});
}

function setupIconBar() {

	/* WARNING: VILE, SLEAZY HACK.  We determine the state of the box based on the image loaded. */
	if ( $('expand_roomlist').src.substring($('expand_roomlist').src.length - 12) == "collapse.gif" ) {
		$('roomlist').style.display = 'block';
		$('roomlist').innerHTML = '';
		FillRooms(IconBarRoomList);
	}
	else {
		$('roomlist').style.display = 'none';
	}

	/* WARNING: VILE, SLEAZY HACK.  We determine the state of the box based on the image loaded. */
	if ( $('expand_wholist').src.substring($('expand_wholist').src.length - 12) == "collapse.gif" ) {
		$('online_users').style.display = 'block';
		activate_iconbar_wholist_populat0r();
	}
	else {
		$('online_users').style.display = 'none';
	}

}

function GenericTreeRoomList(roomlist) {
  var currentExpanded = ctdlLocalPrefs.readPref("rooms_expanded");
  var curRoomName = "";
  if (document.getElementById("rmname")) {
    curRoomName = getTextContent(document.getElementById("rmname"));
  }
  currentDropTargets = new Array();
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
  var mailboxRooms = GetMailboxRooms();
  for(var i=0; i<mailboxRooms.length; i++) {
	  var room = mailboxRooms[i];
	  currentDropTargets.push(addRoomToList(mailboxUL, room, curRoomName));
  }
  if (currentExpanded != null && currentExpanded == _mailbox ) {
	  expandFloor(mailboxSPAN);
  }
  for(var a=0; a<floors.length; a++) {
	  var floor = floors[a];
	  var floornum = floor[0];
    
	  if (floornum != -1)
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
			  var room = roomsForFloor[b];
			  currentDropTargets.push(addRoomToList(floorUL, room, curRoomName));
		  }
		  if (currentExpanded != null && currentExpanded == name) {
			  expandFloor(floorSPAN);
		  }
    }
  }
}
function IconBarRoomList() {
  roomlist = document.getElementById("roomlist");
  GenericTreeRoomList(roomlist);
}
function KNRoomsRoomList() {
  roomlist = document.getElementById("roomlist_knrooms");
  GenericTreeRoomList(roomlist);
}

function addRoomToList(floorUL,room, roomToEmphasize) {
  var roomName = room[RN_ROOM_NAME];
  var flag = room[RN_ROOM_FLAG];
  var curView = room[RN_CUR_VIEW];
  var view = room[RN_DEF_VIEW];
  var raflags = room[RN_RAFLAGS];
  var isMailBox = ((flag & QR_MAILBOX) == QR_MAILBOX);
  var hasNewMsgs = ((raflags & UA_HASNEWMSGS) == UA_HASNEWMSGS);
  var roomLI = document.createElement("li");
  var roomA = document.createElement("a");
  roomA.setAttribute("href","dotgoto?room="+encodeURIComponent(roomName));
  roomA.appendChild(document.createTextNode(roomName));
  roomLI.appendChild(roomA);
  floorUL.appendChild(roomLI);
  var className = "room ";
  if (view == VIEW_MAILBOX) {
    className += "room-private"
  } else if (view == VIEW_ADDRESSBOOK) {
    className += "room-addr";
  } else if (view == VIEW_CALENDAR || view == VIEW_CALBRIEF) {
    className += "room-cal";
  } else if (view == VIEW_TASKS) {
    className += "room-tasks";
  } else if (view == VIEW_NOTES) {
    className += "room-notes";
  } else {
    className += "room-chat";
  }
  if (hasNewMsgs) {
    className += " room-newmsgs";
  }
  if (roomName == roomToEmphasize) {
    className += " room-emphasized";
  }
  roomLI.setAttribute("class", className);
  roomA.dropTarget = true;
  roomA.dropHandler = roomListDropHandler;
  return roomLI;
}

function roomListDropHandler(target, dropped) {
  if (dropped.getAttribute("citadel:msgid")) {
      var room = getTextContent(target);
      var msgIds = "";
      for(msgId in currentlyMarkedRows) { //defined in summaryview.js
	  msgIds += ","+msgId;
	  if (msgIds.length > 800) {
	      var mvCommand = encodeURI("g_cmd=MOVE " + msgIds + "|"+room+"|0");
	      new Ajax.Request("ajax_servcmd", {
		  parameters: mvCommand,
		  method: 'post',
	      });
	      msgIds = "";
	  }

      }
      var mvCommand = encodeURI("g_cmd=MOVE " + msgIds + "|"+room+"|0");
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
  if (target.nodeName.toLowerCase() != "span") {
    return; // ignore clicks on child UL
  }
  ctdlLocalPrefs.setPref("rooms_expanded", target.firstChild.nodeValue);
  var parentUL = target.parentNode;
  if (currentlyExpandedFloor != null) {
    currentlyExpandedFloor.className = currentlyExpandedFloor.className.replace("floor-expanded","");
  }
  parentUL.className = parentUL.className + " floor-expanded";
  currentlyExpandedFloor = parentUL;
}

// These functions handle moving sticky notes around the screen by dragging them

var uid_of_note_being_dragged = 0;
var saved_cursor_style = 'default';
var note_was_dragged = 0;

function NotesDragMouseUp(evt) {
	document.onmouseup = null;
	document.onmousemove = null;
	if (document.layers) {
		document.releaseEvents(Event.MOUSEUP | Event.MOUSEMOVE);
	}

	d = $('note-' + uid_of_note_being_dragged);
	d.style.cursor = saved_cursor_style;

	// If any motion actually occurred, submit an ajax http call to record it to the server
	if (note_was_dragged > 0) {
		p = 'note_uid=' + uid_of_note_being_dragged
			+ '&left=' + d.style.left
			+ '&top=' + d.style.top
			+ '&r=' + CtdlRandomString();
		new Ajax.Request(
			'ajax_update_note',
			{
				method: 'post',
				parameters: p
			}
		);
	}

	uid_of_note_being_dragged = '';
	return true;
}

function NotesDragMouseMove(evt) {
	x = (ns6 ? evt.clientX : event.clientX);
	x_increment = x - saved_x;
	y = (ns6 ? evt.clientY : event.clientY);
	y_increment = y - saved_y;

	// Move the div
	d = $('note-' + uid_of_note_being_dragged);

	divTop = parseInt(d.style.top);
	divLeft = parseInt(d.style.left);

	d.style.top = (divTop + y_increment) + 'px';
	d.style.left = (divLeft + x_increment) + 'px';

	saved_x = x;
	saved_y = y;
	note_was_dragged = 1;
	return true;
}


function NotesDragMouseDown(evt, uid) {
	saved_x = (ns6 ? evt.clientX : event.clientX);
	saved_y = (ns6 ? evt.clientY : event.clientY);
	document.onmouseup = NotesDragMouseUp;
	document.onmousemove = NotesDragMouseMove;
	if (document.layers) {
		document.captureEvents(Event.MOUSEUP | Event.MOUSEMOVE);
	}
	uid_of_note_being_dragged = uid;
	d = $('note-' + uid_of_note_being_dragged);
	saved_cursor_style = d.style.cursor;
	d.style.cursor = 'move';
	return false;		// disable the default action
}


// Called when the user clicks on the palette icon of a sticky note to change its color.
// It toggles the color selector visible or invisible.

function NotesClickPalette(evt, uid) {
	uid_of_note_being_colored = uid;
	d = $('palette-' + uid_of_note_being_colored);

	if (d.style.display) {
		if (d.style.display == 'none') {
			d.style.display = 'block';
		}
		else {
			d.style.display = 'none';
		}
	}
	else {
		d.style.display = 'block';
	}

	return true;
}


// Called when the user clicks on one of the colors in an open color selector.
// Sets the desired color and then closes the color selector.

function NotesClickColor(evt, uid, red, green, blue, notecolor, titlecolor) {
	uid_of_note_being_colored = uid;
	palette_button = $('palette-' + uid_of_note_being_colored);
	note_div = $('note-' + uid_of_note_being_colored);
	titlebar_div = $('titlebar-' + uid_of_note_being_colored);

	// alert('FIXME red=' + red + ' green=' + green + ' blue=' + blue);

	note_div.style.backgroundColor = notecolor;
	titlebar_div.style.backgroundColor = titlecolor;
	palette_button.style.display = 'none';

	// submit an ajax http call to record it to the server
	p = 'note_uid=' + uid_of_note_being_colored
		+ '&red=' + red
		+ '&green=' + green
		+ '&blue=' + blue
		+ '&r=' + CtdlRandomString();
	new Ajax.Request(
		'ajax_update_note',
		{
			method: 'post',
			parameters: p
		}
	);
}




// These functions handle resizing sticky notes by dragging the resize handle

var uid_of_note_being_resized = 0;
var saved_cursor_style = 'default';
var note_was_resized = 0;

function NotesResizeMouseUp(evt) {
	document.onmouseup = null;
	document.onmousemove = null;
	if (document.layers) {
		document.releaseEvents(Event.MOUSEUP | Event.MOUSEMOVE);
	}

	d = $('note-' + uid_of_note_being_resized);
	d.style.cursor = saved_cursor_style;

	// If any motion actually occurred, submit an ajax http call to record it to the server
	if (note_was_resized > 0) {
		p = 'note_uid=' + uid_of_note_being_resized
			+ '&width=' + d.style.width
			+ '&height=' + d.style.height
			+ '&r=' + CtdlRandomString();
		new Ajax.Request(
			'ajax_update_note',
			{
				method: 'post',
				parameters: p
			}
		);
	}

	uid_of_note_being_resized = '';
	return false;		// disable the default action
}

function NotesResizeMouseMove(evt) {
	x = (ns6 ? evt.clientX : event.clientX);
	x_increment = x - saved_x;
	y = (ns6 ? evt.clientY : event.clientY);
	y_increment = y - saved_y;

	// Move the div
	d = $('note-' + uid_of_note_being_resized);

	divTop = parseInt(d.style.height);
	divLeft = parseInt(d.style.width);

	newHeight = divTop + y_increment;
	if (newHeight < 50) newHeight = 50;

	newWidth = divLeft + x_increment;
	if (newWidth < 50) newWidth = 50;

	d.style.height = newHeight + 'px';
	d.style.width = newWidth + 'px';

	saved_x = x;
	saved_y = y;
	note_was_resized = 1;
	return false;		// disable the default action
}


function NotesResizeMouseDown(evt, uid) {
	saved_x = (ns6 ? evt.clientX : event.clientX);
	saved_y = (ns6 ? evt.clientY : event.clientY);
	document.onmouseup = NotesResizeMouseUp;
	document.onmousemove = NotesResizeMouseMove;
	if (document.layers) {
		document.captureEvents(Event.MOUSEUP | Event.MOUSEMOVE);
	}
	uid_of_note_being_resized = uid;
	d = $('note-' + uid_of_note_being_resized);
	saved_cursor_style = d.style.cursor;
	d.style.cursor = 'move';
	return false;		// disable the default action
}


function DeleteStickyNote(evt, uid, confirmation_prompt) {
	uid_of_note_being_deleted = uid;
	d = $('note-' + uid_of_note_being_deleted);

	if (confirm(confirmation_prompt)) {
		new Effect.Puff(d);

		// submit an ajax http call to delete it on the server
		p = 'note_uid=' + uid_of_note_being_deleted
			+ '&deletenote=yes'
			+ '&r=' + CtdlRandomString();
		new Ajax.Request(
			'ajax_update_note',
			{
				method: 'post',
				parameters: p
			}
		);
	}
}

function ctdl_ts_getInnerText(el) {
	if (typeof el == "string") return el;
	if (typeof el == "undefined") { return el };
	if (el.innerText) return el.innerText;	//Not needed but it is faster
	var str = "";
	
	var cs = el.childNodes;
	var l = cs.length;
	for (var i = 0; i < l; i++) {
		switch (cs[i].nodeType) {
			case 1: //ELEMENT_NODE
				str += ts_getInnerText(cs[i]);
				break;
			case 3:	//TEXT_NODE
				str += cs[i].nodeValue;
				break;
		}
	}
	return str;
}


// Place a gradient loadscreen on an element, e.g to use before Ajax.updater
function CtdlLoadScreen(elementid) {
var elem = document.getElementById(elementid);
elem.innerHTML = "<div align=center><br><table border=0 cellpadding=10 bgcolor=\"#ffffff\"><tr><td><img src=\"static/throbber.gif\" /><font color=\"#AAAAAA\">&nbsp;&nbsp;Loading....</font></td></tr></table><br></div>";
}



// Pop open the address book (target_input is the INPUT field to populate)

function PopOpenAddressBook(target_input) {
	$('address_book_popup').style.display = 'block';
	p = 'target_input=' + target_input + '&r=' + CtdlRandomString();
	new Ajax.Updater(
		'address_book_popup_middle_div',
		'display_address_book_middle_div',
		{
			method: 'get',
			parameters: p,
			evalScripts: true
		}
	);
}

function PopulateAddressBookInnerDiv(which_addr_book, target_input) {
	$('address_book_inner_div').innerHTML = "<div align=center><br><table border=0 cellpadding=10 bgcolor=\"#ffffff\"><tr><td><img src=\"static/throbber.gif\" /><font color=\"#AAAAAA\">&nbsp;&nbsp;Loading....</font></td></tr></table><br></div>";
	p = 'which_addr_book=' + which_addr_book
	  + '&target_input=' + target_input
	  + '&r=' + CtdlRandomString();
	new Ajax.Updater(
		'address_book_inner_div',
		'display_address_book_inner_div',
		{
			method: 'get',
			parameters: p
		}
	);
}

// What happens when a contact is selected from the address book popup
// (populate the specified target)

function AddContactsToTarget(target, whichaddr) {
	while (whichaddr.selectedIndex != -1) {
		if (target.value.length > 0) {
			target.value = target.value + ', ';
		}
		target.value = target.value + whichaddr.value;
		whichaddr.options[whichaddr.selectedIndex].selected = false;
	}
}

// Respond to a meeting invitation
function RespondToInvitation(question_divname, title_divname, msgnum, cal_partnum, sc) {
	p = 'msgnum=' + msgnum + '&cal_partnum=' + cal_partnum + '&sc=' + sc ;
	new Ajax.Updater(title_divname, 'respond_to_request', { method: 'post', parameters: p } );
	Effect.Fade(question_divname, { duration: 0.5 });
}

// Handle a received RSVP
function HandleRSVP(question_divname, title_divname, msgnum, cal_partnum, sc) {
	p = 'msgnum=' + msgnum + '&cal_partnum=' + cal_partnum + '&sc=' + sc ;
	new Ajax.Updater(title_divname, 'handle_rsvp', { method: 'post', parameters: p } );
	Effect.Fade(question_divname, { duration: 0.5 });
}
/* var fakeMouse = document.createEvent("MouseEvents");
 fakeMouse.initMouseEvent("click", true, true, window, 
   0,0,0,0,0, false, false, false, false, 0, null); */
// TODO: Collapse into one function
function toggleTaskDtStart(event) {
	var checkBox = $('nodtstart');
	var checkBoxTime = $('dtstart_time_assoc');
	var dtstart = document.getElementById("dtstart");
	var dtstart_date = document.getElementById("dtstart_date");
	var dtstart_time = document.getElementById("dtstart_time");
	if (checkBox.checked) {
		dtstart_date.style.visibility = "hidden";
		dtstart_time.style.visibility = "hidden";
	} else {
		if (checkBoxTime.checked) {
			dtstart_time.style.visibility = "visible";
		} else {
			dtstart_time.style.visibility = "hidden";
		}
		dtstart_date.style.visibility = "visible";
		if (dtstart.value.length == 0)
			dtstart.dpck._initCurrentDate();
	}
}
function toggleTaskDue(event) {
	var checkBox = $('nodue');
	var checkBoxTime = $('due_time_assoc');
	var due = document.getElementById("due");
	var due_date = document.getElementById("due_date");
	var due_time = document.getElementById("due_time");
	if (checkBox.checked) {
		due_date.style.visibility = "hidden";
		due_time.style.visibility = "hidden";
	} else {
		if (checkBoxTime.checked) {
			due_time.style.visibility = "visible";
		} else {
			due_time.style.visibility = "hidden";
		}
		due_date.style.visibility = "visible";
		if (due.value.length == 0)
			due.dpck._initCurrentDate();
	}
}
function ToggleTaskDateOrNoDateActivate(event) {
	var dtstart = document.getElementById("nodtstart");
	if (dtstart != null) {
		toggleTaskDtStart(null);
		toggleTaskDue(null);
		$('nodtstart').observe('click', toggleTaskDtStart);
		$('dtstart_time_assoc').observe('click', toggleTaskDtStart);
		$('nodue').observe('click', toggleTaskDue);
		$('due_time_assoc').observe('click', toggleTaskDue);
	} 
}
function TaskViewGatherCategoriesFromTable() {
	var table = $('taskview');
	
}
function attachDatePicker(relative) {
	var dpck = new DatePicker({
	relative: relative,
	      language: 'en', //wclang.substr(0,2),
	      disableFutureDate: false,
	      dateFormat: [ ["yyyy", "mm", "dd"], "-"],
	      showDuration: 0.2
	});
	document.getElementById(relative).dpck = dpck; // attach a ref to it
}
function eventEditAllDay() {
	var allDayCheck = document.getElementById("alldayevent");
	var dtend_time = document.getElementById("dtend_time");
	var dtstart_time = document.getElementById("dtstart_time");
	if(allDayCheck.checked) {
		dtstart_time.style.visibility = "hidden";
		dtend_time.style.visibility = "hidden";
	} else {
		dtstart_time.style.visibility = "visible";
		dtend_time.style.visibility = "visible";
	}
}

// Functions which handle show/hide of various elements in the recurrence editor

function RecurrenceShowHide() {

	if ($('is_recur').checked) {
		$('rrule_div').style.display = 'block';
	}
	else {
		$('rrule_div').style.display = 'none';
	}

	if ($('freq_selector').selectedIndex == 4) {
		$('weekday_selector').style.display = 'block';
	}
	else {
		$('weekday_selector').style.display = 'none';
	}

	if ($('freq_selector').selectedIndex == 5) {
		$('monthday_selector').style.display = 'block';
	}
	else {
		$('monthday_selector').style.display = 'none';
	}

	if ($('rrend_count').checked) {
		$('rrcount').disabled = false;
	}
	else {
		$('rrcount').disabled = true;
	}

	if ($('rrend_until').checked) {
		$('rruntil').disabled = false;
	}
	else {
		$('rruntil').disabled = true;
	}

	if ($('rrmonthtype_mday').checked) {
		$('rrmday').disabled = false;
	}
	else {
		$('rrmday').disabled = true;
	}

	if ($('rrmonthtype_wday').checked) {
		$('rrmweek').disabled = false;
		$('rrmweekday').disabled = false;
	}
	else {
		$('rrmweek').disabled = true;
		$('rrmweekday').disabled = true;
	}

	if ($('freq_selector').selectedIndex == 6) {
		$('yearday_selector').style.display = 'block';
	}
	else {
		$('yearday_selector').style.display = 'none';
	}

	$('ymday').innerHTML = 'XXXX-' + $('dtstart').value.substr(5);
	$('rrmday').innerHTML = $('dtstart').value.substr(8);

	if ($('rryeartype_ywday').checked) {
		$('rrymweek').disabled = false;
		$('rrymweekday').disabled = false;
		$('rrymonth').disabled = false;
	}
	else {
		$('rrymweek').disabled = true;
		$('rrymweekday').disabled = true;
		$('rrymonth').disabled = true;
	}

}


// Enable or disable the 'check attendee availability' button depending on whether
// the attendees list is empty
function EnableOrDisableCheckButton()
{
	if ($('attendees_box').value.length == 0) {
		$('check_button').disabled = true;
	}
	else {
		$('check_button').disabled = false;
	}
}




function launchChat(event) {
window.open('chat', 'ctdl_chat_window', 'toolbar=no,location=no,directories=no,copyhistory=no,status=no,scrollbars=yes,resizable=yes');
}
// logger
function WCLog(msg) {
  if (!!window.console && !!console.log) {
    console.log(msg);
  } else if (!!window.opera && !!opera.postError) {
    opera.postError(msg);
  } else {
    wc_log += msg + "\r\n";
  }
}

function RefreshSMTPqueueDisplay() {
	new Ajax.Updater('smtpqueue_inner_div',
	'display_smtpqueue_inner_div', { method: 'get',
		parameters: Math.random() } );
}

function DeleteSMTPqueueMsg(msgnum1, msgnum2) {
	var p = encodeURI('g_cmd=DELE ' + msgnum1 + ',' + msgnum2);
	new Ajax.Request(
		'ajax_servcmd', {
			method: 'post',
			parameters: p,
			onComplete: RefreshSMTPqueueDisplay()
		}
	);
}


function ConfirmLogoff() {
	new Ajax.Updater(
		'md-content',
		'do_template?template=confirmlogoff',
		{
			method: 'get',
			evalScripts: true,
			onSuccess: function(cl_success) {
				toggleModal(1);
			}
		}
	);
}


function switch_to_lang(new_lang) {
	p = 'push?url=' + encodeURI(window.location);
	new Ajax.Request(p, { method: 'get' } );
	window.location = 'switch_language?lang=' + new_lang ;
}


function toggle_roomlist() 
{
	/* WARNING: VILE, SLEAZY HACK.  We determine the state of the box based on the image loaded. */
	if ( $('expand_roomlist').src.substring($('expand_roomlist').src.length - 12) == "collapse.gif" ) {
		$('roomlist').style.display = 'none';
		$('expand_roomlist').src = 'static/webcit_icons/expand.gif';
		wstate=0;
	}

	else {
		$('roomlist').style.display = 'block';
		$('expand_roomlist').src = 'static/webcit_icons/collapse.gif';
		$('roomlist').innerHTML = '';
		FillRooms(IconBarRoomList);
		wstate=1;
	}

	// tell the server what I did
	p = 'toggle_roomlist_expanded_state?wstate=' + wstate + '?rand=' + Math.random() ;
	new Ajax.Request(p, { method: 'get' } );

	return false;   /* this prevents the click from registering as a roomlist button press */
}


function toggle_wholist() 
{
	/* WARNING: VILE, SLEAZY HACK.  We determine the state of the box based on the image loaded. */
	if ( $('expand_wholist').src.substring($('expand_wholist').src.length - 12) == "collapse.gif" ) {
		$('online_users').style.display = 'none';
		$('expand_wholist').src = 'static/webcit_icons/expand.gif';
		wstate=0;
	}

	else {
		$('online_users').style.display = 'block';
		$('expand_wholist').src = 'static/webcit_icons/collapse.gif';
		activate_iconbar_wholist_populat0r();
		wstate=1;
	}

	// tell the server what I did
	p = 'toggle_wholist_expanded_state?wstate=' + wstate + '?rand=' + Math.random() ;
	new Ajax.Request(p, { method: 'get' } );

	return false;   /* this prevents the click from registering as a wholist button press */
}


