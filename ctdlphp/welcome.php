<?PHP
	include "ctdlheader.php";
	bbs_page_header();
?>

<h1>You're online!</h1>
<h2><?PHP echo "Welcome to ", $_SESSION["serv_humannode"]; ?></h2>
<h3><?PHP echo "You're in: ", $_SESSION["room"]; ?></h3>

<p>This is a sample welcome.php page.  Someone with mad HTML kung-fu should
edit this and make it useful and attractive.</p>

<P>Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Vestibulum
ante ipsum primis in faucibus orci luctus et ultrices posuere cubilia Curae;
Praesent nec massa. Nam enim est, semper sed, ultrices ac, malesuada et,
odio. Sed sit amet turpis at enim venenatis hendrerit. Vivamus malesuada,
ipsum et condimentum consectetuer, sapien elit suscipit lorem, vitae
pellentesque dolor arcu in dui. Aliquam erat volutpat. Cras libero.
Vestibulum convallis, neque quis accumsan viverra, nisl dolor molestie diam,
ut consequat tellus augue sed augue. Praesent faucibus blandit est. Morbi
feugiat laoreet orci. Nunc eu elit id urna vehicula sollicitudin. Nam elit
lorem, mattis et, interdum eu, bibendum sed, tortor. Mauris eu metus. Fusce
tellus tortor, vehicula at, iaculis vitae, adipiscing quis, sapien. Lorem
ipsum dolor sit amet, consectetuer adipiscing elit.</p>

<b>Sample links</b><BR>
<a href="who.php">Who is online?</a><BR>
<a href="listrooms.php">room list</A><BR>
<a href="page2.php">Page Two</a><BR>
<a href="page3.php">Page Three</a><BR>

<?PHP
	bbs_page_footer();
?>
