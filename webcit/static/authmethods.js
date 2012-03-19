/*
 * Copyright 2010-2011, the Citadel Team
 * Licensed under the GPL V3
 *
 * JavaScript functions which handle various authentication methods.
 */



/****************** COMMON CODE ***********************/


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
 * tab handler for the login box
 */
function authtoggle(show_which_div) {
	$('authbox_userpass').style.display = 'none';
	$('authbox_newuser').style.display = 'none';
	$('authbox_openid').style.display = 'none';
	$('authbox_google').style.display = 'none';
	$('authbox_yahoo').style.display = 'none';
	$('authbox_aol').style.display = 'none';
	$('authbox_success').style.display = 'none';
	$(show_which_div).style.display = 'block';
}


/*
 * Pop out a window for external auth methods
 * (most of them don't handle inline auth very well)
 */
function do_auth_popout(popout_url) {
	window.open(popout_url, "authpopout", "status=1,toolbar=0,width=600,height=400");
}




/****************** USERNAME AND PASSWORD ***********************/

/*
 * Attempt login with username/password, called from modal dialog
 */
function ajax_try_username_and_password() {

	$('login_errmsg').innerHTML = "";
	authtoggle('authbox_success');
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


/****************** REGISTER NEW USER ***********************/

/*
 * Attempt to create a new local username/password, called from modal dialog
 */
function ajax_try_newuser() {

	$('login_errmsg').innerHTML = "";
        $('ajax_newuser_form').request({
		onSuccess: function(ctdlresult) {
			if (ctdlresult.responseText.substr(0,1) == '2') {
				authtoggle('authbox_success');
				window.location = 'pop';
			}
			else {
				$('login_errmsg').innerHTML = ctdlresult.responseText.substr(4) ;
			}
		}
	});
}


/*
 * The user pressed a key while in the newuser or newpassword box.
 * Is it the enter/return key?  Submit the form.
 */
function newuser_onkeypress(e) {
	if (window.event) {		/* IE */
		keynum = e.keyCode;
	}
	else if (e.which) {		/* real browsers */
		keynum = e.which;
	}
	if (keynum == 13) {		/* enter/return key */
		ajax_try_newuser();
	}
}




/****************** OPENID ***********************/

/*
 * Attempt login with OpenID, called from modal dialog
 */
function ajax_try_openid() {
	$('login_errmsg').innerHTML = "";
	openid_url = encodeURI($('ajax_openid_form').elements["openid_url"].value);
	do_auth_popout("openid_login?openid_url=" + openid_url);
}


/*
 * The user pressed a key while in the openid login box.
 * Is it the enter/return key?  Submit the form.
 */
function openid_onkeypress(e) {
	if (window.event) {		/* IE */
		keynum = e.keyCode;
	}
	else if (e.which) {		/* real browsers */
		keynum = e.which;
	}
	if (keynum == 13) {		/* enter/return key */
		ajax_try_openid();
		return false;
	}
	return true;
}


/****************** GOOGLE ***********************/

/*
 * Attempt login with Google, called from modal dialog
 */
function ajax_try_google() {
	$('login_errmsg').innerHTML = "";
	openid_url = encodeURI("https://www.google.com/accounts/o8/id");
	do_auth_popout("openid_login?openid_url=" + openid_url);
}


/****************** GOOGLE ***********************/

/*
 * Attempt login with Yahoo, called from modal dialog
 */
function ajax_try_yahoo() {
	$('login_errmsg').innerHTML = "";
	openid_url = encodeURI("http://yahoo.com");
	do_auth_popout("openid_login?openid_url=" + openid_url);
}


/****************** AOL ***********************/

/*
 * Attempt login with AOL, called from modal dialog
 */
function ajax_try_aol() {
	$('login_errmsg').innerHTML = "";
	openid_url = encodeURI($('ajax_aol_form').elements["aol_screenname"].value);
	do_auth_popout("openid_login?openid_url=http://openid.aol.com/" + openid_url);
}


/*
 * The user pressed a key while in the AOL login box.
 * Is it the enter/return key?  Submit the form.
 */
function aol_onkeypress(e) {
	if (window.event) {		/* IE */
		keynum = e.keyCode;
	}
	else if (e.which) {		/* real browsers */
		keynum = e.which;
	}
	if (keynum == 13) {		/* enter/return key */
		ajax_try_aol();
		return false;
	}
	return true;
}


