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
var query = "";
var currentlyMarkedRows = new Object();

var mouseDownEvent = null;
var exitedMouseDown = false;

var sortModes = {
  "rdate" : sortRowsByDateDescending,
  "date" : sortRowsByDateAscending,
  //  "reverse" : sortRowsByDateDescending,
  "subj" : sortRowsBySubjectAscending,
  "rsubj" : sortRowsBySubjectDescending,
  "sender": sortRowsByFromAscending,
  "rsender" : sortRowsByFromDescending
};
var toggles = {};
window.console = window.console || {};
var opera = opera || null;
if (opera && opera.postError) {
  console.log = opera.postError;
}
var nummsgs = 0;
var startmsg = 0;
function createMessageView() {
  message_view = document.getElementById("message_list_body");
  loadingMsg = document.getElementById("loading");
  getMessages();
  mlh_date = $("mlh_date");
  mlh_subject = $('mlh_subject');
  mlh_from = $('mlh_from');
  toggles["rdate"] = mlh_date;
  toggles["date"] = mlh_date;
  // toggles["reverse"] = mlh_date;
  toggles["subj"] = mlh_subject;
  toggles["rsubj"] = mlh_subject;
  toggles["sender"] = mlh_from;
  toggles["rsender"] = mlh_from;
  mlh_date.observe('click',ApplySort);
  mlh_subject.observe('click',ApplySort);
  mlh_from.observe('click',ApplySort);
  $(document).observe('keyup',CtdlMessageListKeyUp,false);
  window.oncontextmenu = function() { return false; };  
  $('resize_msglist').observe('mousedown', CtdlResizeMouseDown);
  $('m_refresh').observe('click', getMessages);
  document.getElementById('m_refresh').setAttribute("href","#");
  Event.observe(document.onresize ? document : window, "resize", normalizeHeaderTable);
  sizePreviewPane();
  Event.observe(document.onresize ? document : window, "resize", sizePreviewPane);
  $('summpage').observe('change', getPage);
  takeOverSearchOMatic();
  setupDragDrop(); // here for now
}
function getMessages() {
  if (loadingMsg.parentNode == null) {
    message_view.innerHTML = "";
    message_view.appendChild(loadingMsg);
  }
roomName = getTextContent(document.getElementById("rmname"));
 var parameters = {'room':roomName, 'startmsg': startmsg};
 if (is_safe_mode) {
   parameters['maxmsgs'] = 500;
   if (currentSortMode != null) {
     var SortBy = currentSortMode[0];
     if (SortBy.charAt(0) == 'r') {
       SortBy = SortBy.substr(1);
       parameters["SortOrder"] = "2";
     }
     parameters["SortBy"] = SortBy;
   }
 } 
 if (query.length > 0) {
   parameters["query"] = query;
 }
new Ajax.Request("roommsgs", {
    method: 'get',
	onSuccess: loadMessages,
	parameters: parameters,
	sanitize: false,
      evalJSON: false,
      onFailure: function(e) { alert("Failure: " + e);}
	});
}
function loadMessages(transport) {
  try {
  var data = eval('('+transport.responseText+')');
  if (!!data && transport.responseText.length < 2) {
    alert("Message loading failed");
  } 
  nummsgs = data['nummsgs'];
  var msgs = data['msgs'];
  var length = msgs.length;
  rowArray = new Array(); // store so they can be sorted
  var start = new Date();
  for(var i=0; i<length;i++) {
    var trElement = document.createElement("tr");
    var data = msgs[i];
    var msgId = data[0];
    var rowId = "msg_" + msgId;
    trElement.setAttribute("id",rowId);
    //$(trElement).observe('click', CtdlMessageListClick);
    trElement.ctdlMsgId = msgId;
    for(var j=1; j<5;j++) { // 1=msgId (hidden), 4 date timestamp (hidden) 6 = isNew etc. 
      var content = data[j];
      if(content.length < 1) {
	content = "(blank)";
      }
      if (j==3) {
       	trElement.ctdlDate = content;
      } else { 
	try {
      var tdElement = document.createElement("td");
      trElement.appendChild(tdElement);
      var txtContent = document.createTextNode(content);
      tdElement.appendChild(txtContent);
      var x=j;
      if (x==4) x=3;
      var classStmt = "col"+x;
      tdElement.setAttribute("class", classStmt);
	} catch (e) {
	  if (!!window.console && !!console.log) {
	    console.log("Error on #"+msgId +" col"+j+":"+e);
	  }
	}
      }
    }
    if (data[5]) {
      trElement.setAttribute("class", "new_message");
    }
    trElement.dropEnabled = true;
    trElement.ctdlRowId = i;
    trElement.ctdlMarked = false;
    rowArray[i] = trElement; 
  } 
  var end = new Date();
  if (!!window.console && !!console.log) {
    var delta = end.getTime() - start.getTime();
    console.log("loadMessages construct: " + delta);
  }
  } catch (e) {
    window.alert(e);
  }
  if (currentSortMode == null) {
  if (sortmode.length < 1) {
    sortmode = "rdate";
  }
  currentSortMode = [sortmode, sortModes[sortmode]];
  currentSorterToggle = toggles[sortmode];
  }
  if (!is_safe_mode) {
  resortAndDisplay(sortRowsByDateDescending);
  } else {
    resortAndDisplay(null);
  }
  loadingMsg.parentNode.removeChild(loadingMsg);
  setupPageSelector();
  query = "";
}
function resortAndDisplay(sortMode) {
  var start = new Date();
  emptyElement(message_view);
  var fragment = document.createDocumentFragment();
  if (sortMode != null) {
    rowArray.sort(sortMode);
  }
  var length = rowArray.length;
  for(var x=0; x<length; ++x) {
    try {
      var currentRow = rowArray[x];
      var className = currentRow.className;
    className = className.replace("table-alt-row","");
    className = className.replace("table-row","");
    if (((x-1) % 2) == 0) {
      className += " table-alt-row";
    } else {
      className += " table-row";
    }
    currentRow.className = className;
    /* Using element.onclick is evil, but until IE 
       supports addEventListener, it is much faster
       than prototype observe */
    currentRow.onclick = CtdlMessageListClick;
    currentRow.ctdlDnDElement = summaryViewDragAndDropHandler;
    fragment.appendChild(currentRow);
    } catch (e) {
      alert("Exception" + e);
    }
  }
  message_view.appendChild(fragment);
  var end = new Date();
  if (!!window.console && !!console.log) {
    var delta = end.getTime() - start.getTime();
    console.log("resortAndDisplay sort and append: " + delta);
  }
  ApplySorterToggle();
  normalizeHeaderTable();
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
  var subjectOne = getTextContent(a.getElementsByTagName("td")[0]).toLowerCase();
  var subjectTwo = getTextContent(b.getElementsByTagName("td")[0]).toLowerCase();
  return (subjectOne.charCodeAt(0) - subjectTwo.charCodeAt(0));
}

function sortRowsBySubjectDescending(a, b) {
  var subjectOne = getTextContent(a.getElementsByTagName("td")[0]).toLowerCase();
  var subjectTwo = getTextContent(b.getElementsByTagName("td")[0]).toLowerCase();
  return (subjectTwo.charCodeAt(0) - subjectOne.charCodeAt(0));
}

function sortRowsByFromAscending(a, b) {
  var fromOne = getTextContent(a.getElementsByTagName("td")[1]).toLowerCase();
  var fromTwo = getTextContent(b.getElementsByTagName("td")[1]).toLowerCase();
  return (fromOne.charCodeAt(0) - fromTwo.charCodeAt(0));
}

function sortRowsByFromDescending(a, b) {
  var fromOne = getTextContent(a.getElementsByTagName("td")[1]).toLowerCase();
  var fromTwo = getTextContent(b.getElementsByTagName("td")[1]).toLowerCase();
  return (fromTwo.charCodeAt(0) - fromOne.charCodeAt(0));
}

function CtdlMessageListClick(evt) {
  /* Since element.onload is used here, test to see if evt is defined */
  var event = evt ? evt : window.event; 
  var target = event.target ? event.target: event.srcElement; // and again..
  var parent = target.parentNode;
  var msgId = parent.ctdlMsgId;
  // If the ctrl key modifier wasn't used, unmark all rows and load the message
  if (!event.shiftKey && !event.ctrlKey) {
    unmarkAllRows();
    new Ajax.Updater('preview_pane', 'msg/'+msgId, {method: 'get'});
    markRow(parent);
    new Ajax.Request('ajax_servcmd', {
      method: 'post',
	  parameters: 'g_cmd=SEEN ' + msgId + '|1',
	  onComplete: CtdlMarkRowAsRead(parent)});
  } else if (event.button != 2) {
    markRow(parent);
    // TODO: introduce code to mark rows inbetween
  }
}
function CtdlMarkRowAsRead(rowElement) {
  var classes = rowElement.className;
  classes = classes.replace("new_message","");
  rowElement.className = classes;
}
function ApplySort(event) {
  var target = event.target;
  var sortId = target.id;
  removeOldSortClass();
  currentSorterToggle = target;
  var sortModes = getSortMode(target); // returns [[key, func],[key,func]]
  var sortModeToUse = null;
  if (currentSortMode[0] == sortModes[0][0]) {
    sortModeToUse = sortModes[1];
  } else {
    sortModeToUse = sortModes[0];
  }
  currentSortMode = sortModeToUse;
  if (is_safe_mode) {
    getMessages(); // in safe mode, we load from server already sorted
  } else {
  resortAndDisplay(sortModeToUse[1]);
  }
}
function getSortMode(toggleElem) {
  var forward = null;
  var reverse = null;
  for(var key in toggles) {
    var kr = (key.charAt(0) == 'r');
    if (toggles[key] == toggleElem && !kr) {
      forward = [key, sortModes[key]];
    } else if (toggles[key] == toggleElem && kr) {
      reverse = [key, sortModes[key]];
    }
  }
  return [forward, reverse];
}
function removeOldSortClass() {
  if (currentSorterToggle) {
    var classes = currentSorterToggle.className;
    classes = classes.replace("current_sort_mode","");
    classes = classes.replace("sort_ascending","");
    classes = classes.replace("sort_descending","");
    currentSorterToggle.className = classes;
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
function deleteAllMarkedRows() {
  for(msgId in currentlyMarkedRows) {
    var row = currentlyMarkedRows[msgId];
    var rowArrayId = row.ctdlRowId;
    row.parentNode.removeChild(row);
    delete currentlyMarkedRows[msgId];
    delete rowArray[rowArrayId];
  }
}
function CtdlMessageListKeyUp(event) {
  var key = event.which;
  if (key == 46) { // DELETE
    for(msgId in currentlyMarkedRows) {
      new Ajax.Request('ajax_servcmd', 
		       {method: 'post',
			   parameters: 'g_cmd=MOVE ' + msgId + '|_TRASH_|0',
			   });
    }
    deleteAllMarkedRows();
  }
}

function clearMessage(msgId) {
  var row = document.getElementById('msg_'+msgId);
  row.parentNode.removeChild(row);
  delete currentlyMarkedRows[msgId];
}

function summaryViewContextMenu() {
  if (!exitedMouseDown) {
    var contextSource = document.getElementById("listViewContextMenu");
    CtdlSpawnContextMenu(mouseDownEvent, contextSource);
  }
}

function summaryViewDragAndDropHandler() {
  var element = document.createElement("div");
  var msgList = document.createElement("ul");
  element.appendChild(msgList);
  for(msgId in currentlyMarkedRows) {
    msgRow = currentlyMarkedRows[msgId];
    var subject = getTextContent(msgRow.getElementsByTagName("td")[0]);
    var li = document.createElement("li");
    msgList.appendChild(li);
    setTextContent(li, subject);
    li.ctdlMsgId = msgId;
  }
  return element;
}

var saved_y = 0;
function CtdlResizeMouseDown(event) {
  $(document).observe('mousemove', CtdlResizeMouseMove);
  $(document).observe('mouseup', CtdlResizeMouseUp);
  saved_y = event.clientY;
}

function sizePreviewPane() {
  var preview_pane = document.getElementById("preview_pane");
  var content = $('content');  // we'd like to use prototype methods here
  var childElements = content.childElements();
  var sizeOfElementsAbove = 0;
  var heightOfContent = content.offsetHeight;
  for(var i=0; i<childElements.length; i++) {
    var element = childElements[i];
    if (element.id != 'preview_pane') {
      var height = element.offsetHeight;
      sizeOfElementsAbove += height;
    }
  }
  preview_pane.style.height = (heightOfContent-sizeOfElementsAbove)+"px";
}
function CtdlResizeMouseMove(event) {
  var clientX = event.clientX;
  var clientY = event.clientY;
  var summary_view = document.getElementById("summary_view");
  var summaryViewHeight = summary_view.offsetHeight;
  var increment = clientY-saved_y;
  summary_view.style.height = (increment+summaryViewHeight)+"px";
  var msglist = document.getElementById("preview_pane");
  var msgListHeight = msglist.offsetHeight;
  msglist.style.height = (msgListHeight-increment)+"px";
  saved_y = clientY;
  /* For some reason the grippy doesn't work without position: absolute
     so we need to set its top pos manually all the time */
  var resize = document.getElementById("resize_msglist");
  var resizePos = resize.offsetTop;
  resize.style.top = (resizePos+increment)+"px";
}
function CtdlResizeMouseUp(event) {
  $(document).stopObserving('mousemove', CtdlResizeMouseMove);
  $(document).stopObserving('mouseup', CtdlResizeMouseUp);
}
function ApplySorterToggle() {
  var className = currentSorterToggle.className;
  className += " current_sort_mode";
  if (currentSortMode[1] == sortRowsByDateDescending ||
      currentSortMode[1] == sortRowsBySubjectDescending ||
      currentSortMode[1] == sortRowsByFromDescending) {
    className += " sort_descending";
  } else {
    className += " sort_ascending";
  }
  currentSorterToggle.className = className;
}
/** Hack to make the header table line up with the data */
function normalizeHeaderTable() {
  var message_list_hdr = document.getElementById("message_list_hdr");
  var summary_view = document.getElementById("summary_view");
  var headerTable = message_list_hdr.getElementsByTagName("table")[0];
  var dataTable = summary_view.getElementsByTagName("table")[0];
  var dataTableWidth = dataTable.offsetWidth;
  headerTable.style.width = dataTableWidth+"px";
}

function setupPageSelector() {
  var summpage = document.getElementById("summpage");
  summpage.innerHTML = "";
  if (is_safe_mode) {
    summpage.parentNode.style.display="inline !important"; //override webcit.css
  } else {
    return;
  }
  var pages = nummsgs / 500;
  for(var i=0; i<pages; i++) {
    var opt = document.createElement("option");
    var startmsg = i * 500;
    opt.setAttribute("value",startmsg);
    opt.appendChild(document.createTextNode((i+1)));
    summpage.appendChild(opt);
  }
}
function getPage(event) {
  var target = event.target;
  startmsg = target.options.item(target.selectedIndex).value;
  getMessages();
}
function takeOverSearchOMatic() {
  var searchForm = document.getElementById("searchomatic").getElementsByTagName("form")[0];
  // First disable the form post
  searchForm.setAttribute("action","javascript:void();");
  searchForm.removeAttribute("method");
  $(searchForm).observe('submit', doSearch);
}
function doSearch() {
  query = document.getElementById("srchquery").value;
  getMessages();
  return false;
}
