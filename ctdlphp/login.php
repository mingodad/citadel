<?PHP

// $Id$
//
// Initial start page for PHP-based Citadel web sessions.
//
// Copyright (c) 2003 by Art Cancro <ajc@uncensored.citadel.org>
// This program is released under the terms of the GNU General Public License.

	include "ctdlheader.php";
	bbs_page_header();

	echo ctdl_mesg("hello");
?>

	<div align=center>
	<form action="do_login.php" method="POST">

	<table border="0" cellspacing="5" cellpadding="5" BGCOLOR="#EEEEEE">
		<tr><td>User name:</td>
		<td><input type="text" name="user" maxlength="25"></td></tr>
		<tr><td>Password:</td>
		<td><input type="password" name="pass" maxlength="20"></td></tr>

	<tr><td align=center COLSPAN=2>
	<input type="submit" name="action" value="Login">
	<input type="submit" name="action" value="New User">
	<a href="logout.php">Exit</A>
	</td></tr>

	</table>
	</form>
	</div>

<?PHP
	bbs_page_footer();
?>
