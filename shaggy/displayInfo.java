/* displayInfo.java
 * Display room's info
 */

import java.awt.*;

public class displayInfo extends Frame {
  String	name;
  TextArea	text;

  public displayInfo( String name ) {
    super( "Info for : " + name );

    this.name = name;
    setLayout( new BorderLayout() );

    add( "Center", text = new TextArea() );
    Panel p;
    add( "South", p = new Panel() );
    p.add( new Button( "Close" ) );

    citReply	r = citadel.me.getReply( "RINF" );
    if( r.listingFollows() )
      text.append( r.getData() );
    else
      text.append( "<no room info>" );
    resize( 300, 300 );
    show();
  }

  public boolean handleEvent( Event e ) {
    if( e.id == Event.WINDOW_DESTROY )
      dispose();
    return super.handleEvent( e );
  }

  public boolean action( Event e, Object o ) {
    if( e.target instanceof Button )
      dispose();
    return super.action( e, o );
  }
}
