//
// $Id$
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

function hide_imsg_popup_old() {
	if (browserType == "gecko" )
		document.poppedLayer = eval('document.getElementById(\'important_message\')');
	else if (browserType == "ie")
		document.poppedLayer = eval('document.all[\'important_message\']');
	else
		document.poppedLayer = eval('document.layers[\'`important_message\']');

	document.poppedLayer.style.visibility = "hidden";
}

function hide_imsg_popup() {
	// new Effect.FadeTo('important_message', 0.0, 1000, 20, {complete:function() { hide_imsg_popup_old(); }} );
	hide_imsg_popup_old();	// Do it the old way for now, to avoid library conflicts
}


// This function activates the ajax-powered recipient autocompleters on the message entry screen.
function activate_entmsg_autocompleters() {
	new Ajax.Autocompleter('cc_id', 'cc_name_choices', '/cc_autocomplete', {} );
	new Ajax.Autocompleter('bcc_id', 'bcc_name_choices', '/bcc_autocomplete', {} );
	new Ajax.Autocompleter('recp_id', 'recp_name_choices', '/recp_autocomplete', {} );
}
