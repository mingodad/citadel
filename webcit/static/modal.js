var focusedElement = null;

var modal = document.getElementById('modal');
var dialog = document.getElementById('dialog');
var body = document.getElementById('global');
var html = document.documentElement;

var modalShowing = (html.className === 'modal');


// Have to hack for Safari, due to poor support for the focus() function.
try {
	var isSafari = window.navigator.vendor.match(/Apple/);
} catch (ex) {
	isSafari = false;
}
if ( isSafari ) {
	var dialogFocuser = document.createElement('a');
	dialogFocuser.href="#";
	dialogFocuser.style.display='block';
	dialogFocuser.style.height='0';
	dialogFocuser.style.width='0';
	dialogFocuser.style.position = 'absolute';
	dialog.insertBefore(dialogFocuser, dialog.firstChild);
} else {
	dialogFocuser = dialog;
}

window.onunload = function () {
	dialogFocuser = focusedElement = modal = dialog = body = html = null;
};

var onfocus = function (e) {
	e = e || window.event;
	var el = e.target || e.srcElement;

	// save the last focused element when the modal is hidden.
	if ( !modalShowing ) {
		focusedElement = el;
		return;
	}
	
	// if we're focusing the dialog, then just clear the blurring flag.
	// else, focus the dialog and prevent the other event.
	var p = el.parentNode;
	while ( p && p.parentNode && p !== dialog ) {
		p=p.parentNode;
	}
	if ( p !== dialog ) {
		dialogFocuser.focus();
	}
};



var onblur = function () {
	if ( !modalShowing ) {
		focusedElement = body;
	}
};

html.onfocus = html.onfocusin = onfocus;
html.onblur = html.onfocusout = onblur;
if ( isSafari ) {
	html.addEventListener('DOMFocusIn',onfocus);
	html.addEventListener('DOMFocusOut',onblur);
}
// focus and blur events are tricky to bubble.
// need to do some special stuff to handle MSIE.


function toggleModal (b) {

	if (modalShowing && b) return;
	if (!modalShowing && !b) return;
	
	html.className=modalShowing?'':'modal';

	modalShowing = !modalShowing;

	if (modalShowing) {
		dialog.focus();
	} else if (focusedElement) {
		try {
			focusedElement.focus();
		} catch(ex) {}
	}
	
};
