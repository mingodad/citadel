/* citFrame.java
 * shell for citPanel, so we can be an applet and application
 */

import java.awt.*;

public class citFrame extends Frame {
  citPanel	cp;

  public citFrame() {
    super( "Citadel!" );

    setLayout( new BorderLayout() );

    add( "Center", cp = new citPanel() );

    resize( 400, 400 );
    show();
  }

  public boolean handleEvent( Event e ) {
    if( e.id == Event.WINDOW_DESTROY ) {
      if( citadel.me.theNet != null )
	citadel.me.theNet.println( "QUIT" );
      System.out.println( "Bye bye!" );
      System.exit( 0 );
    }
    return super.handleEvent( e );
  }
}

