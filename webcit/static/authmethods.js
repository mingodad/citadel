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
		window.location = destination_url;
		return;
	}

	/* If not logged in, go modal and ask the user to log in first. */
	p = 'do_template?template=get_logged_in?destination_url=' + destination_url;
	new Ajax.Updater(
		'md-content',
		p,
                {
                        method: 'get',
			onSuccess: function(cl_success) {
				toggleModal(1);
			}
                }
        );
}


/*
 * Attempt login with username/password, called from modal dialog
 */
function ajax_try_username_and_password(destination_url) {
	$('login_errmsg').innerHTML = "";
        $('ajax_username_password_form').request({
		onSuccess: function(ctdlresult) {
			if (ctdlresult.responseText.substr(0,1) == '2') {
				window.location = destination_url;
			}
			else {
				$('login_errmsg').innerHTML = ctdlresult.responseText.substr(4) ;
			}
		}
	});
}
