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
      mail = true;
    } else {
      who.disable();
      who.setText( "DISABLED" );
      mail = false;
    }

    msg.setText( "" );

    np.setLabel( room );
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
      citadel.me.cp.deck.show( citadel.me.cp, "Message" ); // Umm... this is bad
    } else if( e.target == cancel ) {
      citadel.me.cp.deck.show( citadel.me.cp, "Message" ); // Umm... this is bad    } else if( e.taget == who ) {
      msg.requestFocus();
    }
    return super.action( e, o );
  }
}
