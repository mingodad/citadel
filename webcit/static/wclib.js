//
// $Id: wclib.js,v 625.2 2005/09/18 04:04:32 ajc Exp $
//
// JavaScript function library for WebCit
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
	new Ajax.Autocompleter('cc_id', 'cc_name_choices', '/cc_autocomplete', {} );
	new Ajax.Autocompleter('bcc_id', 'bcc_name_choices', '/bcc_autocomplete', {} );
	new Ajax.Autocompleter('recp_id', 'recp_name_choices', '/recp_autocomplete', {} );
}


// Static variables for mailbox view...
//
var CtdlNumMsgsSelected = 0;
var CtdlMsgsSelected = new Array(100);	// arbitrary

// This gets called when you single click on a message in the mailbox view.
// We know that the element id of the table row will be the letter 'm' plus the message number.
//
function CtdlSingleClickMsg(msgnum) {

	// $('preview_pane').innerHTML = '<div align="center">Loading...</div>' ;

	if (CtdlNumMsgsSelected > 0) {
		for (i=0; i<CtdlNumMsgsSelected; ++i) {
			$('m'+CtdlMsgsSelected[i]).style.backgroundColor = '#fff';
			$('m'+CtdlMsgsSelected[i]).style.color = '#000';
		}
		CtdlNumMsgsSelected = 0;
	}

	$('m'+msgnum).style.backgroundColor='#69aaff';
	$('m'+msgnum).style.color='#fff';
	CtdlNumMsgsSelected = CtdlNumMsgsSelected + 1;
	CtdlMsgsSelected[CtdlNumMsgsSelected-1] = msgnum;

	// Update the preview pane
	new Ajax.Updater('preview_pane', '/msg/'+msgnum, { method: 'get' } );

	// Mark the message as read
	new Ajax.Request(
		'/ajax_servcmd', {
			method: 'post',
			parameters: 'g_cmd=SEEN '+msgnum+'|1',
			onComplete: CtdlRemoveTheUnseenBold(msgnum)
		}
	);
}

function CtdlRemoveTheUnseenBold(msgnum) {
	$('m'+msgnum).style.fontWeight='normal' ;
}

