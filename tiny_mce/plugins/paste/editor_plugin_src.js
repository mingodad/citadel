/* Import plugin specific language pack */ 
tinyMCE.importPluginLanguagePack('paste', 'en,sv,cs,zh_cn,fr_ca,da,he,nb,de,hu,ru,ru_KOI8-R,ru_UTF-8,nn,fi,es,cy,is,pl,nl,fr,pt_br');

function TinyMCE_paste_getInfo() {
	return {
		longname : 'Paste text/word',
		author : 'Moxiecode Systems',
		authorurl : 'http://tinymce.moxiecode.com',
		infourl : 'http://tinymce.moxiecode.com/tinymce/docs/plugin_paste.html',
		version : tinyMCE.majorVersion + "." + tinyMCE.minorVersion
	};
};

function TinyMCE_paste_initInstance(inst) {
	if (tinyMCE.isMSIE && tinyMCE.getParam("paste_auto_cleanup_on_paste", false))
		tinyMCE.addEvent(inst.getBody(), "paste", TinyMCE_paste_handleEvent);
}

function TinyMCE_paste_handleEvent(e) {
	switch (e.type) {
		case "paste":
			var html = TinyMCE_paste__clipboardHTML();

			// Removes italic, strong etc
			tinyMCE.execCommand('delete');

			if (html && html.length > 0)
				tinyMCE.execCommand('mcePasteWord', false, html);

			tinyMCE.cancelEvent(e);
			return false;
	}

	return true;
}

function TinyMCE_paste_getControlHTML(control_name) { 
	switch (control_name) { 
		case "pastetext":
			var cmd = 'tinyMCE.execInstanceCommand(\'{$editor_id}\',\'mcePasteText\', true);return false;';
			return '<a href="javascript:' + cmd + '" onclick="' + cmd + '" target="_self" onmousedown="return false;"><img id="{$editor_id}pastetext" src="{$pluginurl}/images/pastetext.gif" title="{$lang_paste_text_desc}" width="20" height="20" class="mceButtonNormal" onmouseover="tinyMCE.switchClass(this,\'mceButtonOver\');" onmouseout="tinyMCE.restoreClass(this);" onmousedown="tinyMCE.restoreClass(this);" /></a>'; 

		case "pasteword":
			var cmd = 'tinyMCE.execInstanceCommand(\'{$editor_id}\',\'mcePasteWord\', true);return false;';
			return '<a href="javascript:' + cmd + '" onclick="' + cmd + '" target="_self" onmousedown="return false;"><img id="{$editor_id}pasteword" src="{$pluginurl}/images/pasteword.gif" title="{$lang_paste_word_desc}" width="20" height="20" class="mceButtonNormal" onmouseover="tinyMCE.switchClass(this,\'mceButtonOver\');" onmouseout="tinyMCE.restoreClass(this);" onmousedown="tinyMCE.restoreClass(this);" /></a>'; 

		case "selectall":
			var cmd = 'tinyMCE.execInstanceCommand(\'{$editor_id}\',\'mceSelectAll\');return false;';
			return '<a href="javascript:' + cmd + '" onclick="' + cmd + '" target="_self" onmousedown="return false;"><img id="{$editor_id}selectall" src="{$pluginurl}/images/selectall.gif" title="{$lang_selectall_desc}" width="20" height="20" class="mceButtonNormal" onmouseover="tinyMCE.switchClass(this,\'mceButtonOver\');" onmouseout="tinyMCE.restoreClass(this);" onmousedown="tinyMCE.restoreClass(this);" /></a>'; 
	} 

	return ''; 
} 

function TinyMCE_paste_execCommand(editor_id, element, command, user_interface, value) { 
	switch (command) { 
		case "mcePasteText": 
			if (user_interface) {
				if ((tinyMCE.isMSIE && !tinyMCE.isOpera) && !tinyMCE.getParam('paste_use_dialog', false))
					TinyMCE_paste__insertText(clipboardData.getData("Text"), true); 
				else { 
					var template = new Array(); 
					template['file']	= '../../plugins/paste/pastetext.htm'; // Relative to theme 
					template['width']  = 450; 
					template['height'] = 400; 
					var plain_text = ""; 
					tinyMCE.openWindow(template, {editor_id : editor_id, plain_text: plain_text, resizable : "yes", scrollbars : "no", inline : "yes", mceDo : 'insert'}); 
				}
			} else
				TinyMCE_paste__insertText(value['html'], value['linebreaks']);

			return true;

		case "mcePasteWord": 
			if (user_interface) {
				if ((tinyMCE.isMSIE && !tinyMCE.isOpera) && !tinyMCE.getParam('paste_use_dialog', false)) {
					var html = TinyMCE_paste__clipboardHTML();

					if (html && html.length > 0)
						TinyMCE_paste__insertWordContent(html);
				} else { 
					var template = new Array(); 
					template['file']	= '../../plugins/paste/pasteword.htm'; // Relative to theme 
					template['width']  = 450; 
					template['height'] = 400; 
					var plain_text = ""; 
					tinyMCE.openWindow(template, {editor_id : editor_id, plain_text: plain_text, resizable : "yes", scrollbars : "no", inline : "yes", mceDo : 'insert'});
				}
			} else
				TinyMCE_paste__insertWordContent(value);

		 	return true;

		case "mceSelectAll":
			tinyMCE.execInstanceCommand(editor_id, 'selectall'); 
			return true; 

	} 

	// Pass to next handler in chain 
	return false; 
} 

function TinyMCE_paste__insertText(content, bLinebreaks) { 
	if (content && content.length > 0) {
		if (bLinebreaks) { 
			// Special paragraph treatment 
			if (tinyMCE.getParam("paste_create_paragraphs", true)) {
				var rl = tinyMCE.getParam("paste_replace_list", '\u2122,<sup>TM</sup>,\u2026,...,\u201c|\u201d,",\u2019,\',\u2013|\u2014|\u2015|\u2212,-').split(',');
				for (var i=0; i<rl.length; i+=2)
					content = content.replace(new RegExp(rl[i], 'gi'), rl[i+1]);

				content = tinyMCE.regexpReplace(content, "\r\n\r\n", "</p><p>", "gi"); 
				content = tinyMCE.regexpReplace(content, "\r\r", "</p><p>", "gi"); 
				content = tinyMCE.regexpReplace(content, "\n\n", "</p><p>", "gi"); 

				// Has paragraphs 
				if ((pos = content.indexOf('</p><p>')) != -1) { 
					tinyMCE.execCommand("Delete"); 

					var node = tinyMCE.selectedInstance.getFocusElement(); 

					// Get list of elements to break 
					var breakElms = new Array(); 

					do { 
						if (node.nodeType == 1) { 
							// Don't break tables and break at body 
							if (node.nodeName == "TD" || node.nodeName == "BODY") 
								break; 
	
							breakElms[breakElms.length] = node; 
						} 
					} while(node = node.parentNode); 

					var before = "", after = "</p>"; 
					before += content.substring(0, pos); 

					for (var i=0; i<breakElms.length; i++) { 
						before += "</" + breakElms[i].nodeName + ">"; 
						after += "<" + breakElms[(breakElms.length-1)-i].nodeName + ">"; 
					} 

					before += "<p>"; 
					content = before + content.substring(pos+7) + after; 
				} 
			} 

			if (tinyMCE.getParam("paste_create_linebreaks", true)) {
				content = tinyMCE.regexpReplace(content, "\r\n", "<br />", "gi"); 
				content = tinyMCE.regexpReplace(content, "\r", "<br />", "gi"); 
				content = tinyMCE.regexpReplace(content, "\n", "<br />", "gi"); 
			}
		} 
	
		tinyMCE.execCommand("mceInsertRawHTML", false, content); 
	}
}

function TinyMCE_paste__insertWordContent(content) { 
	if (content && content.length > 0) {
		// Cleanup Word content
		var bull = String.fromCharCode(8226);
		var middot = String.fromCharCode(183);

		var rl = tinyMCE.getParam("paste_replace_list", '\u2122,<sup>TM</sup>,\u2026,...,\u201c|\u201d,",\u2019,\',\u2013|\u2014|\u2015|\u2212,-').split(',');
		for (var i=0; i<rl.length; i+=2)
			content = content.replace(new RegExp(rl[i], 'gi'), rl[i+1]);

		if (tinyMCE.getParam("paste_convert_headers_to_strong", false)) {
			content = content.replace(new RegExp('<p class=MsoHeading.*?>(.*?)<\/p>', 'gi'), '<p><b>$1</b></p>');
		}

		content = content.replace(new RegExp('tab-stops: list [0-9]+.0pt">', 'gi'), '">' + "--list--");
		content = content.replace(new RegExp(bull + "(.*?)<BR>", "gi"), "<p>" + middot + "$1</p>");
		content = content.replace(new RegExp('<SPAN style="mso-list: Ignore">', 'gi'), "<span>" + bull); // Covert to bull list
		content = content.replace(/<o:p><\/o:p>/gi, "");
		content = content.replace(new RegExp('<br style="page-break-before: always;.*>', 'gi'), '-- page break --'); // Replace pagebreaks
		content = content.replace(new RegExp('<(!--)([^>]*)(--)>', 'g'), "");  // Word comments
		content = content.replace(/<\/?span[^>]*>/gi, "");
		content = content.replace(new RegExp('<(\\w[^>]*) style="([^"]*)"([^>]*)', 'gi'), "<$1$3");
		content = content.replace(/<\/?font[^>]*>/gi, "");

		// Strips class attributes.
		switch (tinyMCE.getParam("paste_strip_class_attributes", "all")) {
			case "all":
				content = content.replace(/<(\w[^>]*) class=([^ |>]*)([^>]*)/gi, "<$1$3");
				break;

			case "mso":
				content = content.replace(new RegExp('<(\\w[^>]*) class="?mso([^ |>]*)([^>]*)', 'gi'), "<$1$3");
				break;
		}

		content = content.replace(new RegExp('href="?' + TinyMCE_paste__reEscape("" + document.location) + '', 'gi'), 'href="' + tinyMCE.settings['document_base_url']);
		content = content.replace(/<(\w[^>]*) lang=([^ |>]*)([^>]*)/gi, "<$1$3");
		content = content.replace(/<\\?\?xml[^>]*>/gi, "");
		content = content.replace(/<\/?\w+:[^>]*>/gi, "");
		content = content.replace(/-- page break --\s*<p>&nbsp;<\/p>/gi, ""); // Remove pagebreaks
		content = content.replace(/-- page break --/gi, ""); // Remove pagebreaks

//		content = content.replace(/\/?&nbsp;*/gi, ""); &nbsp;
//		content = content.replace(/<p>&nbsp;<\/p>/gi, '');

		if (!tinyMCE.settings['force_p_newlines']) {
			content = content.replace('', '' ,'gi');
			content = content.replace('</p>', '<br /><br />' ,'gi');
		}

		if (!tinyMCE.isMSIE && !tinyMCE.settings['force_p_newlines']) {
			content = content.replace(/<\/?p[^>]*>/gi, "");
		}

		content = content.replace(/<\/?div[^>]*>/gi, "");

		// Convert all middlot lists to UL lists
		if (tinyMCE.getParam("paste_convert_middot_lists", true)) {
			var div = document.createElement("div");
			div.innerHTML = content;

			// Convert all middot paragraphs to li elements
			var className = tinyMCE.getParam("paste_unindented_list_class", "unIndentedList");

			while (TinyMCE_paste_convertMiddots(div, "--list--")) ; // bull
			while (TinyMCE_paste_convertMiddots(div, middot, className)) ; // Middot
			while (TinyMCE_paste_convertMiddots(div, bull)) ; // bull

			content = div.innerHTML;
		}

		// Replace all headers with strong and fix some other issues
		if (tinyMCE.getParam("paste_convert_headers_to_strong", false)) {
			content = content.replace(/<h[1-6]>&nbsp;<\/h[1-6]>/gi, '<p>&nbsp;&nbsp;</p>');
			content = content.replace(/<h[1-6]>/gi, '<p><b>');
			content = content.replace(/<\/h[1-6]>/gi, '</b></p>');
			content = content.replace(/<b>&nbsp;<\/b>/gi, '<b>&nbsp;&nbsp;</b>');
			content = content.replace(/^(&nbsp;)*/gi, '');
		}

		content = content.replace(/--list--/gi, ""); // Remove --list--

		// Insert cleaned content
		tinyMCE.execCommand("mceInsertContent", false, content);
		tinyMCE.execCommand("mceCleanup"); // Do normal cleanup
	}
}

function TinyMCE_paste__reEscape(s) {
	var l = "?.\\*[](){}+^$:";
	var o = "";

	for (var i=0; i<s.length; i++) {
		var c = s.charAt(i);

		if (l.indexOf(c) != -1)
			o += '\\' + c;
		else
			o += c;
	}

	return o;
}

function TinyMCE_paste_convertMiddots(div, search, class_name) {
	var mdot = String.fromCharCode(183);
	var bull = String.fromCharCode(8226);

	var nodes = div.getElementsByTagName("p");
	for (var i=0; i<nodes.length; i++) {
		var p = nodes[i];

		// Is middot
		if (p.innerHTML.indexOf(search) != -1) {
			var ul = document.createElement("ul");

			if (class_name)
				ul.className = class_name;

			// Add the first one
			var li = document.createElement("li");
			li.innerHTML = p.innerHTML.replace(new RegExp('' + mdot + '|' + bull + '|--list--|&nbsp;', "gi"), '');
			ul.appendChild(li);

			// Add the rest
			var np = p.nextSibling;
			while (np) {
				// Not element or middot paragraph
				if (np.nodeType != 1 || np.innerHTML.indexOf(search) == -1)
					break;

				var cp = np.nextSibling;
				var li = document.createElement("li");
				li.innerHTML = np.innerHTML.replace(new RegExp('' + mdot + '|' + bull + '|--list--|&nbsp;', "gi"), '');
				np.parentNode.removeChild(np);
				ul.appendChild(li);
				np = cp;
			}

			p.parentNode.replaceChild(ul, p);

			return true;
		}
	}

	return false;
}

function TinyMCE_paste__clipboardHTML() {
	var div = document.getElementById('_TinyMCE_clipboardHTML');

	if (!div) {
		var div = document.createElement('DIV');
		div.id = '_TinyMCE_clipboardHTML';

		with (div.style) {
			visibility = 'hidden';
			overflow = 'hidden';
			position = 'absolute';
			width = 1;
			height = 1;
		}

		document.body.appendChild(div);
	}

	div.innerHTML = '';
	var rng = document.body.createTextRange();
	rng.moveToElementText(div);
	rng.execCommand('Paste');
	var html = div.innerHTML;
	div.innerHTML = '';
	return html;
}
