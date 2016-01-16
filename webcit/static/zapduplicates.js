function loadZapMessages(transport)
{
    var dupes = '';
    var dupcount = 0;
    try {
	var data = evalJSON(transport.responseText);
	if (!!data && transport.responseText.length < 2) {
	    alert("Message loading failed");
	} 
	nummsgs = data['nummsgs'];
	msgs = data['msgs'];
	var length = msgs.length;
	rowArray = new Array(length); // store so they can be sorted
	wCLog("Row array length: "+rowArray.length);
	for(var x=1; x<length; ++x) {
	    var currentRow = msgs[x];
	    var LastRow = msgs[x-1];
	    if ((currentRow[1] == LastRow[1]) &&
		(currentRow[2] == LastRow[2]) &&
		(currentRow[4] == LastRow[4]))
	    {
		dupcount ++;
		dupes += currentRow[0] + ',' ;
		if (dupes.length > 800) {
		    var mvCommand = "g_cmd=MOVE%20" + dupes + "|"+encodeURIComponent('Trash')+"|0";
		    new Ajax.Request("ajax_servcmd", {
			parameters: mvCommand,
			method: 'post',
		    });
		    dupes = "";
		}

	    }

	}

    } catch (e) {
	window.alert(e+"|"+e.description);
    }
    var mvCommand = "g_cmd=MOVE%20" + dupes + "|"+encodeURIComponent('Trash')+"|0";
    new Ajax.Request("ajax_servcmd", {
	parameters: mvCommand,
	method: 'post',
    });

    alert ('deleted: '+dupcount+'messages');
}

function TriggerLoadDupes ()
{
//    alert("bla");
// http://127.0.0.1:2000/roommsgs?room=test%20rss&startmsg=0&stopmsg=499&SortBy=Subject&SortOrder=1
	roomName = getTextContent(document.getElementById("rmname"));
    var parameters = {'room':roomName, 'startmsg': 0, 'stopmsg': -1, 'SortBy' : 'Subject', 'SortOrder' : 1};

	new Ajax.Request("roommsgs", {
		method: 'get',
				onSuccess: loadZapMessages,
				parameters: parameters,
				sanitize: false,
				evalJSON: false,
				onFailure: function(e) { alert("Failure: " + e);}
	});
}
