/* messagePanel.java
 */

import java.awt.*;
import java.util.*;

public class messagePanel extends Panel {
  Choice	reading;
  Button	who_is_online, room_info;
  Button	next_room, goto_room, page_user;
  Button	next_msg, prev_msg, enter_msg, zap_room, back;
  TextField	msgInfo;
  TextArea	theMsg;
  NamedPanel	np;

  Vector	msgs;
  int		cur_pos = -1;

  String	name;
  int		total, unread, info, flags, highest, highest_read;
  boolean	mail, aide;
  int		mail_num, floor;

  public messagePanel() {
    setLayout( new BorderLayout() );

    VertPanel	vp = new VertPanel();
    vp.add( reading = new Choice() );
    reading.addItem( "Read New" );
    reading.addItem( "Read All" );
    reading.addItem( "Last 5" );
    reading.select( 0 );

    vp.add( next_msg = new Button( "Next Message" ) );
    vp.add( prev_msg = new Button( "Prev Message" ) );
    vp.add( enter_msg = new Button( "Enter Message" ) );
    vp.add( next_room = new Button( "Next Room" ) );
    vp.add( goto_room = new Button( "Goto Room" ) );
    vp.add( room_info = new Button( "Room Info" ) );
    vp.add( zap_room = new Button( "Zap Room" ) );
    vp.add( who_is_online = new Button( "Who is Online" ) );
    vp.add( page_user = new Button( "Page User" ) );
    vp.add( back = new Button( "Back" ) );

    add( "West", vp );

    np = new NamedPanel( "Message" );
    np.setLayout( new BorderLayout() );
    np.add( "North", msgInfo = new TextField() );
    np.add( "Center", theMsg = new TextArea() );
    add( "Center", np );

    /*    Panel p = new Panel();
	  add( "South", p );*/
  }

  public boolean action( Event e, Object o ) {
    if( e.target == reading ) {
      getMsgsPtrs();
    } else if( e.target == page_user ) {
      citadel.me.page_user();
    } else if( e.target == who_is_online ) {
      citadel.me.who_online();
    } else if( e.target == room_info ) {
      new displayInfo( name );
    } else if( e.target == next_msg ) {
      cur_pos++;
      displayMessage();
    } else if( e.target == prev_msg ) {
      cur_pos--;
      displayMessage();
    } else if( e.target == enter_msg ) {
      citadel.me.enterMsg( name );
    } else if( e.target == next_room ) {
      citadel.me.nextNewRoom();
    } else if( e.target == goto_room ) {
      citadel.me.gotoRoom();
    } else if( e.target == zap_room ) {
      new promptWindow( new zapPrompt( name ) );
    } else if( e.target == back ) {
      citadel.me.mainMenu();
    }
    return super.action( e, o );
  }

  public void refresh( citReply r ) {
    name = r.getArg( 0 );
    np.setLabel( name + (citadel.me.floors() ? 
		 " (" + citadel.me.rooms.getRoomsFloorName( name )+")" : "" ) );
    total = citadel.atoi( r.getArg( 1 ) );
    unread = citadel.atoi( r.getArg( 2 ) );
    info = citadel.atoi( r.getArg( 3 ) );
    flags = citadel.atoi( r.getArg( 4 ) );
    highest = citadel.atoi( r.getArg( 5 ) );
    highest_read = citadel.atoi( r.getArg( 6 ) );
    mail = citadel.atoi( r.getArg( 7 ) ) != 0;
    aide = citadel.atoi( r.getArg( 8 ) ) != 0;
    mail_num = citadel.atoi( r.getArg( 9 ) ); 
    floor = citadel.atoi( r.getArg( 10 ) );

    msgInfo.setText( "" );
    theMsg.setText( "" );

    if( info != 0 ) new displayInfo( name );
    getMsgsPtrs();
  }

  public void getMsgsPtrs() {
    msgs = null;
    String	which = "new";
    if( reading.getSelectedIndex() == 1 ) which = "all";
    else if( reading.getSelectedIndex() == 2 ) which = "last|5";
    citReply	r = citadel.me.getReply( "MSGS " + which );
    if( !r.error() ) {
      msgs = r.listing;
      cur_pos = 0;
    }
    displayMessage();
  }

  public void displayMessage() {
    if( msgs == null) { 
      msgInfo.setText( "<no messages>" );
      theMsg.setText( "" );
      return;
    }

    if( cur_pos <= 0 ) {
      cur_pos = 0;
      prev_msg.disable();
    } else
      prev_msg.enable();

    if( cur_pos >= msgs.size()-1 ) {
      cur_pos = msgs.size()-1;
      next_msg.disable();
    } else
      next_msg.enable();

    String	num = (String)msgs.elementAt( cur_pos );

    citReply	r = citadel.me.getReply( "MSG0 " + num + "|0" );
    if( r.error() ) {
      msgInfo.setText( "<error>" );
      theMsg.setText( r.getArg(0) );
      return;
    }

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

    String	sum = (cur_pos+1) + "/" + msgs.size() + " " + time + " from " + from;
    if( !node.equals( citadel.me.serverInfo.human_name ) )
      sum = sum + " (@"+node+")";
    if( !rcpt.equals( "" ) )
      sum = sum + " to " + rcpt;
    if( !room.equals(name) )
      sum = sum + " in " + room;
   
    msgInfo.setText( sum );
    theMsg.setText( r.getData() ); /* this relies on the fact that we've removed the header lines above.  probably a messy way to deal with references. */

    int	n = citadel.atoi( num );
    if( n > highest_read ) {
      highest_read = n;
      citadel.me.getReply( "SLRP " + num );
    }
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
						  
}
