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


// Given a unix timestamp, outputs a date/time using the browser's
// timezone and locale.
function output_datetime(unixtimestamp) {
	var now = new Date();
	now.setTime(unixtimestamp * 1000);
	document.write(now.toLocaleString());
}
