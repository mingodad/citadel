/* passwordWindow.java
 * Prompt user for room password
 */

import java.awt.*;

public class passwordWindow extends Frame {
  String	room;
  TextField	text;

  public passwordWindow( String room ) {
    super( "Password for: " + room );

    this.room = room;

    setLayout( new BorderLayout() );
    add( "North", new Label( "Please enter password for " + room ) );

    PairPanel	pp = new PairPanel();
    pp.addLeft( new Label ( "Password: " ) );
    pp.addRight( text = new TextField() );
    text.setEchoCharacter( '.' );

    add( "South", new Button( "Goto!" ) );

    resize( 250, 150 );
    show();
  }

  public boolean handleEvent( Event e ) {
    if( e.id == Event.WINDOW_DESTROY ) {
      dispose();
    }
    return super.handleEvent( e );
  }

  public boolean action( Event e, Object o ) {
    if( (e.target == text) || (e.target instanceof Button) ) {
      String	s = text.getText();
      citReply	r = citadel.me.getReply( "GOTO " + room + " " + s );
      if( r.ok() ) {
	citadel.me.enterRoom( r );
	dispose();
      } else {
	dispose();
	new passwordWindow( room );
      }
    }
    return super.action( e, o );
  }
}
