<?PHP

// $Id$
//
// Log out the user and destroy the session.
//
// Copyright (c) 2003 by Art Cancro <ajc@uncensored.citadel.org>
// This program is released under the terms of the GNU General Public License.

	include "ctdlheader.php";

	bbs_page_header();

	echo ctdl_mesg("goodbye");

	echo "<a href=\"login.php\">Log in again</a><BR>\n" ;

	bbs_page_footer();
	ctdl_end_session();
?>
