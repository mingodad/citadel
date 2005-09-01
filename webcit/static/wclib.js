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
	new Effect.FadeTo('important_message', 0.0, 1000, 20, {complete:function() { hide_imsg_popup_old(); }} );
	// we still do it the old way afterwards, just in case the browser didn't dazzle us enough
}
