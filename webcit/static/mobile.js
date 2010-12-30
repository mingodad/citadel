/*
 * Copyright 2005 - 2009 The Citadel Team
 * Licensed under the GPL V3
 */

var currentMsgDisplay = null;
function CtdlLoadMsgMouseDown(event, msgnum) {
  /* 	if (currentMsgDisplay != null) {
		currentMsgDisplay.style.display = "none";
	} 
	var id = "m_"+msgnum;
var preview_pane = document.getElementById(id);
preview_pane.style.display = "block";
preview_pane.innerHTML = "<i>Loading message</i>";
currentMsgDisplay = preview_pane;
var req = new XMLHttpRequest();
req.open('GET', '/msg/'+msgnum, true);
req.onreadystatechange = function (aEvt) {
  if (req.readyState == 4) {
     if(req.status == 200)
      currentMsgDisplay.innerHTML = "<button onMouseDown=\"CtdlHideMsg()\">(Hide message)</button><br>"+req.responseText;
     else
      currentMsgDisplay.innerHTML = "Error loading message";
  }
};
req.send(null); */
  window.location = "/mobilemsg/"+msgnum;
}
function CtdlHideMsg() {
	currentMsgDisplay.style.display = "none";
}
