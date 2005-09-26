/* Import plugin specific language pack */
tinyMCE.importPluginLanguagePack('advlink', 'en,de,sv,zh_cn,cs,fa,fr_ca,fr,pl,pt_br,nl');

function TinyMCE_advlink_getInfo() {
	return {
		longname : 'Advanced link',
		author : 'Moxiecode Systems',
		authorurl : 'http://tinymce.moxiecode.com',
		infourl : 'http://tinymce.moxiecode.com/tinymce/docs/plugin_advlink.html',
		version : tinyMCE.majorVersion + "." + tinyMCE.minorVersion
	};
};

function TinyMCE_advlink_getControlHTML(control_name) {
	switch (control_name) {
		case "link":
			return '<a href="javascript:tinyMCE.execInstanceCommand(\'{$editor_id}\',\'mceAdvLink\');" onmousedown="return false;"><img id="{$editor_id}_advlink" src="{$themeurl}/images/link.gif" title="{$lang_link_desc}" width="20" height="20" class="mceButtonNormal" onmouseover="tinyMCE.switchClass(this,\'mceButtonOver\');" onmouseout="tinyMCE.restoreClass(this);" onmousedown="tinyMCE.restoreClass(this);" /></a>';
	}

	return "";
}

function TinyMCE_advlink_execCommand(editor_id, element, command, user_interface, value) {
	switch (command) {
		case "mceAdvLink":
			var anySelection = false;
			var inst = tinyMCE.getInstanceById(editor_id);
			var focusElm = inst.getFocusElement();

			if (tinyMCE.selectedElement)
				anySelection = (tinyMCE.selectedElement.nodeName.toLowerCase() == "img") || (selectedText && selectedText.length > 0);

			if (anySelection || (focusElm != null && focusElm.nodeName == "A")) {
				var template = new Array();

				template['file']   = '../../plugins/advlink/link.htm';
				template['width']  = 480;
				template['height'] = 400;

				// Language specific width and height addons
				template['width']  += tinyMCE.getLang('lang_advlink_delta_width', 0);
				template['height'] += tinyMCE.getLang('lang_advlink_delta_height', 0);

				tinyMCE.openWindow(template, {editor_id : editor_id, inline : "yes"});
			}

			return true;
	}

	return false;
}

function TinyMCE_advlink_handleNodeChange(editor_id, node, undo_index, undo_levels, visual_aid, any_selection) {
	tinyMCE.switchClassSticky(editor_id + '_advlink', 'mceButtonDisabled', true);

	if (node == null)
		return;

	if (any_selection)
		tinyMCE.switchClassSticky(editor_id + '_advlink', 'mceButtonNormal', false);

	do {
		if (node.nodeName == "A" && tinyMCE.getAttrib(node, 'href') != "")
			tinyMCE.switchClassSticky(editor_id + '_advlink', 'mceButtonSelected', false);
	} while ((node = node.parentNode));

	return true;
}
