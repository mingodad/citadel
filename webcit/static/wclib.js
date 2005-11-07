//
// $Id: wclib.js,v 625.2 2005/09/18 04:04:32 ajc Exp $
//
// JavaScript function library for WebCit.
//
//


//
// This code handles the popups for instant messages.
//

var browserType;

if (document.layers) {browserType = "nn4"}
if (document.all) {browserType = "ie"}
if (window.navigator.userAgent.toLowerCase().match("gecko")) {
	browserType= "gecko"
}

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
var CtdlMsgsSelected = new Array(65536);	// arbitrary

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

