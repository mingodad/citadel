/*jshint strict: false, bitwise: false */
/*global document, window, Ajax, currentlyMarkedRows, Event, event, taskViewActivate, setTimeout, fillRooms, $, ctdlLocalPrefs, currentDropTargets, iconBarRoomList, confirm, Effect */
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

var supportsAddEventListener = (!!document.addEventListener);
var today = new Date();

var wc_log = "";
if (document.all) {browserType = "ie";}
if (window.navigator.userAgent.toLowerCase().match("gecko")) {
	browserType= "gecko";
}
var ns6=document.getElementById&&!document.all;
Event.observe(window, 'load', ToggleTaskDateOrNoDateActivate);
Event.observe(window, 'load', taskViewActivate);
//document.observe("dom:loaded", setupPrefEngine);
document.observe("dom:loaded", setupIconBar);
function ctdlRandomString()  {
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
 
    return ( ( str1 === str2 ) ? 0 : ( ( str1 > str2 ) ? 1 : -1 ) );
}

function ctdlMarkLog(Which, Status)
{
    if (Status) {
	document.getElementById(Which).checked = false;
    }
    else {
	document.getElementById(Which).checked = true;
    }
 
}
function ToggleLogEnable(Which)
{
    var p;
    var element = document.getElementById(Which);
    if (element.hasOwnProperty('checked')) {
	var Status = element.checked;
	if (!Status) {
	    p = encodeURI('g_cmd=LOGS ' + Which + '|0');
	}
	else {
	    p = encodeURI('g_cmd=LOGS ' + Which + '|1');
	}
	new Ajax.Request('ajax_servcmd', {
	    method: 'post',
	    parameters: p,
	    onComplete: ctdlMarkLog(Which, Status)
	});
    }
}

function SMTPRunQueue()
{
    var p;

    p= encodeURI('g_cmd=SMTP runqueue');
    new Ajax.Request('ajax_servcmd', {
	method: 'post',
	parameters: p,
	onComplete: function(transport) { ajax_important_message(transport.responseText.substr(4));}
    });
}

function NetworkSynchronizeRoom(NodeName)
{
    var p;

    p= encodeURI('g_cmd=NSYN ' + NodeName);
    new Ajax.Request('ajax_servcmd', {
	method: 'post',
	parameters: p,
	onComplete: function(transport) { ajax_important_message(transport.responseText.substr(4));}
    });
}
function ToggleVisibility ($Which)
{
    if (document.getElementById)
    {
	if (document.getElementById($Which).style.display  === "none") {
	    document.getElementById($Which).style.display  = "inline";
	}
	else {
	    document.getElementById($Which).style.display  = "none";
	}
    }
}

function emptyElement(element) {
  var childNodes = element.childNodes;
  for(var i=0; i<childNodes.length; i++) {
    try {
    element.removeChild(childNodes[i]);
    } catch (e) {
      wCLog(e+"|"+e.description);
    }
  }
}
// Implements superior internet explorer 'extract all child text from element' feature'.
// Falls back on buggy, patent violating standardized method
function getTextContent(element) {
  if (element.textContent === undefined) {
    return element.innerText;
  }
  return element.textContent;
}
/** Same reasons as above */
function setTextContent(element, textContent) {
  if(element.textContent === undefined) {
    element.innerText = textContent;
  } else {
  element.textContent = textContent;
  }
}

// We love string tokenizers.
function extract_token(source_string, token_num, delimiter) {
    var j, i = 0;
    var extracted_string = source_string;

    if (token_num > 0) {
	for (i=0; i<token_num; ++i) {
	    j = extracted_string.indexOf(delimiter);
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
    if (browserType === "gecko") {
	document.poppedLayer = eval('document.getElementById(\'important_message\')');
    }
    else if (browserType === "ie") {
	document.poppedLayer = eval('document.all[\'important_message\']');
    }
    else {
	document.poppedLayer = eval('document.layers[\'`important_message\']');
    }

    document.poppedLayer.style.visibility = "hidden";
}
function remove_something(what_to_search, new_visibility) {
    if (browserType === "gecko") {
	document.poppedLayer = eval('document.getElementById(\'' + what_to_search + '\')');
    }
    else if (browserType === "ie") {
	document.poppedLayer = eval('document.all[\'' + what_to_search + '\']');
    }
    else {
	document.poppedLayer = eval('document.layers[\'`' + what_to_search + '\']');
    }
    if (document.poppedLayer !== null) {
	document.poppedLayer.innerHTML = "";
    }
}

function unhide_imsg_popup() {
    if (browserType === "gecko") {
	document.poppedLayer = eval('document.getElementById(\'important_message\')');
    }
    else if (browserType === "ie") {
	document.poppedLayer = eval('document.all[\'important_message\']');
    }
    else {
	document.poppedLayer = eval('document.layers[\'`important_message\']');
    }

    document.poppedLayer.style.visibility = "visible";
    setTimeout('hide_imsg_popup()', 5000);
}

function ajax_important_message(messagetext)
{
    if (browserType === "gecko") {
	document.poppedLayer = eval('document.getElementById(\'important_message\')');
    }
    else if (browserType === "ie") {
	document.poppedLayer = eval('document.all[\'important_message\']');
    }
    else {
	document.poppedLayer = eval('document.layers[\'`important_message\']');
    }
    document.poppedLayer.style.visibility = "visible";
    setTimeout('hide_imsg_popup()', 5000);
    document.poppedLayer.innerHTML = messagetext;
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
	if ( $('expand_roomlist').src.substring($('expand_roomlist').src.length - 12) === "collapse.gif" ) {
		$('roomlist').style.display = 'block';
		$('roomlist').innerHTML = '';
		fillRooms(iconBarRoomList);
	}
	else {
		$('roomlist').style.display = 'none';
	}

	/* WARNING: VILE, SLEAZY HACK.  We determine the state of the box based on the image loaded. */
	if ( $('expand_wholist').src.substring($('expand_wholist').src.length - 12) === "collapse.gif" ) {
		$('online_users').style.display = 'block';
		activate_iconbar_wholist_populat0r();
	}
	else {
		$('online_users').style.display = 'none';
	}

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

    var d = $('note-' + uid_of_note_being_dragged);
    d.style.cursor = saved_cursor_style;

    // If any motion actually occurred, submit an ajax http call to record it to the server
    if (note_was_dragged > 0) {
	var p = 'note_uid=' + uid_of_note_being_dragged
	    + '&left=' + d.style.left
	    + '&top=' + d.style.top
	    + '&r=' + ctdlRandomString();
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
var saved_x, saved_y;
function NotesDragMouseMove(evt) {
    var x = (ns6 ? evt.clientX : event.clientX);
    var x_increment = x - saved_x;
    var y = (ns6 ? evt.clientY : event.clientY);
    var y_increment = y - saved_y;

    // Move the div
    var d = $('note-' + uid_of_note_being_dragged);

    var divTop = parseInt(d.style.top);
    var divLeft = parseInt(d.style.left);

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
    var d = $('note-' + uid_of_note_being_dragged);
    saved_cursor_style = d.style.cursor;
    d.style.cursor = 'move';
    return false;		// disable the default action
}


// Called when the user clicks on the palette icon of a sticky note to change its color.
// It toggles the color selector visible or invisible.

function NotesClickPalette(evt, uid) {
    var uid_of_note_being_colored = uid;
    var d = $('palette-' + uid_of_note_being_colored);

    if (d.style.display) {
	if (d.style.display === 'none') {
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
    var uid_of_note_being_colored = uid;
    var palette_button = $('palette-' + uid_of_note_being_colored);
    var note_div = $('note-' + uid_of_note_being_colored);
    var titlebar_div = $('titlebar-' + uid_of_note_being_colored);

    // alert('FIXME red=' + red + ' green=' + green + ' blue=' + blue);

    note_div.style.backgroundColor = notecolor;
    titlebar_div.style.backgroundColor = titlecolor;
    palette_button.style.display = 'none';

    // submit an ajax http call to record it to the server
    var p = 'note_uid=' + uid_of_note_being_colored
	+ '&red=' + red
	+ '&green=' + green
	+ '&blue=' + blue
	+ '&r=' + ctdlRandomString();
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

    var d = $('note-' + uid_of_note_being_resized);
    d.style.cursor = saved_cursor_style;

    // If any motion actually occurred, submit an ajax http call to record it to the server
    if (note_was_resized > 0) {
	var p = 'note_uid=' + uid_of_note_being_resized
	    + '&width=' + d.style.width
	    + '&height=' + d.style.height
	    + '&r=' + ctdlRandomString();
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
    var x = (ns6 ? evt.clientX : event.clientX);
    var x_increment = x - saved_x;
    var y = (ns6 ? evt.clientY : event.clientY);
    var y_increment = y - saved_y;

    // Move the div
    var d = $('note-' + uid_of_note_being_resized);

    var divTop = parseInt(d.style.height);
    var divLeft = parseInt(d.style.width);

    var newHeight = divTop + y_increment;
    if (newHeight < 50) {
	newHeight = 50;
    }

    var newWidth = divLeft + x_increment;
    if (newWidth < 50) {
	newWidth = 50;
    }
    d.style.height = newHeight + 'px';
    d.style.width = newWidth + 'px';

    saved_x = x;
    saved_y = y;
    note_was_resized = 1;
    return false;		// disable the default action
}


function NotesResizeMouseDown(evt, uid) {
    var saved_x = (ns6 ? evt.clientX : event.clientX);
    var saved_y = (ns6 ? evt.clientY : event.clientY);
    document.onmouseup = NotesResizeMouseUp;
    document.onmousemove = NotesResizeMouseMove;
    if (document.layers) {
	document.captureEvents(Event.MOUSEUP | Event.MOUSEMOVE);
    }
    uid_of_note_being_resized = uid;
    var d = $('note-' + uid_of_note_being_resized);
    saved_cursor_style = d.style.cursor;
    d.style.cursor = 'move';
    return false;		// disable the default action
}


function DeleteStickyNote(evt, uid, confirmation_prompt) {
    var uid_of_note_being_deleted = uid;
    var d = $('note-' + uid_of_note_being_deleted);

    if (confirm(confirmation_prompt)) {
	new Effect.Puff(d);

	// submit an ajax http call to delete it on the server
	var p = 'note_uid=' + uid_of_note_being_deleted
	    + '&deletenote=yes'
	    + '&r=' + ctdlRandomString();
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
    if (typeof el === "string") {
	return el;
    }
    if (typeof el === "undefined") {
	return el;
    }
    if (el.innerText) {
	return el.innerText;	//Not needed but it is faster
    }
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
    elem.innerHTML = "<div align=center><br>" + 
	"<table border=0 cellpadding=10 bgcolor=\"#ffffff\">" + 
	" <tr><td><img src=\"static/throbber.gif\" />" + 
	" <font color=\"#AAAAAA\">&nbsp;&nbsp;Loading....</font>" + 
	"</td></tr></table><br></div>";
}



// Pop open the address book (target_input is the INPUT field to populate)

function PopOpenAddressBook(target_input) {
	$('address_book_popup').style.display = 'block';
	p = 'target_input=' + target_input + '&r=' + ctdlRandomString();
	new Ajax.Updater(
		'address_book_popup_middle_div',
		'do_template?template=addressbook_list',
		{
			method: 'get',
			parameters: p,
			evalScripts: true
		}
	);
}

function PopulateAddressBookInnerDiv(which_addr_book, target_input) {

    $('address_book_inner_div').innerHTML =
	"<div align=center><br>" + 
	"<table border=0 cellpadding=10 bgcolor=\"#ffffff\">" + 
	"<tr><td><img src=\"static/throbber.gif\" />" + 
	"<font color=\"#AAAAAA\">" +
	"&nbsp;&nbsp;Loading...." + 
	"</font></td></tr></table><br></div>";

    p = 'which_addr_book=' + which_addr_book
	+ '&target_input=' + target_input
	+ '&r=' + ctdlRandomString()
	+ "&template=addressbook_namelist";
    new Ajax.Updater(
	'address_book_inner_div',
	'do_template',
	{
	    method: 'get',
	    parameters: p
	}
    );
}

// What happens when a contact is selected from the address book popup
// (populate the specified target)

function AddContactsToTarget(target, whichaddr) {
	while (whichaddr.selectedIndex !== -1) {
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
	if (dtstart.value.length === 0) {
	    dtstart.dpck._initCurrentDate();
	}
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
	if (due.value.length === 0) {
	    due.dpck._initCurrentDate();
	}
    }
}
function ToggleTaskDateOrNoDateActivate(event) {
	var dtstart = document.getElementById("nodtstart");
	if (dtstart !== null) {
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

	if ($('freq_selector').selectedIndex === 4) {
		$('weekday_selector').style.display = 'block';
	}
	else {
		$('weekday_selector').style.display = 'none';
	}

	if ($('freq_selector').selectedIndex === 5) {
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

	if ($('freq_selector').selectedIndex === 6) {
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
	if ($('attendees_box').value.length === 0) {
		$('check_button').disabled = true;
	}
	else {
		$('check_button').disabled = false;
	}
}




function launchChat(event) {
    window.open('chat',
		'ctdl_chat_window',
		'toolbar=no,location=no,directories=no,copyhistory=no,status=no,scrollbars=yes,resizable=yes');
}
// logger
function wCLog(msg) {
  if (!!window.console && !!console.log) {
    console.log(msg);
  } else if (!!window.opera && !!opera.postError) {
    opera.postError(msg);
  } else {
    wc_log += msg + "\r\n";
  }
}

function RefreshSMTPqueueDisplay() {
	new Ajax.Updater('mailqueue_list',
	'dotskip?room=__CitadelSMTPspoolout__&view=11&ListOnly=yes', { method: 'get',
		parameters: Math.random() } );
}

function DeleteSMTPqueueMsg(msgnum1, msgnum2) {
    var p = encodeURI('g_cmd=DELE ' + msgnum1 + ',' + msgnum2);
    new Ajax.Request(
	'ajax_servcmd', {
	    method: 'post',
	    parameters: p,
	    onComplete: function(transport) {
		ajax_important_message(transport.responseText.substr(4));
		RefreshSMTPqueueDisplay();}
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
	if ( $('expand_roomlist').src.substring($('expand_roomlist').src.length - 12) === "collapse.gif" ) {
		$('roomlist').style.display = 'none';
		$('expand_roomlist').src = 'static/webcit_icons/expand.gif';
		wstate=0;
	}

	else {
		$('roomlist').style.display = 'block';
		$('expand_roomlist').src = 'static/webcit_icons/collapse.gif';
		$('roomlist').innerHTML = '';
		fillRooms(iconBarRoomList);
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
	if ( $('expand_wholist').src.substring($('expand_wholist').src.length - 12) === "collapse.gif" ) {
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

function getBlogStartText(wikitype) {
    var wikitext = document.getElementById("wikitext").innerHTML;
    var wikipublish = document.getElementById("wikipublish").innerHTML;
    var wikilinktext = document.getElementById("wikilinktext").innerHTML;
    var wikilinkmore = document.getElementById("wikilinkmore").innerHTML;
    var wikilinkembedmedia = document.getElementById("wikilinkembedmedia").innerHTML;
    if (wikitype) {
	return "<html><head></head><body>\n" +
	    "<h1>" + wikitext + "</h1>\n" + 
	    "<p>" + wikipublish + "</p>\n" +
	    "<a href='wiki?page=firstarticle'>" + wikilinktext + "</a>" +
	    "<p>" + wikilinkmore + "</p>\n" + 
	    "<p>" + wikilinkembedmedia + " </p>\n<p><img src='/download_file/test.jpg' alt=\"alttext\"></p>\n" + 
	    "</body></html>";
    }
    else {
	return "#" + wikitext + "\n" + 
	    wikipublish + "\n\n" + 
	    "[" + wikilinktext + "](wiki?page=firstarticle)\n\n" +
	    wikilinkmore + "\n\n" + 
	    wikilinkembedmedia + "\n\n ![alttext](/download_file/test.jpg)";
    }
}

function create_blog()
{
    var er_view_blog = document.getElementById('er_view_blog');
    var Nonce = document.getElementById('Nonce');
    var roomname = document.getElementById('er_name').value;
    var editroomname = roomname + '\\edit';
    var filePath = "files_" + roomname;
    var floorID = document.getElementById('er_floor').value;

    var selects = document.getElementById("er_floor");
    var selectedFloor = selects.options[selects.selectedIndex].value;
    var selectedFloorName = selects.options[selects.selectedIndex].text;

    var vselects = document.getElementById("er_view");
    var vselectedMarkup = vselects.options[vselects.selectedIndex].value;
    var vselectedMarkupName = vselects.options[vselects.selectedIndex].text;

    var adminPW = document.getElementById('adminlist_passworded').checked;

    var passvoid = document.getElementById('er_password').value;
    var roomtypeWiki = document.getElementById('er_blog_markup_html').value;

    var isHtmlWiki = vselectedMarkup === roomtypeWiki;
    var starttext = getBlogStartText(isHtmlWiki);

    ToggleVisibility('er_password');
    var type_edit;
    if (adminPW) {
	type_edit = document.getElementById('adminlist_passworded').value;
	ToggleVisibility('li_adminlist_invonly');
	ToggleVisibility('adminlist_passworded');
	
    }
    else {
	type_edit = document.getElementById('adminlist_invonly').value;
	ToggleVisibility('adminlist_invonly');
	ToggleVisibility('li_adminlist_passworded');
    }

    ToggleVisibility('er_floor');
    document.getElementById('er_floor_fixed').innerHTML = selectedFloorName;
    ToggleVisibility('er_floor_fixed');

    ToggleVisibility('er_name');
    document.getElementById('er_name_fixed').innerHTML = roomname;
    ToggleVisibility('er_name_fixed');

    ToggleVisibility('er_view');
    document.getElementById('er_view_fixed').innerHTML = vselectedMarkupName;
    ToggleVisibility('er_view_fixed');

    ToggleVisibility('create_buttons');

    ToggleVisibility('edit_info');
    ToggleVisibility('throbber');
    

    var roomdata = {
	create_blog_room: {
	    nonce:       Nonce,
	    er_name:     roomname,
	    type:        'public',
	    er_view:     er_view_blog,
	    er_floor:    floorID,
	    template:    "room_result_json",
	    ok_button:   1
	},
	setflags_blog_room: {
	    nonce:       Nonce,
	    er_name:     roomname,
	    go:          roomname,
	    type:        'public',
	    er_floor:    floorID,

	    directory:   "yes",
	    er_dirname:  filePath,
	    ulallowed:   "no",
	    dlallowed:   "yes",
	    ulmsg:       "no",
	    visdir:      "no",

	    anon:        "no",
	    last_tabsel: 1,
	    er_view:     er_view_blog,
	    template:    "room_result_json",
	    ok_button:   1
	},
	create_blog_edit_room: {
	    nonce:       Nonce,
	    er_name:     editroomname,
	    type:        type_edit,
	    er_view:     vselectedMarkup,
	    er_floor:    floorID,
	    er_password: passvoid,
	    template:    "room_result_json",
	    ok_button:   1
	},
	setflags_blog_edit_room: {
	    nonce:       Nonce,
	    er_name:     editroomname,
	    go:          editroomname,
	    type:        type_edit,
	    er_floor:    floorID,

	    directory:   "yes",
	    er_dirname:  filePath,
	    ulallowed:   "yes",
	    dlallowed:   "yes",
	    ulmsg:       "no",
	    visdir:      "yes",

	    anon:        "no",
	    last_tabsel: 1,
	    er_view:     er_view_blog,
	    template:    "room_result_json",
	    ok_button:   1
	},
	blog_wiki_startmessage : {
	    nonce:       Nonce,
	    force_room:  editroomname,
	    page:        "home",
	    markdown:    (isHtmlWiki)?0:1,
	    msgtext:     starttext

	}
    };

    /* promises anyone?
     *  - create the blog room
     *  - set the blog rooms file flags
     *  - create the edit room
     *  - set the blog edit room flags
     */
    new Ajax.Request("/entroom",
		     { method: 'post',
		       parameters: $H(roomdata.create_blog_room).toQueryString(),
		       onComplete: function(transport) {
			   new Ajax.Request("/editroom",
					    { method: 'post',
					      parameters: $H(roomdata.setflags_blog_room).toQueryString(),
					      onComplete: function(transport) {
						  new Ajax.Request("/entroom",
								   { method: 'post',
								     parameters: $H(roomdata.create_blog_edit_room).toQueryString(),
								     onComplete: function(transport) {
									 new Ajax.Request("/editroom",
											  { method: 'post',
											    parameters: $H(roomdata.setflags_blog_edit_room).toQueryString(),
											    onComplete: function(transport) {
												ToggleVisibility('throbber');
												new Ajax.Request("/post",
														 { method: 'post',
														   parameters: $H(roomdata.blog_wiki_startmessage).toQueryString(),
														   onComplete: function(transport) {

														   }
														 }
														);
											    }
											  }
											 );
								     }
								   }
								  );
					      }
					    }
					   )
		       }
		     }
		    );

    return false;
}


function deleteAllSelectedMessages() {

}


function publishMessage()
{
    var messages = document.getElementsByClassName("message");
    var messageIdParts = messages[0].id.split('|');
    var editRoomName = getTextContent(document.getElementById("rmname"));
    var roomName = editRoomName.substring(0, editRoomName.length - 5);

    var publish = {
	editRoom: editRoomName,
	blogRoom: roomName,
	msgNo : messageIdParts[1],
	msgIdStr : messageIdParts[2]
    }

    mvCommand = encodeURI("g_cmd=MOVE " + publish.msgNo + "|" + publish.blogRoom + "|1");
    
    new Ajax.Request("ajax_servcmd", {
	parameters: mvCommand,
	method: 'post',
	onSuccess: function(transport) {
	    WCLog(transport.responseText);
	}
    });
}