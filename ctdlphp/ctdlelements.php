<?PHP

// $Id$
// 
// Translates Citadel protocol data to insertable HTML snippets.  You can
// use these directly, or you can bypass them completely.
//
// Copyright (c) 2003 by Art Cancro <ajc@uncensored.citadel.org>
// This program is released under the terms of the GNU General Public License.


//
// Fetch and display a message.
//
function display_message($msgnum) {

	echo '<TABLE border=0 width=100% bgcolor="#DDDDDD"><TR><TD>' ;
	// Fetch the message from the server
	list($ok, $response, $fields) = ctdl_fetch_message($msgnum);

	// Bail out gracefully if the message isn't there.
	if (!$ok) {
		echo "Error: " . htmlspecialchars($response) . "<BR>" ;
		echo '</TD></TR></TABLE>' ;
		return;
	}

	// Begin header
	echo "<B><I>" .
		strftime("%b %d %Y %I:%M%p ", $fields["time"]) .
		" from " . htmlspecialchars($fields["from"]) .
		"</I></B><BR>\n" ;

	// Do message text
	echo $fields["text"] . "<BR>";

	echo '</TD></TR></TABLE><BR>' ;
}



?>
