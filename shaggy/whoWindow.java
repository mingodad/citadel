/* whoWindow.java
 * Who is online window
 */

import java.awt.*;
import java.util.Vector;

public class whoWindow extends Frame {
  Button	refresh, page, close;
  List		users;
  Vector	list;

  public whoWindow() {
    super( "Citadel: Who is online" );

    NamedPanel	np = new NamedPanel( "Who is currently online" );
    np.setLayout( new BorderLayout() );
    np.add( "Center", users = new List() );
    users.setFont( new Font( "Courier", Font.PLAIN, 12 ) );

    Panel	p = new Panel();
    p.add( refresh = new Button( "Refresh" ) );
    p.add( page = new Button( "Page" ) );
    p.add( close = new Button( "Close" ) );

    add( "Center", np );
    add( "South", p );

    refresh();

    resize( 400, 300 );
    show();
  }

  public boolean handleEvent( Event e ) {
    if( e.id == Event.WINDOW_DESTROY ) {
      dispose();
      citadel.me.wo = null;
    }
    return super.handleEvent( e );
  }

  public boolean action( Event e, Object o ) {
    if( e.target == close ) {
      dispose();
      citadel.me.wo = null;
    } else if( (e.target == page) || (e.target == users) ) {
      int	i = users.getSelectedIndex();
      try {
	String	user = (String)list.elementAt( i );
	citadel.me.page_user( user );
      } catch( Exception exp ) {}
    } else if( e.target == refresh ) {
      refresh();
    }
    return super.action( e, o );
  }

  public void refresh() {
    list = new Vector();
    users.clear();

    citReply	r = citadel.me.getReply( "RWHO" );
    int		i = 0;
    String	s;
    while( (s = r.getLine( i++ )) != null ) {
      int	j = s.indexOf( '|' ) + 1;
      int	k = s.indexOf( '|', j );
      int	l = s.indexOf( '|', k + 1 );
      String	user = s.substring( j, k );
      String	room = s.substring( k+1, l );
      list.addElement( user );
      users.addItem( pad( user, room ) );
    }
  }

  public String pad( String u, String r ) {
    StringBuffer	s = new StringBuffer( u );
    while( s.length() < 30 )
      s.append( ' ' );
    s.append( r );
    return s.toString();
  }
}
