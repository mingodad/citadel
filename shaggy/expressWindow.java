/* expressWindow.java
 * for showing express messages...
 */

import java.awt.*;

public class expressWindow extends Frame {
  String	user;
  TextArea	msg;
  Button	reply, ok;

  public expressWindow( citReply r ) {
    user = r.getArg( 3 );
    
    setTitle( user + " : express message" );

    setLayout( new BorderLayout() );
    NamedPanel	np = new NamedPanel( "Message" );

    np.setLayout( new BorderLayout() );
    np.add( "Center", msg = new TextArea() );
    msg.append( r.getData() );

    add( "Center", np );

    Panel p = new Panel();
    p.add( reply = new Button( "Reply" ) );
    p.add( ok = new Button( "OK" ) );
    add( "South", p );

    resize( 300, 300 );
    show();
  }

  public boolean handleEvent( Event e ) {
    if( e.id == Event.WINDOW_DESTROY )
      dispose();
    return super.handleEvent( e );
  }

  public boolean action( Event e, Object o ) {
    if( e.target == reply ) {
      dispose();
      citadel.me.page_user( user );
    } else if( e.target == ok )
      dispose();
    return super.action( e, o );
  }
}
