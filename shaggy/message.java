import java.util.*;

public class message {
    String	blurb;
    String	message;

    public message( roomInfo ri, int count, int cur_pos, citReply r ) {
	Vector	msg = r.listing;

	String	s, from="", time="", room="", node="", rcpt="";
	do {
	    s = (String)msg.firstElement();
	    msg.removeElementAt( 0 );
	    from = begin( from, s, "from=" );
	    time = begin( time, s, "time=" );
	    room = begin( room, s, "room=" );
	    node = begin( node, s, "hnod=" );
	    rcpt = begin( rcpt, s, "rcpt=" );
	} while( (msg.size() > 0) && (!s.equals( "text" )) );

	time = makeDate( time );

	blurb = (cur_pos+1) + "/" + count + " " + time + " from " + from;
	//	if( !node.equals( citadel.me.serverInfo.human_name ) )
	if( ri.net )
	    blurb = blurb + " (@"+node+")";
	if( !rcpt.equals( "" ) )
	    blurb = blurb + " to " + rcpt;
	if( !room.equals( ri.name) )
	  blurb = blurb + " in " + room;
   
	/* this relies on the fact that we've removed
	   the header lines above.  probably a messy way 
	   to deal with references. */
	message = r.getData(); 
    }

    public String makeDate( String time ) {
	long	t=0;
	try {
	    t = Long.parseLong( time );
	} catch( NumberFormatException nfe ) {
	};

	Date	d = new Date( t*1000 );
	return d.toLocaleString();
    }

    public String begin( String def, String data, String key ) {
	if( data.indexOf( key ) == 0 )
	    return data.substring( key.length() );
	return def;
    }

    public String getBlurb() { return blurb; }
    public String getText() { return message; }
}


