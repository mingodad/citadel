<?PHP
	include "ctdlheader.php";
	bbs_page_header();

	ctdl_mesg("hello");
?>

	<div align=center>
	<form action="do_login.php" method="POST">

	<table border="0" cellspacing="5" cellpadding="5" BGCOLOR="#EEEEEE">
		<tr><td>User name:</td>
		<td><input type="text" name="name" maxlength="25"></td></tr>
		<tr><td>Password:</td>
		<td><input type="password" name="pass" maxlength="20"></td></tr>

	<tr><td align=center COLSPAN=2>
	<input type="submit" name="action" value="Login">
	<input type="submit" name="action" value="New User">
	<input type="submit" name="action" value="Exit">
	</td></tr>

	</table>
	</form>
	</div>

<a href="page2.php">Page Two</a><BR>
<a href="page3.php">Page Three</a><BR>

<?PHP
	bbs_page_footer();
?>

