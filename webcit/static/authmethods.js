/*
 * Copyright 2010, the Citadel Team
 * Licensed under the GPL V3
 *
 * JavaScript functions which handle various authentication methods.
 */


/*
 * Are we logged in right now?
 */
function IsLoggedIn() {
	if ($('is_logged_in').innerHTML == "yes") {
		return 1;
	}
	else {
		return 0;
	}
}



/*
 * Wrapper script to require logging in before completing an action
 */
function GetLoggedInFirst(destination_url) {

	/* If logged in already, go directly to the destination. */
	if (IsLoggedIn()) {
		window.location = decodeURIComponent(destination_url);
		return;
	}

	p = 'push?url=' + destination_url;
	new Ajax.Request(p, { method: 'get' } );

	/* If not logged in, go modal and ask the user to log in first. */
	new Ajax.Updater(
		'md-content',
		'do_template?template=get_logged_in',
                {
                        method: 'get',
			onSuccess: function() {
				toggleModal(1);
			}
                }
        );
}


/*
 * Attempt login with username/password, called from modal dialog
 */
function ajax_try_username_and_password() {

	$('login_errmsg').innerHTML = "";
        $('ajax_username_password_form').request({
		onSuccess: function(ctdlresult) {
			if (ctdlresult.responseText.substr(0,1) == '2') {
				window.location = 'pop';
			}
			else {
				$('login_errmsg').innerHTML = ctdlresult.responseText.substr(4) ;
			}
		}
	});
}

/*
 * The user pressed a key while in the username or password box.
 * Is it the enter/return key?  Submit the form.
 */
function username_and_password_onkeypress(e) {
	if (window.event) {		/* IE */
		keynum = e.keyCode
	}
	else if (e.which) {		/* real browsers */
		keynum = e.which
	}
	if (keynum == 13) {		/* enter/return key */
		ajax_try_username_and_password();
	}
}


/*
 * tab handler for the login box
 */
function authtoggle(show_which_div) {
	$('authbox_userpass').style.display = 'none';
	$('authbox_openid').style.display = 'none';
	$(show_which_div).style.display = 'block';
}
