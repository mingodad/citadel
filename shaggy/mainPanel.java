/* mainPanel.java
 */

import java.awt.*;

public class mainPanel extends Panel {
  List		newL, oldL;
  Button	next_room, goto_room;
  Button	who_is_online, page_user;
  Button	logout;

  public mainPanel() {
    setLayout( new BorderLayout() );

    Panel	p = new Panel();
    p.setLayout( new GridLayout( 1, 2 ) );

    NamedPanel	np = new NamedPanel( "New Messages" );
    np.setLayout( new BorderLayout() );
    np.add( "Center", newL = new List() );

    p.add( np );

    np = new NamedPanel( "Seen Messages" );
    np.setLayout( new BorderLayout() );
    np.add( "Center", oldL = new List() );
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

    citadel.me.rooms.setList( newL, oldL );
  }

  public boolean action( Event e, Object o ) {
    if( (e.target == newL) || (e.target == oldL) || (e.target == goto_room)) {
      String room = getRoom();
      if( room != null )
	citadel.me.enterRoom( room );
    } else if( e.target == who_is_online ) {
      citadel.me.who_online();
    } else if( e.target == page_user ) {
      citadel.me.page_user();
    } else if (e.target == next_room ) {
      citadel.me.nextNewRoom();
    } else if( e.target == goto_room ) {
      citadel.me.gotoRoom( getRoom(), true );
    } else if( e.target == logout ) {
      citadel.me.logoff();
    } else {
      return super.action( e, o );
    }
    return true;
  }


  public boolean handleEvent( Event e ) {
    if( e.id == Event.LIST_SELECT ) {
      if( e.target == newL ) {
	int	i = oldL.getSelectedIndex();
	if( i != -1 )
	  oldL.deselect( i );
      } else {
	int	i = newL.getSelectedIndex();
	if( i != -1 )
	  newL.deselect( i );
      }
    }
    return super.handleEvent( e );
  }

  public void refresh() { 
    citadel.me.rooms.refresh();
  }

  public String getRoom() {
    String s = newL.getSelectedItem();
    if( s == null ) s = oldL.getSelectedItem();

    return s;
  }

}
