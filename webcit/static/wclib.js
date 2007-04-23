//
// $Id: wclib.js,v 625.2 2005/09/18 04:04:32 ajc Exp $
//
// JavaScript function library for WebCit.
//
//


var browserType;
var room_is_trash = 0;

if (document.layers) {browserType = "nn4"}
if (document.all) {browserType = "ie"}
if (window.navigator.userAgent.toLowerCase().match("gecko")) {
	browserType= "gecko"
}

var ns6=document.getElementById&&!document.all;


function CtdlRandomString()  {
	return((Math.random()+'').substr(3));
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



// This code handles the popups for important-messages.
function hide_imsg_popup() {
	if (browserType == "gecko" )
		document.poppedLayer = eval('document.getElementById(\'important_message\')');
	else if (browserType == "ie")
		document.poppedLayer = eval('document.all[\'important_message\']');
	else
		document.poppedLayer = eval('document.layers[\'`important_message\']');

	document.poppedLayer.style.visibility = "hidden";
}


// This function activates the ajax-powered recipient autocompleters on the message entry screen.
function activate_entmsg_autocompleters() {
	new Ajax.Autocompleter('cc_id', 'cc_name_choices', 'cc_autocomplete', {} );
	new Ajax.Autocompleter('bcc_id', 'bcc_name_choices', 'bcc_autocomplete', {} );
	new Ajax.Autocompleter('recp_id', 'recp_name_choices', 'recp_autocomplete', {} );
}



// Toggle the icon bar between menu/roomlist...
var which_div_expanded = null;
var num_drop_targets = 0;
var drop_targets_elements = new Array();
var drop_targets_roomnames = new Array();

function switch_to_room_list() {
	$('iconbar').innerHTML = $('iconbar').innerHTML.substr(0, $('iconbar').innerHTML.indexOf('switch'));
	CtdlLoadScreen('iconbar');
	new Ajax.Updater('iconbar', 'iconbar_ajax_rooms', { method: 'get' } );
}

function expand_floor(floor_div) {
	if (which_div_expanded != null) {
		if ($(which_div_expanded) != null) {
			$(which_div_expanded).style.display = 'none' ;
		}
	}

	// clicking on the already-expanded floor causes the whole list to collapse
	if (which_div_expanded == floor_div) {
		which_div_expanded = null;

		// notify the server that no floors are expanded
		new Ajax.Request(
			'set_floordiv_expanded/-1', {
				method: 'post'
			}
		);
		return true;
	}

	// expand the requested floor
	$(floor_div).style.display = 'block';
	which_div_expanded = floor_div;

	// notify the server of which floor is expanded
	new Ajax.Request(
		'set_floordiv_expanded/'+floor_div, {
			method: 'post'
		}
	);
}

function switch_to_menu_buttons() {
	which_div_expanded = null;
	num_drop_targets = 0;
	CtdlLoadScreen('iconbar');
	new Ajax.Updater('iconbar', 'iconbar_ajax_menu', { method: 'get' } );
}


// Static variables for mailbox view...
//
var CtdlNumMsgsSelected = 0;
var CtdlMsgsSelected = new Array();
var CtdlLastMsgnumSelected = 0;

// This gets called when you single click on a message in the mailbox view.
// We know that the element id of the table row will be the letter 'm' plus the message number.
//
function CtdlSingleClickMsg(evt, msgnum) {

	// Clear the preview pane until we load the new message
	$('preview_pane').innerHTML = '';

	// De-select any messages that were already selected, *unless* the Ctrl or
	// Shift key is being pressed, in which case the user wants multi select
	// or group select.
	if ( (!evt.ctrlKey) && (!evt.shiftKey) ) {
		if (CtdlNumMsgsSelected > 0) {
			for (i=0; i<CtdlNumMsgsSelected; ++i) {
				$('m'+CtdlMsgsSelected[i]).style.backgroundColor = '#fff';
				$('m'+CtdlMsgsSelected[i]).style.color = '#000';
			}
			CtdlNumMsgsSelected = 0;
		}
	}

	// For multi select ... is the message being clicked already selected?
	already_selected = 0;
	if ( (evt.ctrlKey) && (CtdlNumMsgsSelected > 0) ) {
		for (i=0; i<CtdlNumMsgsSelected; ++i) {
			if (CtdlMsgsSelected[i] == msgnum) {
				already_selected = 1;
				already_selected_pos = i;
			}
		}
	}

	// Now select (or de-select) the message
	if ( (evt.ctrlKey) && (already_selected == 1) ) {

		// Deselect: first un-highlight it...
		$('m'+msgnum).style.backgroundColor = '#fff';
		$('m'+msgnum).style.color = '#000';

		// Then remove it from the selected messages list.
		for (i=already_selected_pos; i<(CtdlNumMsgsSelected-1); ++i) {
			CtdlMsgsSelected[i] = CtdlMsgsSelected[i+1];
		}
		CtdlNumMsgsSelected = CtdlNumMsgsSelected - 1;
		
	}

	else if (evt.shiftKey) {
		
		// Group select: first clear everything out...
		if (CtdlNumMsgsSelected > 0) {
			for (i=0; i<CtdlNumMsgsSelected; ++i) {
				$('m'+CtdlMsgsSelected[i]).style.backgroundColor = '#fff';
				$('m'+CtdlMsgsSelected[i]).style.color = '#000';
			}
		}
		CtdlNumMsgsSelected = 0;

		// Then highlight and select the group.
		// Traverse the table looking for a row whose ID contains the desired msgnum

		var in_the_group = 0;
		var is_edge = 0;
		var table = $('summary_headers');
		if (table) {
			for (var r = 0; r < table.rows.length; r++) {
				var thename = table.rows[r].id;
				if ( (thename.substr(1) == msgnum) || (thename.substr(1) == CtdlLastMsgnumSelected) ) {
					in_the_group = 1 - in_the_group;
					is_edge = 1;
				}
				else {
					is_edge = 0;
				}
				if ( (in_the_group == 1) || (is_edge == 1) ) {
					// Highlight it...
					table.rows[r].style.backgroundColor='#69aaff';
					table.rows[r].style.color='#fff';

					// And add it to the selected messages list.
					CtdlNumMsgsSelected = CtdlNumMsgsSelected + 1;
					CtdlMsgsSelected[CtdlNumMsgsSelected-1] = thename.substr(1);
				}
			}
		}
	}

	else {
		// Select: first highlight it...
		$('m'+msgnum).style.backgroundColor='#69aaff';
		$('m'+msgnum).style.color='#fff';

		// Then add it to the selected messages list.
		CtdlNumMsgsSelected = CtdlNumMsgsSelected + 1;
		CtdlMsgsSelected[CtdlNumMsgsSelected-1] = msgnum;

		// Gradient
		CtdlLoadScreen('preview_pane');
		// Update the preview pane
		new Ajax.Updater('preview_pane', 'msg/'+msgnum, { method: 'get' } );
	
		// Mark the message as read
		new Ajax.Request(
			'ajax_servcmd', {
				method: 'post',
				parameters: 'g_cmd=SEEN '+msgnum+'|1',
				onComplete: CtdlRemoveTheUnseenBold(msgnum)
			}
		);
	}
	
	// Save the selected position in case the user does a group select next time.
	CtdlLastMsgnumSelected = msgnum;

	return false;		// try to defeat the default click behavior
}

// Delete selected messages.
function CtdlDeleteSelectedMessages(evt) {
	
	if (CtdlNumMsgsSelected < 1) {
		// Nothing to delete, so exit silently.
		return false;
	}
	for (i=0; i<CtdlNumMsgsSelected; ++i) {
		if (parseInt(room_is_trash) > 0) {
			new Ajax.Request(
				'ajax_servcmd', {
					method: 'post',
					parameters: 'g_cmd=DELE ' + CtdlMsgsSelected[i],
					onComplete: CtdlClearDeletedMsg(CtdlMsgsSelected[i])
				}
			);
		}
		else {
			new Ajax.Request(
				'ajax_servcmd', {
					method: 'post',
					parameters: 'g_cmd=MOVE ' + CtdlMsgsSelected[i] + '|_TRASH_|0',
					onComplete: CtdlClearDeletedMsg(CtdlMsgsSelected[i])
				}
			);
		}
	}
	CtdlNumMsgsSelected = 0;

	// Clear the preview pane too.
	$('preview_pane').innerHTML = '';
}


// Move selected messages.
function CtdlMoveSelectedMessages(evt, target_roomname) {
	
	if (CtdlNumMsgsSelected < 1) {
		// Nothing to delete, so exit silently.
		return false;
	}
	for (i=0; i<CtdlNumMsgsSelected; ++i) {
		new Ajax.Request(
			'ajax_servcmd', {
				method:'post',
				parameters:'g_cmd=MOVE ' + CtdlMsgsSelected[i] + '|' + target_roomname + '|0',
				onComplete:CtdlClearDeletedMsg(CtdlMsgsSelected[i])
			}
		);
	}
	CtdlNumMsgsSelected = 0;

	// Clear the preview pane too.
	$('preview_pane').innerHTML = '';
}



// This gets called when the user touches the keyboard after selecting messages...
function CtdlMsgListKeyPress(evt) {
	if(document.all) {				// aIEeee
		var whichKey = window.event.keyCode;
	}
	else {						// non-sux0r browsers
		var whichKey = evt.which;
	}
	if (whichKey == 46) {				// DELETE key
		CtdlDeleteSelectedMessages(evt);
	}
	return true;
}

// Take the boldface away from a message to indicate that it has been seen.
function CtdlRemoveTheUnseenBold(msgnum) {
	$('m'+msgnum).style.fontWeight='normal';
}

// A message has been deleted, so yank it from the list.
function CtdlClearDeletedMsg(msgnum) {


	// Traverse the table looking for a row whose ID contains the desired msgnum
	var table = $('summary_headers');
	if (table) {
		for (var r = 0; r < table.rows.length; r++) {
			var thename = table.rows[r].id;
			if (thename.substr(1) == msgnum) {
				try {
					table.deleteRow(r);
				}
				catch(e) {
					alert('error: browser failed to clear row ' + r);
				}
			}
		}
	}
	else {						// if we can't delete it,
		new Effect.Squish('m'+msgnum);		// just hide it.
	}


}

// These functions called when the user down-clicks on the message list resizer bar

var saved_x = 0;
var saved_y = 0;

function CtdlResizeMsgListMouseUp(evt) {
	document.onmouseup = null;
	document.onmousemove = null;
	if (document.layers) {
		document.releaseEvents(Event.MOUSEUP | Event.MOUSEMOVE);
	}
	return true;
}

function CtdlResizeMsgListMouseMove(evt) {
	y = (ns6 ? evt.clientY : event.clientY);
	increment = y - saved_y;

	// First move the bottom of the message list...
	d = $('message_list');
	if (d.offsetHeight){
		divHeight = d.offsetHeight;
	}
	else if (d.style.pixelHeight) {
		divHeight = d.style.pixelHeight;
	}
	d.style.height = (divHeight + increment) + 'px';

	// Then move the top of the preview pane...
	d = $('preview_pane');
	if (d.offsetTop){
		divTop = d.offsetTop;
	}
	else if (d.style.pixelTop) {
		divTop = d.style.pixelTop;
	}
	d.style.top = (divTop + increment) + 'px';

	// Resize the bottom of the preview pane...
	d = $('preview_pane');
	if (d.offsetHeight){
		divHeight = d.offsetHeight;
	}
	else if (d.style.pixelHeight) {
		divHeight = d.style.pixelHeight;
	}
	d.style.height = (divHeight - increment) + 'px';

	// Then move the top of the slider bar.
	d = $('resize_msglist');
	if (d.offsetTop){
		divTop = d.offsetTop;
	}
	else if (d.style.pixelTop) {
		divTop = d.style.pixelTop;
	}
	d.style.top = (divTop + increment) + 'px';

	saved_y = y;
	return true;
}

function CtdlResizeMsgListMouseDown(evt) {
	saved_y = (ns6 ? evt.clientY : event.clientY);
	document.onmouseup = CtdlResizeMsgListMouseUp;
	document.onmousemove = CtdlResizeMsgListMouseMove;
	if (document.layers) {
		document.captureEvents(Event.MOUSEUP | Event.MOUSEMOVE);
	}
	return false;		// disable the default action
}



// These functions handle drag and drop message moving

var mm_div = null;

function CtdlMoveMsgMouseDown(evt, msgnum) {

	// do the highlight first
	CtdlSingleClickMsg(evt, msgnum);

	// Now handle the possibility of dragging
	saved_x = (ns6 ? evt.clientX : event.clientX);
	saved_y = (ns6 ? evt.clientY : event.clientY);
	document.onmouseup = CtdlMoveMsgMouseUp;
	document.onmousemove = CtdlMoveMsgMouseMove;
	if (document.layers) {
		document.captureEvents(Event.MOUSEUP | Event.MOUSEMOVE);
	}

	return false;
}

function CtdlMoveMsgMouseMove(evt) {
	x = (ns6 ? evt.clientX : event.clientX);
	y = (ns6 ? evt.clientY : event.clientY);

	if ( (x == saved_x) && (y == saved_y) ) {
		return true;
	}

	if (CtdlNumMsgsSelected < 1) { 
		return true;
	}

	if (!mm_div) {


		drag_o_text = "<div style=\"overflow:none; background-color:#fff; color:#000; border: 1px solid black; filter:alpha(opacity=75); -moz-opacity:.75; opacity:.75;\"><tr><td>";
		for (i=0; i<CtdlNumMsgsSelected; ++i) {
			drag_o_text = drag_o_text + 
				ctdl_ts_getInnerText(
					$('m'+CtdlMsgsSelected[i]).cells[0]
				) + '<br>';
		}
		drag_o_text = drag_o_text + "<div>";

		mm_div = document.createElement("DIV");
		mm_div.style.position='absolute';
		mm_div.style.top = y + 'px';
		mm_div.style.left = x + 'px';
		mm_div.style.pixelHeight = '300';
		mm_div.style.pixelWidth = '300';
		mm_div.innerHTML = drag_o_text;
		document.body.appendChild(mm_div);
	}
	else {
		mm_div.style.top = y + 'px';
		mm_div.style.left = x + 'px';
	}

	return false;	// prevent the default mouse action from happening?
}

function CtdlMoveMsgMouseUp(evt) {
	document.onmouseup = null;
	document.onmousemove = null;
	if (document.layers) {
		document.releaseEvents(Event.MOUSEUP | Event.MOUSEMOVE);
	}

	if (mm_div) {
		document.body.removeChild(mm_div);	
		mm_div = null;
	}

	if (num_drop_targets < 1) {	// nowhere to drop
		return true;
	}

	// Did we release the mouse button while hovering over a drop target?
	// NOTE: this only works cross-browser because the iconbar div is always
	//	positioned at 0,0.  Browsers differ in whether the 'offset'
	//	functions return pos relative to the document or parent.

	for (i=0; i<num_drop_targets; ++i) {

		x = (ns6 ? evt.clientX : event.clientX);
		y = (ns6 ? evt.clientY : event.clientY);

		l = parseInt(drop_targets_elements[i].offsetLeft);
		t = parseInt(drop_targets_elements[i].offsetTop);
		r = parseInt(drop_targets_elements[i].offsetLeft)
		  + parseInt(drop_targets_elements[i].offsetWidth);
		b = parseInt(drop_targets_elements[i].offsetTop)
		  + parseInt(drop_targets_elements[i].offsetHeight);

		/* alert('Offsets are: ' + l + ' ' + t + ' ' + r + ' ' + b + '.'); */
	
		if ( (x >= l) && (x <= r) && (y >= t) && (y <= b) ) {
			// Yes, we dropped it on a hotspot.
			CtdlMoveSelectedMessages(evt, drop_targets_roomnames[i]);
			return true;
		}
	}

	return true;
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



// This function handles the creation of new notes in the "Notes" view.
//
function add_new_note() {

	new_eid = CtdlRandomString();

	$('new_notes_here').innerHTML = $('new_notes_here').innerHTML
		+ '<IMG ALIGN=MIDDLE src=\"static/storenotes_48x.gif\">'
		+ '<span id=\"note' + new_eid + '\">' + Date() + '</span><br />'
	;

	new Ajax.InPlaceEditor('note' + new_eid,
		'updatenote?eid=' + new_eid , {rows:5,cols:72});
}

function CtdlShowRaw(msgnum) {
var customnav = document.createElement("span");
var mode_citadel = document.createElement("a");
mode_citadel.appendChild(document.createTextNode("Citadel Source"));
var mode_rfc822 = document.createElement("a");
mode_rfc822.appendChild(document.createTextNode(" RFC822 Source"));
mode_citadel.setAttribute("href","#");
mode_rfc822.setAttribute("href","#");
mode_rfc822.setAttribute("onclick","rawSwitch822('" + msgnum + "');");
mode_citadel.setAttribute("onclick","rawSwitchCitadel('" + msgnum + "');");
customnav.appendChild(mode_citadel);
customnav.appendChild(mode_rfc822);
customnav.setAttribute("class","floatcustomnav");
floatwindow("headerscreen","pre",customnav);
rawSwitch822(msgnum);
}

function rawSwitch822(msgnum) {
CtdlLoadScreen("headerscreen");
new Ajax.Updater("headerscreen", 
'ajax_servcmd_esc',
 { method: 'post',parameters: 'g_cmd=MSG2 ' +msgnum  } );

}

function rawSwitchCitadel(msgnum) {
CtdlLoadScreen("headerscreen");
new Ajax.Updater("headerscreen", 
'ajax_servcmd_esc',
 { method: 'post',parameters: 'g_cmd=MSG0 ' +msgnum  } );

}

function floatwindow(newdivid,contentelementtype,customnav) {
var windiv = document.createElement("div");
windiv.setAttribute("class","floatwindow");
var winid = newdivid+"_window";
windiv.setAttribute("id",winid);
var nav = document.createElement("div");
if (customnav != null) {
nav.appendChild(customnav);
}
var minimizeA = document.createElement("a");
var minimizeButton = document.createTextNode("Close");
minimizeA.appendChild(minimizeButton);
minimizeA.setAttribute("onclick","killFloatWindow(this);");
minimizeA.setAttribute("href","#");
nav.appendChild(minimizeA);
nav.setAttribute("class","floatnav");
windiv.appendChild(nav);
var contentarea = document.createElement("pre");
contentarea.setAttribute("class","floatcontent");
contentarea.setAttribute("id",newdivid);
windiv.appendChild(contentarea);
document.body.appendChild(windiv);
}
function killFloatWindow(caller) {
var span = caller.parentNode;
var fwindow = span.parentNode;
fwindow.parentNode.removeChild(fwindow);
}
// Place a gradient loadscreen on an element, e.g to use before Ajax.updater
function CtdlLoadScreen(elementid) {
var elem = document.getElementById(elementid);
elem.innerHTML = "<div align=center><br><table border=0 cellpadding=10 bgcolor=\"#ffffff\"><tr><td><img src=\"static/throbber.gif\" /><font color=\"#AAAAAA\">&nbsp;&nbsp;Loading....</font></td></tr></table><br /></div>";
}


// Show info for a user, basically replaces showuser()
// matt@comalies is to blame for this poorly coded masterpiece. 
function CtdlShowUserInfoPopup(Element) {
	try {
		// hopefully no one needs to use the class attribute... could be better done 
		// with xmlns though..
		var user = Element.getAttribute("class");
		var updname = "biospace_"+user;
		if (document.getElementById(updname) == null) {
			// insert a space for the bio
			var pNode = Element.parentNode;
			var newdiv = document.createElement("div");
			newdiv.id = updname;
			newdiv.innerHTML = "Getting user info....";
			pNode.appendChild(newdiv);
			CtdlLoadScreen(updname);
			new Ajax.Updater(updname, 'showuser_ajax?who='+user, { method: 'get' } );
		}
	}
	catch(err) {
		return true;
	}
	return false;
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
	Nifty('div#address_book_popup_container_div','big transparent');
}

function PopulateAddressBookInnerDiv(which_addr_book, target_input) {
	$('address_book_inner_div').innerHTML = "<div align=center><br><table border=0 cellpadding=10 bgcolor=\"#ffffff\"><tr><td><img src=\"static/throbber.gif\" /><font color=\"#AAAAAA\">&nbsp;&nbsp;Loading....</font></td></tr></table><br /></div>";
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
