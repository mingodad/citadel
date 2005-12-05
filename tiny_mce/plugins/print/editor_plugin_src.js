/* Import theme	specific language pack */
tinyMCE.importPluginLanguagePack('print', 'en,sv,zh_cn,fa,fr_ca,fr,de,pl,pt_br,cs,nl,da,he,nb,hu,ru,ru_KOI8-R,ru_UTF-8,nn,fi,es,cy,is,zh_tw,zh_tw_utf8,sk');

function TinyMCE_print_getInfo() {
	return {
		longname : 'Print',
		author : 'Moxiecode Systems',
		authorurl : 'http://tinymce.moxiecode.com',
		infourl : 'http://tinymce.moxiecode.com/tinymce/docs/plugin_print.html',
		version : tinyMCE.majorVersion + "." + tinyMCE.minorVersion
	};
};

function TinyMCE_print_getControlHTML(control_name)	{
	switch (control_name) {
		case "print":
			var cmd = 'tinyMCE.execInstanceCommand(\'{$editor_id}\',\'mcePrint\',true);return false;';
			return '<a href="javascript:' + cmd + '" onclick="' + cmd + '" target="_self" onmousedown="return false;"><img id="{$editor_id}_print" src="{$pluginurl}/images/print.gif" title="{$lang_print_desc}" width="20" height="20" class="mceButtonNormal" onmouseover="tinyMCE.switchClass(this,\'mceButtonOver\');" onmouseout="tinyMCE.restoreClass(this);" onmousedown="tinyMCE.restoreAndSwitchClass(this,\'mceButtonDown\');" /></a>';
	}

	return "";
}

/**
 * Executes	the	search/replace commands.
 */
function TinyMCE_print_execCommand(editor_id, element, command,	user_interface,	value) {
	// Handle commands
	switch (command) {
		case "mcePrint":
			tinyMCE.getInstanceById(editor_id).contentWindow.print();
			return true;
	}

	// Pass to next handler in chain
	return false;
}
