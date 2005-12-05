/* Import plugin specific language pack */
tinyMCE.importPluginLanguagePack('insertdatetime', 'cs,el,en,fr_ca,it,ko,sv,zh_cn,fa,fr,de,pl,pt_br,nl,da,he,nb,ru,ru_KOI8-R,ru_UTF-8,nn,fi,es,cy,is,pl');

function TinyMCE_insertdatetime_getInfo() {
	return {
		longname : 'Insert date/time',
		author : 'Moxiecode Systems',
		authorurl : 'http://tinymce.moxiecode.com',
		infourl : 'http://tinymce.moxiecode.com/tinymce/docs/plugin_insertdatetime.html',
		version : tinyMCE.majorVersion + "." + tinyMCE.minorVersion
	};
};

/**
 * Returns the HTML contents of the insertdate, inserttime controls.
 */
function TinyMCE_insertdatetime_getControlHTML(control_name) {
	switch (control_name) {
		case "insertdate":
			var cmd = 'tinyMCE.execInstanceCommand(\'{$editor_id}\',\'mceInsertDate\');return false;';
			return '<a href="javascript:' + cmd + '" onclick="' + cmd + '" target="_self" onmousedown="return false;"><img id="{$editor_id}_insertdate" src="{$pluginurl}/images/insertdate.gif" title="{$lang_insertdate_desc}" width="20" height="20" class="mceButtonNormal" onmouseover="tinyMCE.switchClass(this,\'mceButtonOver\');" onmouseout="tinyMCE.restoreClass(this);" onmousedown="tinyMCE.restoreAndSwitchClass(this,\'mceButtonDown\');" /></a>';

		case "inserttime":
			var cmd = 'tinyMCE.execInstanceCommand(\'{$editor_id}\',\'mceInsertTime\');return false;';
			return '<a href="javascript:' + cmd + '" onclick="' + cmd + '" target="_self" onmousedown="return false;"><img id="{$editor_id}_inserttime" src="{$pluginurl}/images/inserttime.gif" title="{$lang_inserttime_desc}" width="20" height="20" class="mceButtonNormal" onmouseover="tinyMCE.switchClass(this,\'mceButtonOver\');" onmouseout="tinyMCE.restoreClass(this);" onmousedown="tinyMCE.restoreAndSwitchClass(this,\'mceButtonDown\');" /></a>';
	}

	return "";
}

/**
 * Executes the mceInsertDate command.
 */
function TinyMCE_insertdatetime_execCommand(editor_id, element, command, user_interface, value) {
	/* Adds zeros infront of value */
	function addZeros(value, len) {
		value = "" + value;

		if (value.length < len) {
			for (var i=0; i<(len-value.length); i++)
				value = "0" + value;
		}

		return value;
	}

	/* Returns the date object in the specified format */
	function getDateTime(date, format) {
		format = tinyMCE.regexpReplace(format, "%D", "%m/%d/%y");
		format = tinyMCE.regexpReplace(format, "%r", "%I:%M:%S %p");
		format = tinyMCE.regexpReplace(format, "%Y", "" + date.getFullYear());
		format = tinyMCE.regexpReplace(format, "%y", "" + date.getYear());
		format = tinyMCE.regexpReplace(format, "%m", addZeros(date.getMonth()+1, 2));
		format = tinyMCE.regexpReplace(format, "%d", addZeros(date.getDate(), 2));
		format = tinyMCE.regexpReplace(format, "%H", "" + addZeros(date.getHours(), 2));
		format = tinyMCE.regexpReplace(format, "%M", "" + addZeros(date.getMinutes(), 2));
		format = tinyMCE.regexpReplace(format, "%S", "" + addZeros(date.getSeconds(), 2));
		format = tinyMCE.regexpReplace(format, "%I", "" + ((date.getHours() + 11) % 12 + 1));
		format = tinyMCE.regexpReplace(format, "%p", "" + (date.getHours() < 12 ? "AM" : "PM"));
		format = tinyMCE.regexpReplace(format, "%B", "" + tinyMCE.getLang("lang_inserttime_months_long")[date.getMonth()]);
		format = tinyMCE.regexpReplace(format, "%b", "" + tinyMCE.getLang("lang_inserttime_months_short")[date.getMonth()]);
		format = tinyMCE.regexpReplace(format, "%A", "" + tinyMCE.getLang("lang_inserttime_day_long")[date.getDay()]);
		format = tinyMCE.regexpReplace(format, "%a", "" + tinyMCE.getLang("lang_inserttime_day_short")[date.getDay()]);
		format = tinyMCE.regexpReplace(format, "%%", "%");

		return format;
	}

	// Handle commands
	switch (command) {
		case "mceInsertDate":
			tinyMCE.execInstanceCommand(editor_id, 'mceInsertContent', false, getDateTime(new Date(), tinyMCE.getParam("plugin_insertdate_dateFormat", tinyMCE.getLang('lang_insertdate_def_fmt'))));
			return true;

		case "mceInsertTime":
			tinyMCE.execInstanceCommand(editor_id, 'mceInsertContent', false, getDateTime(new Date(), tinyMCE.getParam("plugin_insertdate_timeFormat", tinyMCE.getLang('lang_inserttime_def_fmt'))));
			return true;
	}

	// Pass to next handler in chain
	return false;
}
