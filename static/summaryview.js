/** Webcit Summary View v2
    All comments, flowers and death threats to Mathew McBride
    <matt@mcbridematt.dhs.org> / <matt@comalies>
*/
document.observe("dom:loaded", createMessageView);

var message_view = null;
var loadingMsg = null;
var rowArray = null;
var currentSortMode = null;

// Header elements
var mlh_date = null;
var mlh_subject = null;
var mlh_from = null;
var currentSorterToggle = null;
var currentlyMarkedRows = new Object();

var mouseDownEvent = null;
var exitedMouseDown = false;

function createMessageView() {
  message_view = document.getElementById("message_list_body");
  loadingMsg = document.getElementById("loading");
  roomName = getTextContent(document.getElementById("rmname"))
  new Ajax.Request("roommsgs", {
    method: 'get',
	onSuccess: loadMessages,
	parameters: {'room':roomName},
	sanitize: false,
	evalJSON: false
	});
  mlh_date = $("mlh_date");
  mlh_subject = $('mlh_subject');
  mlh_from = $('mlh_from');
  mlh_date.observe('click',ToggleDateSort);
  mlh_subject.observe('click',ToggleSubjectSort);
  mlh_from.observe('click',ToggleFromSort);
  /* This handles our 'context-menu'. Don't feed to IE */
  if (message_view.addEventListener != undefined) {
  $(message_view).observe('mousedown', mouseDownHandler);
  $(message_view).observe('mouseup',mouseUpHandler);
  }
  $(document).observe('keyup',CtdlMessageListKeyUp,false);
  window.oncontextmenu = function() { return false; };  
}
function loadMessages(transport) {
  var msgs = eval(transport.responseText);
  if (!!msgs && transport.responseText.length < 2) {
    alert("Message loading failed");
  }
  var length = msgs.length;
  rowArray = new Array(); // store so they can be sorted
  for(var i=0; i<length;i++) {
    var trElement = document.createElement("tr");
    var data = msgs[i];
    var msgId = data[0];
    var rowId = "msg_" + msgId;
    trElement.setAttribute("id",rowId);
    $(trElement).observe('click', CtdlMessageListClick);
    trElement.ctdlMsgId = msgId;
    for(var j=1; j<4;j++) { // 1=msgId (hidden), 5 = isNew etc. 
      var content = data[j];
      if (j==3) {
       	trElement.ctdlDate = content;
	date = new Date(content*1000);
	content = date.toLocaleString();
      }
      var tdElement = document.createElement("td");
      trElement.appendChild(tdElement);
      setTextContent(tdElement,content);
      var classStmt = "col"+j;
      tdElement.setAttribute("class", classStmt);
    }
    if (data[4]) {
      trElement.setAttribute("class", "new_message");
    }
    rowArray[i] = trElement;
  }
  currentSortMode = sortRowsByDateDescending;
  resortAndDisplay(sortRowsByDateDescending);
  loadingMsg.parentNode.removeChild(loadingMsg);
}
function resortAndDisplay(sortMode) {
  emptyElement(message_view);
  fragment = document.createDocumentFragment();
 rowArray.sort(sortMode);
  for(var x=0; x<rowArray.length; x++) {
    rowArray[x].className = rowArray[x].className.replace("table-alt-row","");
    rowArray[x].className = rowArray[x].className.replace("table-row","");
    if (((x-1) % 2) == 0) {
      rowArray[x].className += rowArray[x].className = " table-alt-row";
    } else {
      rowArray[x].className += " table-row";
    }
    //message_view.appendChild(rowArray[x]);
    fragment.appendChild(rowArray[x]);
  }
  message_view.appendChild(fragment);
}
function sortRowsByDateAscending(a, b) {
  var dateOne = a.ctdlDate;
  var dateTwo = b.ctdlDate;
  return (dateOne - dateTwo);
}
function sortRowsByDateDescending(a, b) {
  var dateOne = a.ctdlDate;
  var dateTwo = b.ctdlDate;
  return (dateTwo - dateOne);
}

function sortRowsBySubjectAscending(a, b) {
  var subjectOne = getTextContent(a.getElementsByTagName("TD")[0]).toLowerCase();
  var subjectTwo = getTextContent(b.getElementsByTagName("TD")[0]).toLowerCase();
  return (subjectOne.charCodeAt(0) - subjectTwo.charCodeAt(0));
}

function sortRowsBySubjectDescending(a, b) {
  var subjectOne = getTextContent(a.getElementsByTagName("TD")[0]).toLowerCase();
  var subjectTwo = getTextContent(b.getElementsByTagName("TD")[0]).toLowerCase();
  return (subjectTwo.charCodeAt(0) - subjectOne.charCodeAt(0));
}

function sortRowsByFromAscending(a, b) {
  var fromOne = getTextContent(a.getElementsByTagName("TD")[1]).toLowerCase();
  var fromTwo = getTextContent(b.getElementsByTagName("TD")[1]).toLowerCase();
  return (fromOne.charCodeAt(0) - fromTwo.charCodeAt(0));
}

function sortRowsByFromDescending(a, b) {
  var fromOne = getTextContent(a.getElementsByTagName("TD")[1]).toLowerCase();
  var fromTwo = getTextContent(b.getElementsByTagName("TD")[1]).toLowerCase();
  return (fromTwo.charCodeAt(0) - fromOne.charCodeAt(0));
}

function CtdlMessageListClick(event) {
  var target = event.target;
  var parentNode = target.parentNode;
  var msgId = parentNode.ctdlMsgId;
  // If the shift key modifier wasn't used, unmark all rows and load the message
  if (!event.shiftKey) {
    unmarkAllRows();
    new Ajax.Updater('preview_pane', 'msg/'+msgId, {method: 'get'});
    markRow(parentNode);
  } else if (event.button != 2) {
    markRow(parentNode);
    var selection = window.getSelection();
    var range = selection.getRangeAt(0);
    var childNodesInRange = range.cloneContents().childNodes;
    for(var x=0; x<childNodesInRange.length; x++) {
      var row = childNodesInRange[x];
      var id = row.id;
      var actualRow = document.getElementById(id);
      if (!actualRow.ctdlMarked) {
	markRow(actualRow);
      }
    }
  }
}

function ToggleDateSort(event) {
  removeOldSortClass();
  currentSorterToggle = mlh_date;
  var sortModeToUse = null;
  if (currentSortMode == sortRowsByDateAscending) {
   sortModeToUse = sortRowsByDateDescending;
 } else {
   sortModeToUse = sortRowsByDateAscending;
 }
  resortAndDisplay(sortModeToUse);
  currentSortMode = sortModeToUse;
  mlh_date.className = mlh_date.className += " current_sort_mode";
}
function ToggleSubjectSort(event) {
  removeOldSortClass();
  currentSorterToggle = mlh_subject;
  var sortModeToUse = null;
  if (currentSortMode == sortRowsBySubjectAscending) {
    sortModeToUse = sortRowsBySubjectDescending;
  } else {
    sortModeToUse = sortRowsBySubjectAscending;
  }
  resortAndDisplay(sortModeToUse);
  currentSortMode = sortModeToUse;
  mlh_subject.className = mlh_subject.className += " current_sort_mode";
}
function ToggleFromSort(event) {
  removeOldSortClass();
  currentSorterToggle = mlh_from;
  var sortModeToUse = null;
  if (currentSortMode == sortRowsByFromAscending) {
    sortModeToUse = sortRowsByFromDescending;
  } else {
    sortModeToUse = sortRowsByFromAscending;
  }
  resortAndDisplay(sortModeToUse);
  currentSortMode = sortModeToUse;
  mlh_from.className = mlh_from.className += " current_sort_mode";
}
function removeOldSortClass() {
  if (currentSorterToggle) {
    currentSorterToggle.className = currentSorterToggle.className.replace("current_sort_mode","");
  }
}
function markRow( row) {
  var msgId = row.ctdlMsgId;
  row.className = row.className += " marked_row";
  row.ctdlMarked = true;
  currentlyMarkedRows[msgId] = row;
}
function unmarkRow(row) {
  var msgId = row.ctdlMsgId;
  row.className = row.className.replace("marked_row","");
  row.ctdlMarked = false;
  delete currentlyMarkedRows[msgId];
}
function unmarkAllRows() {
  for(msgId in currentlyMarkedRows) {
    unmarkRow(currentlyMarkedRows[msgId]);
  }
}

function CtdlMessageListKeyUp(event) {
  var key = event.which;
  if (key == 46) { // DELETE
    for(msgId in currentlyMarkedRows) {
      new Ajax.Request('ajax_servcmd', 
		       {method: 'post',
			   parameters: 'g_cmd=MOVE ' + msgId + '|_TRASH_|0',
			   onComplete: clearMessage(msgId)
			   });
    }
  }
}

function clearMessage(msgId) {
  var row = document.getElementById('msg_'+msgId);
  row.parentNode.removeChild(row);
  delete currentlyMarkedRows[msgId];
}
/* Since right click is hard to capture, implement an alternative method inspired by BillyG's PDA OS.. hold mouse down for context menu */
function mouseDownHandler(event) {
  exitedMouseDown = false;
  mouseDownEvent = event;
  setTimeout('summaryViewContextMenu();', 1000);
}
function mouseUpHandler(event) {
  exitedMouseDown = true;
}
function summaryViewContextMenu() {
  if (!exitedMouseDown) {
    var contextSource = document.getElementById("listViewContextMenu");
    CtdlSpawnContextMenu(mouseDownEvent, contextSource);
  }
}
