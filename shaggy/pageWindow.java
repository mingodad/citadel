/* pageWindow.java
 */

import java.awt.*;

public class pageWindow extends Frame {
  String	user;
  Choice	who;
  TextField	msg;
  Button	send, cancel;

  public pageWindow( String user ) {
    this.user = user;

    setTitle( "Page a user" );
    setLayout( new BorderLayout() );

    PairPanel	pp = new PairPanel();
    pp.addLeft( new Label( "Send message to:" ) );
    pp.addRight( who = new Choice() );

    add( "North", pp );

    NamedPanel	np = new NamedPanel( "Message" );
    np.setLayout( new BorderLayout() );
    np.add( "Center", msg = new TextField() );

    add( "Center", np );

    Panel p = new Panel();
    p.add( send = new Button( "Send" ) );
    p.add( cancel = new Button( "Cancel" ) );
    add( "South", p );

    citReply	r = citadel.me.getReply( "RWHO" );
    int	i=0, which=0;
    String	s;
    while( (s=r.getLine( i++ )) != null ) {
      String u = getUser(s);
      if( u.equalsIgnoreCase( user ) ) which = i-1;
      who.addItem( u );
    }

    who.select( which );

    resize( 300, 150 );
    show();
  }

  public String getUser( String s ) {
    int	i = s.indexOf( '|' )+1;
    int j = s.indexOf( '|', i );
    return s.substring( i, j );
  }

  public boolean handleEvent( Event e ) {
    if( e.id == Event.WINDOW_DESTROY )
      dispose();
    return super.handleEvent( e );
  }

  public boolean action( Event e, Object o ) {
    if( (e.target == msg) || (e.target == send) ) {
      String user = who.getSelectedItem();
      String m = msg.getText();
      if( m.length() > 0 )
	 citadel.me.getReply( "SEXP " + user + "|" + m );
      dispose();
    } else if( e.target == cancel )
      dispose();
    return super.action( e, o );
  }
}
