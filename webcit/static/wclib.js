//
// $Id: wclib.js,v 625.2 2005/09/18 04:04:32 ajc Exp $
//
// JavaScript function library for WebCit.
//
//


var browserType;

if (document.layers) {browserType = "nn4"}
if (document.all) {browserType = "ie"}
if (window.navigator.userAgent.toLowerCase().match("gecko")) {
	browserType= "gecko"
}

var ns6=document.getElementById&&!document.all;


//
// This code handles the popups for instant messages.
//


function hide_page_popup() {
	if (browserType == "gecko" )
		document.poppedLayer = eval('document.getElementById(\'page_popup\')');
	else if (browserType == "ie")
		document.poppedLayer = eval('document.all[\'page_popup\']');
	else
		document.poppedLayer = eval('document.layers[\'`page_popup\']');

	document.poppedLayer.style.visibility = "hidden";
}

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


// Static variables for mailbox view...
//
var CtdlNumMsgsSelected = 0;
var CtdlMsgsSelected = new Array();

// This gets called when you single click on a message in the mailbox view.
// We know that the element id of the table row will be the letter 'm' plus the message number.
//
function CtdlSingleClickMsg(evt, msgnum) {

	// Clear the preview pane until we load the new message
	$('preview_pane').innerHTML = '';

	// De-select any messages that were already selected, *unless* the Ctrl key
	// is being pressed, in which case the user wants multi select.
	if (!evt.ctrlKey) {
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
			}
		}
	}

	// Now select (or de-select) the message
	if ( (evt.ctrlKey) && (already_selected == 1) ) {
		$('m'+msgnum).style.backgroundColor = '#fff';
		$('m'+msgnum).style.color = '#000';
	}
	else {
		$('m'+msgnum).style.backgroundColor='#69aaff';
		$('m'+msgnum).style.color='#fff';
		CtdlNumMsgsSelected = CtdlNumMsgsSelected + 1;
		CtdlMsgsSelected[CtdlNumMsgsSelected-1] = msgnum;
	}

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

	return false;		// try to defeat the default click behavior
}

// Delete selected messages.
function CtdlDeleteSelectedMessages(evt) {
	
	if (CtdlNumMsgsSelected < 1) {
		// Nothing to delete, so exit silently.
		return false;
	}
	for (i=0; i<CtdlNumMsgsSelected; ++i) {
		new Ajax.Request(
			'ajax_servcmd', {
				method: 'post',
				parameters: 'g_cmd=MOVE ' + CtdlMsgsSelected[i] + '|_TRASH_|0',
				onComplete: CtdlClearDeletedMsg(CtdlMsgsSelected[i])
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
// (IE barfs on m9999.innerHTML='' so we use a script.aculo.us effect instead.)
function CtdlClearDeletedMsg(msgnum) {
	new Effect.Squish('m'+msgnum);
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

	// Did we release the mouse button while hovering over a drop target?
	// NOTE: this only works cross-browser because the iconbar div is always
	//	positioned at 0,0.  Browsers differ in whether the 'offset'
	//	functions return pos relative to the document or parent.

	x = (ns6 ? evt.clientX : event.clientX);
	y = (ns6 ? evt.clientY : event.clientY);

	l = parseInt($('dropstuff').offsetLeft);
	t = parseInt($('dropstuff').offsetTop);
	r = parseInt($('dropstuff').offsetLeft) + parseInt($('dropstuff').offsetWidth);
	b = parseInt($('dropstuff').offsetTop) + parseInt($('dropstuff').offsetHeight);

	// alert('Offsets are: ' + l + ' ' + t + ' ' + r + ' ' + b + '.');

	if ( (x >= l) && (x <= r) && (y >= t) && (y <= b) ) {
		// Yes, we dropped it on a hotspot.  Just delete for now... FIXME
		CtdlDeleteSelectedMessages(evt);
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


// icon bar toggler tabs...

function switch_to_room_list() {
	new Ajax.Updater('iconbar', 'iconbar_ajax_rooms', { method: 'get' } );
	fuckitup();
}

function switch_to_menu_buttons() {
	new Ajax.Updater('iconbar', 'iconbar_ajax_menu', { method: 'get' } );
}
