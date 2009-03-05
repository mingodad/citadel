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
var markedRowId = null;

var mouseDownEvent = null;
var exitedMouseDown = false;

var currentPage = 0;
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
  //window.oncontextmenu = function() { return false; };  
  $('resize_msglist').observe('mousedown', CtdlResizeMouseDown);
  $('m_refresh').observe('click', getMessages);
  document.getElementById('m_refresh').setAttribute("href","#");
  Event.observe(document.onresize ? document : window, "resize", normalizeHeaderTable);
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
 var parameters = {'room':roomName, 'startmsg': startmsg, 'stopmsg': -1};
 if (is_safe_mode) {
   parameters['stopmsg'] = parseInt(startmsg)+500;
   //parameters['maxmsgs'] = 500;
   if (currentSortMode != null) {
     var SortBy = currentSortMode[0];
     if (SortBy.charAt(0) == 'r') {
       SortBy = SortBy.substr(1);
       parameters["SortOrder"] = "0";
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
  WCLog("Row array length: "+rowArray.length);
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
      //tdElement.setAttribute("class", classStmt);
      tdElement.className = classStmt;
	} catch (e) {
	  WCLog("Error on #"+msgId +" col"+j+":"+e);
	}
      }
    }
    if (data[5]) {
      trElement.ctdlNewMsg = true;
    }
    trElement.dropEnabled = true;
    trElement.ctdlMarked = false;
    rowArray[i] = trElement; 
  } 
  var end = new Date();
  var delta = end.getTime() - start.getTime();
    WCLog("loadMessages construct: " + delta);
  } catch (e) {
    //window.alert(e+"|"+e.description);
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
    setupPageSelector();
    resortAndDisplay(null);
  }
  if (loadingMsg.parentNode != null) {
    loadingMsg.parentNode.removeChild(loadingMsg);
  }
  sizePreviewPane();
}
function resortAndDisplay(sortMode) {
  WCLog("Begin resortAndDisplay");
  var start = new Date();
  /* We used to try and clear out the message_view element,
     but stupid IE doesn't even do that properly */
  var message_view_parent = message_view.parentNode;
  message_view_parent.removeChild(message_view);
  message_view = document.createElement("tbody");
  message_view.setAttribute("id","message_list_body");
  message_view.className="mailbox_summary";
  message_view_parent.appendChild(message_view);
  
  var fragment = document.createDocumentFragment();
  if (sortMode != null) {
    rowArray.sort(sortMode);
  }
  var length = rowArray.length;
  for(var x=0; x<length; ++x) {
    try {
      var currentRow = rowArray[x];
      currentRow.setAttribute("class","");
      var className = "";
    if (((x-1) % 2) == 0) {
      className = "table-alt-row";
    } else {
      className = "table-row";
    }
    if (currentRow.ctdlNewMsg) {
      className += " new_message";
    }
    currentRow.className = className;
    /* Using element.onclick is evil, but until IE 
       supports addEventListener, it is much faster
       than prototype observe */
    currentRow.onclick = CtdlMessageListClick;
    currentRow.ctdlDnDElement = summaryViewDragAndDropHandler;
    currentRow.ctdlRowId = x;
    fragment.appendChild(currentRow);
    } catch (e) {
      alert("Exception" + e);
    }
  }
  message_view.appendChild(fragment);
  var end = new Date();
    var delta = end.getTime() - start.getTime();
    WCLog("resortAndDisplay sort and append: " + delta);
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
  return strcmp(subjectOne, subjectTwo);
}

function sortRowsBySubjectDescending(a, b) {
  var subjectOne = getTextContent(a.getElementsByTagName("td")[0]).toLowerCase();
  var subjectTwo = getTextContent(b.getElementsByTagName("td")[0]).toLowerCase();
  return strcmp(subjectOne, subjectTwo);
}

function sortRowsByFromAscending(a, b) {
  var fromOne = getTextContent(a.getElementsByTagName("td")[1]).toLowerCase();
  var fromTwo = getTextContent(b.getElementsByTagName("td")[1]).toLowerCase();
  return strcmp(fromOne, fromTwo);
}

function sortRowsByFromDescending(a, b) {
  var fromOne = getTextContent(a.getElementsByTagName("td")[1]).toLowerCase();
  var fromTwo = getTextContent(b.getElementsByTagName("td")[1]).toLowerCase();
  return strcmp(fromOne, fromTwo);
}

function CtdlMessageListClick(evt) {
  /* Since element.onload is used here, test to see if evt is defined */
  var event = evt ? evt : window.event; 
  var target = event.target ? event.target: event.srcElement; // and again..
  var parent = target.parentNode;
  var msgId = parent.ctdlMsgId;
  // If the ctrl key modifier wasn't used, unmark all rows and load the message
  if (!event.shiftKey && !event.ctrlKey && !event.altKey) {
    unmarkAllRows();
    markedRowId = parent.ctdlRowId;
    document.getElementById("preview_pane").innerHTML = "";
    new Ajax.Updater('preview_pane', 'msg/'+msgId+'?Mail=1', {method: 'get'});
    markRow(parent);
    new Ajax.Request('ajax_servcmd', {
      method: 'post',
	  parameters: 'g_cmd=SEEN ' + msgId + '|1',
	  onComplete: CtdlMarkRowAsRead(parent)});
  } else if (event.button != 2 && event.shiftKey) {
    markRow(parent);
    var rowId = parent.ctdlRowId;
    var startMarkingFrom = 0;
    var finish = 0;
    if (rowId > markedRowId) {
      startMarkingFrom = markedRowId+1;
      finish = rowId;
    } else if (rowId < markedRowId) {
      startMarkingFrom = rowId+1;
      finish = markedRowId;
    } 
    for(var x = startMarkingFrom; x<finish; x++) {
      WCLog("Marking row "+x);
      markRow(rowArray[x]);
    }
  } else if (event.button != 2 && (event.ctrlKey || event.altKey)) {
    markRow(parent);
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
    /* classes = classes.replace("current_sort_mode","");
    classes = classes.replace("sort_ascending","");
    classes = classes.replace("sort_descending",""); */
    currentSorterToggle.className = "";
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
  // Now we have to reconstruct rowarray as the array length has changed */
  var newRowArray = new Array();
  var x=0;
  for(var i=0; i<rowArray.length; i++) {
    var currentRow = rowArray[i];
    if (currentRow != null) {
      newRowArray[x] = currentRow;
      x++;
    }
  }
  rowArray = newRowArray;
  resortAndDisplay(null);
}

function deleteAllSelectedMessages() {
    for(msgId in currentlyMarkedRows) {
      if (!room_is_trash) {
      new Ajax.Request('ajax_servcmd', 
		       {method: 'post',
			   parameters: 'g_cmd=MOVE ' + msgId + '|_TRASH_|0'
			   });
      } else {
	new Ajax.Request('ajax_servcmd', {method: 'post',
	      parameters: 'g_cmd=DELE '+msgId});
      }
    }
    document.getElementById("preview_pane").innerHTML = "";
    deleteAllMarkedRows();
}

function CtdlMessageListKeyUp(event) {
	var key = event.which;
	if (key == 46) { // DELETE
		deleteAllSelectedMessages();
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
  var summary_view = document.getElementById("summary_view");
  var banner = document.getElementById("banner");
  var message_list_hdr = document.getElementById("message_list_hdr");
  var content = $('global');  // we'd like to use prototype methods here
  var childElements = content.childElements();
  var sizeOfElementsAbove = 0;
  var heightOfViewPort = document.viewport.getHeight() // prototypejs method
  var bannerHeight = banner.offsetHeight;
  var contentViewPortHeight = heightOfViewPort-banner.offsetHeight-message_list_hdr.offsetHeight;
  contentViewPortHeight = 0.98 * contentViewPortHeight; // leave some error
  // Set summary_view to 20%;
  var summary_height = ctdlLocalPrefs.readPref("svheight");
  if (summary_height == null) {
    summary_height = 0.20 * contentViewPortHeight;
  }
  // Set preview_pane to the remainder
  var preview_height = contentViewPortHeight - summary_height;
  
  summary_view.style.height = (summary_height)+"px";
  preview_pane.style.height = (preview_height)+"px";
}
function CtdlResizeMouseMove(event) {
  var clientX = event.clientX;
  var clientY = event.clientY;
  var summary_view = document.getElementById("summary_view");
  var summaryViewHeight = summary_view.offsetHeight;
  var increment = clientY-saved_y;
  var summary_view_height = increment+summaryViewHeight;
  summary_view.style.height = (summary_view_height)+"px";
  // store summary view height 
  ctdlLocalPrefs.setPref("svheight",summary_view_height);
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
  var resize_msglist = document.getElementById("resize_msglist");
  var headerTable = message_list_hdr.getElementsByTagName("table")[0];
  var dataTable = summary_view.getElementsByTagName("table")[0];
  var dataTableWidth = dataTable.offsetWidth;
  headerTable.style.width = dataTableWidth+"px";
}

function setupPageSelector() {
  var summpage = document.getElementById("summpage");
  var select_page = document.getElementById("selectpage");
  summpage.innerHTML = "";
  if (is_safe_mode) {
    WCLog("unhiding parent page");
    select_page.className = "";
  } else {
    return;
  }
  var pages = nummsgs / 500;
  for(var i=0; i<pages; i++) {
    var opt = document.createElement("option");
    var startmsg = i * 500;
    opt.setAttribute("value",startmsg);
    if (currentPage == i) {
      opt.setAttribute("selected","selected");
    }
    opt.appendChild(document.createTextNode((i+1)));
    summpage.appendChild(opt);
  }
}
function getPage(event) {
  var target = event.target;
  startmsg = target.options.item(target.selectedIndex).value;
  currentPage = target.selectedIndex;
  //query = ""; // We are getting a page from the _entire_ msg list, don't query
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
