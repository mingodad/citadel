/* logoffPanel.java
 * Display the "goodbye" message
 */

import java.awt.*;

public class logoffPanel extends Panel {
  TextArea	area;
  TextField	host;
  Button	connect,close;
  boolean	applet;

  public logoffPanel() {
    applet = citadel.me.applet;
    setLayout( new BorderLayout() );

    NamedPanel	np = new NamedPanel("Disconnected");
    np.setLayout( new BorderLayout() );

    np.add( "Center", area = new TextArea() );
    area.setFont( new Font( "Courier", Font.PLAIN, 12 ) );

    add( "Center", np );

    Panel	p = new Panel();

    if( !applet ) {
      p.add( host = new TextField(20) );
      host.setText( citadel.me.host );
    }

    p.add( connect = new Button ( "Connect" ) );

    if( !applet )
      p.add( close = new Button( "Close" ) );

    add( "South", p );
  }

  public boolean action ( Event e, Object o ) {
    if( (e.target == connect) || (e.target == host) ) {
      if( !applet )
	citadel.me.host = host.getText();

      citadel.me.openConnection( citadel.me.host );
      citadel.me.cp.login();
    } else if ( e.target == close ) {
      System.out.println( "Thanks!" );
      System.exit( 0 );
    }
    return super.action( e, o );
  }

  public void refresh( String s ) {
    if( s == null )
      area.setText( citadel.me.getSystemMessage( "goodbye" ) );
    else
      area.setText( s );
  }
}
