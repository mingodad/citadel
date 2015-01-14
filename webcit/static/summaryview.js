/*
 * Webcit Summary View v2
 * All comments, flowers and death threats to Mathew McBride
 * <matt@mcbridematt.dhs.org> / <matt@comalies>
 * Copyright 2009 The Citadel Team
 * Licensed under the GPL V3
 *
 * QA reminders: because I keep forgetting / get cursed.
 * After changing anything in here, make sure that you still can:
 * 1. Resort messages in both normal and paged view.
 * 2. Select a range with shift-click 
 * 3. Select messages with ctrl-click
 * 4. Normal click will deselect everything done above
 * 5. Move messages, and they will disappear
 */

document.observe("dom:loaded", createMessageView);

var msgs = null;
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
var markedRowIndex = null;
var currentlyHasRowsSelected = false;

var mouseDownEvent = null;
var exitedMouseDown = false;

var originalMarkedRow = null;
var previousFinish = 0;
var markedFrom = 0;
var trTemplate = new Array(11);
trTemplate[0] = "<tr id=\"";
trTemplate[2] = "\" citadel:dropenabled=\"dropenabled\" class=\"";
trTemplate[4] = "\" citadel:dndelement=\"summaryViewDragAndDropHandler\" citadel:msgid=\"";
trTemplate[6] = "\" citadel:ctdlrowid=\"";
trTemplate[8] = "\"><td class=\"col1\">";
trTemplate[10] = "</td><td class=\"col2\">";
trTemplate[12] = "</td><td class=\"col3\">";
trTemplate[14] = "</td></tr>";


var currentPage = 0;
var sortModes = {
	"rdate" : sortRowsByDateDescending,
	"date" : sortRowsByDateAscending,
	"subj" : sortRowsBySubjectAscending,
	"rsubj" : sortRowsBySubjectDescending,
	"sender": sortRowsByFromAscending,
	"rsender" : sortRowsByFromDescending
};
var toggles = {};

var nummsgs = 0;
var startmsg = 0;
var newmsgs = 0;

function refreshMessageCounter() {
    var templStr = document.getElementById("nummsgs_template").innerHTML;
    templStr = templStr.replace(/UNREADMSGS/, newmsgs);
    templStr = templStr.replace(/TOTALMSGS/,  nummsgs);
    document.getElementById("nummsgs").innerHTML = templStr;
}

function createMessageView() {
	message_view = document.getElementById("message_list_body");
	loadingMsg = document.getElementById("loading");
	getMessages();
	mlh_date = $("mlh_date");
	mlh_subject = $('mlh_subject');
	mlh_from = $('mlh_from');
	toggles["rdate"] = mlh_date;
	toggles["date"] = mlh_date;
	toggles["subj"] = mlh_subject;
	toggles["rsubj"] = mlh_subject;
	toggles["sender"] = mlh_from;
	toggles["rsender"] = mlh_from;
	mlh_date.observe('click',ApplySort);
	mlh_subject.observe('click',ApplySort);
	mlh_from.observe('click',ApplySort);
	$(document).observe('keyup',CtdlMessageListKeyUp,false);
	$('resize_msglist').observe('mousedown', CtdlResizeMouseDown);
	$('m_refresh').observe('click', getMessages);
	document.getElementById('m_refresh').setAttribute("href","#");
	Event.observe(document.onresize ? document : window, "resize", normalizeHeaderTable);
	Event.observe(document.onresize ? document : window, "resize", sizePreviewPane);
	if ( $('summpage') ) {
		$('summpage').observe('change', getPage);
	}
	else {
		alert('error: summpage does not exist');
	}
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
		parameters['stopmsg'] = parseInt(startmsg)+499;
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

function evalJSON(data) {
	var jsonData = null;
	if (typeof(JSON) === 'object' && typeof(JSON.parse) === 'function') {
		try {
			jsonData = JSON.parse(data);
		} catch (e) {
			// ignore
		}
	}
	if (jsonData == null) {
		jsonData = eval('('+data+')');
	}
	return jsonData;
}
function loadMessages(transport) {
	try {
		var data = evalJSON(transport.responseText);
		if (!!data && transport.responseText.length < 2) {
			alert("Message loading failed");
		} 
		nummsgs = data['nummsgs'];
	        newmsgs =  data['newmsgs'];
		msgs = data['msgs'];
		var length = msgs.length;
		rowArray = new Array(length); // store so they can be sorted
		WCLog("Row array length: "+rowArray.length);
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
		resortAndDisplay(currentSortMode[1]);
	} else {
		setupPageSelector();
		resortAndDisplay(null);
	}
	if (loadingMsg.parentNode != null) {
		loadingMsg.parentNode.removeChild(loadingMsg);
	}
	sizePreviewPane();
        refreshMessageCounter();
}
function resortAndDisplay(sortMode) {
	WCLog("Begin resortAndDisplay");
  
	/* We used to try and clear out the message_view element,
	   but stupid IE doesn't even do that properly */
	var message_view_parent = message_view.parentNode;
	message_view_parent.removeChild(message_view);
	var startSort = new Date();
	try {
		if (sortMode != null) {
			msgs.sort(sortMode);
		}
	} catch (e) {
		WCLog("Sort error: " + e);
	}
	var endSort = new Date();
	WCLog("Sort rowArray in " + (endSort-startSort));
	var start = new Date();
	var length = msgs.length;
	var compiled = new Array(length+2);
	compiled[0] = "<table class=\"mailbox_summary\" id=\"summary_headers\" \"cellspacing=0\" style=\"width:100%;-moz-user-select:none;\">";
	for(var x=0; x<length; ++x) {
		try {
			var currentRow = msgs[x];
			trTemplate[1] = "msg_"+currentRow[0];
			var className = "";
			if (((x-1) % 2) == 0) {
				className += "table-alt-row";
			} else {
				className += "table-row";
			}
			if (currentRow[5]) {
				className += " new_message";
			}
			trTemplate[3] = className;
			trTemplate[5] = currentRow[0];
			trTemplate[7] = x;
			trTemplate[9] = currentRow[1];
			trTemplate[11] = currentRow[2];
			trTemplate[13] = currentRow[4];
			var i = x+1;
			compiled[i] = trTemplate.join("");
		} catch (e) {
			WCLog("Exception on row " +  x + ":" + e);
		}
	}
	compiled[length+2] = "</table>";
	var end = new Date();
	WCLog("iterate: " + (end-start));
	var compile = compiled.join("");
	start = new Date();
	$(message_view_parent).update(compile);
	message_view_parent.observe('click', CtdlMessageListClick);
	message_view = message_view_parent.firstChild;
	end = new Date();
	var delta = end.getTime() - start.getTime();
	WCLog("append: " + delta);
	ApplySorterToggle();
	normalizeHeaderTable();
}
function sortRowsByDateAscending(a, b) {
	var dateOne = a[3];
	var dateTwo = b[3];
	return (dateOne - dateTwo);
};
function sortRowsByDateDescending(a, b) {
	var dateOne = a[3];
	var dateTwo = b[3];
	return (dateTwo - dateOne);
};
function sortRowsBySubjectAscending(a, b) {
	var subjectOne = a[1];
	var subjectTwo = b[1];
	return strcmp(subjectOne, subjectTwo);
};
function sortRowsBySubjectDescending(a, b) {
	var subjectOne = a[1];
	var subjectTwo = b[1];
	return strcmp(subjectTwo, subjectOne);
};
function sortRowsByFromAscending(a, b) {
	var fromOne = a[2];
	var fromTwo = b[2];
	return strcmp(fromOne, fromTwo);
};
function sortRowsByFromDescending(a, b) {
	var fromOne = a[2];
	var fromTwo = b[2];
	return strcmp(fromTwo, fromOne);
};
function CtdlMessageListClick(evt) {
    /* Since element.onload is used here, test to see if evt is defined */
    var event = evt ? evt : window.event; 
    var target = event.target ? event.target: event.srcElement; // and again..
    var parent = target.parentNode;
    var msgId = parent.getAttribute("citadel:msgid");
    var is_shift_pressed = event.shiftKey;
    var is_ctrl_pressed = event.ctrlKey;
    var rowclass = parent.getAttribute("class");
    var msgUnseen = rowclass.search("new_message") >= 0;

    /* debugging
       str = '.';
       if (is_shift_pressed) {
       str = str + 'S';
       }
       str = str + '.';
       if (is_ctrl_pressed) {
       str = str + 'C';
       }
       str = str + '.';
       $('ib_summary').innerHTML = str;
    */

    // If the ctrl key modifier wasn't used, unmark all rows and load the message
    if (!is_shift_pressed && !is_ctrl_pressed) {
	previousFinish = 0;
	markedFrom = 0;
	unmarkAllRows();
	markedRowIndex = parent.rowIndex;
	originalMarkedRow = parent;
	document.getElementById("preview_pane").innerHTML = "";
	new Ajax.Updater('preview_pane', 'msg/'+msgId, {method: 'get'});
	markRow(parent);

	if (msgUnseen) {
	    var p = encodeURI('g_cmd=SEEN ' + msgId + '|1');
	    new Ajax.Request('ajax_servcmd', {
		method: 'post',
		parameters: p,
		onComplete: CtdlMarkRowAsRead(parent)
	    });
	}
	// If the shift key modifier is used, mark a range...
    } else if (event.button != 2 && is_shift_pressed) {
	if (originalMarkedRow == null) {
	    originalMarkedRow = parent;
	    markRow(parent);
	} else {
	    unmarkAllRows();
	    markRow(parent);
	    markRow(originalMarkedRow);
	}
	var rowIndex = parent.rowIndex;
	if (markedFrom == 0) {
	    markedFrom = rowIndex;
	}
	var startMarkingFrom = 0;
	var finish = 0;
	if (markedRowIndex === null) {
	    startMarkingFrom = 0;
	    finish = rowIndex;
	}
	else if (rowIndex > markedRowIndex) {
	    startMarkingFrom = markedRowIndex+1;
	    finish = rowIndex;
	} else if (rowIndex < markedRowIndex) {
	    startMarkingFrom = rowIndex+1;
	    finish = markedRowIndex;
	}
	previousFinish = finish;
	WCLog('startMarkingFrom=' + startMarkingFrom + ', finish=' + finish);
	for(var x = startMarkingFrom; x<finish; x++) {
	    WCLog("Marking row " + x);
	    markRow(parent.parentNode.rows[x]);
	}
	// If the ctrl key modifier is used, toggle one message
    } else if (event.button != 2 && is_ctrl_pressed) {
	if (parent.getAttribute("citadel:marked")) {
	    unmarkRow(parent);
	}
	else {
	    markRow(parent);
	}
    }
}
function CtdlMarkRowAsRead(rowElement) {
	var classes = rowElement.className;
	classes = classes.replace("new_message","");
	rowElement.className = classes;
        newmsgs--;
	refreshMessageCounter();
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
function markRow(row) {
	var msgId = row.getAttribute("citadel:msgid");
	row.className = row.className += " marked_row";
	row.setAttribute("citadel:marked","marked");
	currentlyMarkedRows[msgId] = row;
}
function unmarkRow(row) {
	var msgId = row.getAttribute("citadel:msgid");
	row.className = row.className.replace("marked_row","");
	row.removeAttribute("citadel:marked");
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
		var rowclass = row.getAttribute("class");
		var msgUnseen = rowclass.search("new_message") >= 0;

		var rowArrayId = row.getAttribute("citadel:ctdlrowid");
		row.parentNode.removeChild(row);
		delete currentlyMarkedRows[msgId];
		delete msgs[rowArrayId];
		if (msgUnseen)
		    newmsgs--;
	        nummsgs--;
	}
	// Now we have to reconstruct rowarray as the array length has changed */
	var newMsgs = new Array(msgs.length-1);
	var x=0;
	for(var i=0; i<rowArray.length; i++) {
		var currentRow = msgs[i];
		if (currentRow != null) {
			newMsgs[x] = currentRow;
			x++;
		}
	}
	msgs = newMsgs;
	resortAndDisplay(null);
	refreshMessageCounter();
}
function deleteAllSelectedMessages() {
    var mvCommand = "";
    var msgIds = "";
    for(msgId in currentlyMarkedRows) {
	msgIds += ","+msgId;

	if (msgIds.length > 800) {
	    if (!room_is_trash) {
		mvCommand = encodeURI("g_cmd=MOVE " + msgIds + "|_TRASH_|0");
	    }
	    else {
		mvCommand = encodeURI("g_cmd=DELE " + msgIds);
	    }
	    new Ajax.Request("ajax_servcmd", {
		parameters: mvCommand,
		method: 'post',
		onSuccess: function(transport) {
		    WCLog(transport.responseText);
		}
	    });
	    msgIds = "";
	}
    }

    if (!room_is_trash) {
	mvCommand = encodeURI("g_cmd=MOVE " + msgIds + "|_TRASH_|0");
    }
    else {
	mvCommand = encodeURI("g_cmd=DELE " + msgIds);
    }
    new Ajax.Request("ajax_servcmd", {
	parameters: mvCommand,
	method: 'post',
	onSuccess: function(transport) {
	    WCLog(transport.responseText);
	}
    });

    document.getElementById("preview_pane").innerHTML = "";
    deleteAllMarkedRows();
}


function CtdlMessageListKeyUp(event) {
	var key = event.which || event.keyCode;

	if (key == 46) {				/* DELETE */
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
	contentViewPortHeight = 0.95 * contentViewPortHeight; // leave some error (especially for FF3!!)
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

/* Hack to make the header table line up with the data */
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
	var pages = nummsgs / 499;
	for(var i=0; i<pages; i++) {
		var opt = document.createElement("option");
		var startmsg = i * 499;
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
