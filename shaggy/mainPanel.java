/* mainPanel.java
 */

import java.awt.*;

public class mainPanel extends Panel {
  List		new_msgs, seen;
  Button	next_room, goto_room;
  Button	who_is_online, page_user;
  Button	logout;

  public mainPanel() {
    setLayout( new BorderLayout() );

    Panel	p = new Panel();
    p.setLayout( new GridLayout( 1, 2 ) );

    NamedPanel	np = new NamedPanel( "New Messages" );
    np.setLayout( new BorderLayout() );
    np.add( "Center", new_msgs = new List() );

    p.add( np );

    np = new NamedPanel( "Seen Messages" );
    np.setLayout( new BorderLayout() );
    np.add( "Center", seen = new List() );
    p.add( np );

    add( "Center", p );

    p = new Panel();
    p.add( next_room = new Button ( "Next Room" ) );
    p.add( goto_room = new Button ( "Goto Room" ) );
    p.add( who_is_online = new Button( "Who is Online" ) );
    p.add( page_user = new Button( "Page User" ) );
    add( "North", p );

    p = new Panel();
    p.add( logout = new Button( "Logout" ) );
    add( "South", p );
  }

  public boolean action( Event e, Object o ) {
    if( (e.target == new_msgs) || (e.target == seen) || (e.target == goto_room)) {
      String room = getRoom();
      if( room != null )
	citadel.me.enterRoom( room );
    } else if( e.target == who_is_online ) {
      citadel.me.who_online();
    } else if( e.target == page_user ) {
      citadel.me.page_user();
    } else if (e.target == next_room ) {
      citadel.me.nextNewRoom();
    } else if( e.target == logout ) {
      citadel.me.logoff();
    } else {
      return super.action( e, o );
    }
    return true;
  }


  public boolean handleEvent( Event e ) {
    if( e.id == Event.LIST_SELECT ) {
      if( e.target == new_msgs ) {
	int	i = seen.getSelectedIndex();
	if( i != -1 )
	  seen.deselect( i );
      } else {
	int	i = new_msgs.getSelectedIndex();
	if( i != -1 )
	  new_msgs.deselect( i );
      }
    }
    return super.handleEvent( e );
  }

  public void refresh() { 
    new_msgs.clear(); 
    parseRooms( new_msgs, citadel.me.getReply( "LKRN" ) );

    seen.clear();
    parseRooms( seen, citadel.me.getReply( "LKRO" ) );
  }
	
  public void parseRooms( List l, citReply r ) {
    int		i=0;
    String	s;

    while( (s = r.getLine( i++) ) != null ) {
      int j = s.indexOf( '|' );
      if( j != -1 )
	l.addItem( s.substring( 0, j ) );
      else
	l.addItem( s );
    }
  }

  public String getRoom() {
    String s = new_msgs.getSelectedItem();
    if( s == null ) s = seen.getSelectedItem();

    return s;
  }

  public String nextNewRoom() {
    if( new_msgs.countItems() == 0 ) return "Lobby";

    return new_msgs.getItem( 0 );
  }

  public void visited( String room ) {
    for( int i = 0; i < new_msgs.countItems(); i++ ) {
      if( room.equals( new_msgs.getItem( i ) ) ) {
	new_msgs.delItem( i );
	seen.addItem( room );
      }
    }
  }
}
