/* enterPanel.java
 * For entering messages
 */

import java.awt.*;

public class enterPanel extends Panel {
  boolean	mail;
  String	room, recip;
  NamedPanel	np;

  TextField	who;
  TextArea	msg;

  Button	ok, cancel;

  public enterPanel() {
    PairPanel	pp = new PairPanel();
    pp.addLeft( new Label( "Recipient : " ) );
    pp.addRight( who = new TextField( 10 ) );

    setLayout( new BorderLayout() );
    add( "North", pp );

    np = new NamedPanel( "room name" );
    np.setLayout( new BorderLayout() );
    np.add( "Center", msg = new TextArea() );

    add( "Center", np );

    Panel p = new Panel();

    p.add( ok = new Button ( "Send" ) );
    p.add( cancel = new Button ( "Cancel" ) );
    add( "South", p );
    mail = false;
  }

  public void refresh( String room, String r ) {
    if( room.equalsIgnoreCase( "Mail" ) ) {
      who.enable();
      who.setText( r );
      if( r == null )
	who.requestFocus();
      else
	msg.requestFocus();
      mail = true;
    } else {
      who.disable();
      who.setText( "DISABLED" );
      msg.requestFocus();
      mail = false;
    }

    msg.setText( "" );

    np.setLabel( room + " (" + citadel.me.rooms.getRoomsFloorName( room  )+")" );
    this.room = room;
    this.recip = r;

    msg.enable();
    if( !mail ) {
      citReply rep = citadel.me.getReply( "ENT0 0|0|0|0|0" );
      if( !rep.ok() ) {
	msg.setText( "You aren't allowed to post here." );
	msg.disable();
      }
    }
  }

  public boolean action( Event e, Object o ) {
    if( e.target == ok ) {
      citadel.me.sendMessage( msg.getText(), who.getText(), mail );
      citadel.me.showMsgPane();
    } else if( e.target == cancel ) {
      citadel.me.showMsgPane();
      msg.requestFocus();
    }
    return super.action( e, o );
  }
}
