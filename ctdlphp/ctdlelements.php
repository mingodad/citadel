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
	echo "<B><I>" ;
	echo strftime("%b %d %Y %I:%M%p ", $fields["time"]) ;
	echo " from " . htmlspecialchars($fields["from"]) ;
	
	if (isset($fields["rfca"]) && strlen($fields["rfca"]) > 0) {
		echo " &lt;" . htmlspecialchars($fields["rfca"]) . "&gt;" ;
	}
	else if ( (strlen($fields["node"]) > 0) 
	     && (strcasecmp($fields["node"], $_SESSION["serv_nodename"])) ) {
		echo " @" . htmlspecialchars($fields["node"]) ;
		if (strlen($fields["hnod"]) > 0) {
			echo " (" . htmlspecialchars($fields["hnod"]) . ")" ;
		}
	}

	if (isset($fields["rcpt"]) && strlen($fields["rcpt"]) > 0) {
		echo " to " . htmlspecialchars($fields["rcpt"]) ;
	}
	echo "</I></B><BR>\n" ;

	// Subject
	if (strlen($fields["subj"]) > 0) {
		echo"<i>Subject: " .
			htmlspecialchars($fields["subj"]) .
			"</i><BR>\n" ;
	}

	// Do message text
	if ($fields['formatet_text'] != "")
		echo $fields['formatet_text'] . "<BR>";
	else 
		echo $fields["text"] . "<BR>";

	echo '</TD></TR></TABLE><BR>' ;
}

function get_message_partlist($msgnum) {

	// Fetch the message from the server
	list($ok, $response, $fields) = ctdl_fetch_message($msgnum);

	// Bail out gracefully if the message isn't there.
	if (!$ok) {
		echo "Error: " . htmlspecialchars($response) . "<BR>" ;
		return false;
	}
	if (isset($fields['part']))
	{
		$parts = explode('|', $fields['part']);
		print_r($parts);
		return $parts;

	}
	return false;
}



?>
