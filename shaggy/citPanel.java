/* citPanel.java
 * The daddy of the mack daddy.
 */

import java.awt.*;

public class citPanel extends Panel {
  loginPanel	lp;
  mainPanel	mp;
  messagePanel	msgp;
  enterPanel	ep;
  logoffPanel	offP;

  CardLayout	deck;

  public  citPanel() {
    setLayout( deck = new CardLayout() );

    add( "Login", lp = new loginPanel() );
    add( "Main", mp = new mainPanel() );
    add( "Message", msgp = new messagePanel() );
    add( "Enter", ep = new enterPanel() );
    add( "Logoff", offP = new logoffPanel() );

    citadel.me.cp = this;
    login();
  }

  public void login() {
    deck.show( this, "Login" );
    lp.refresh();
  }

  public void mainMenu() {
    deck.show( this, "Main" );
    mp.refresh();
  }

  public void enterRoom( citReply r ) {
    deck.show( this, "Message" );
    msgp.refresh( r );
  }

  public void enterMsg( String room ) {
    enterMsg( room, null );
  }

  public void enterMsg( String room, String recip ) {
    deck.show( this, "Enter" );
    ep.refresh( room, recip );
  }

  public void logoff( String err ) {
    offP.refresh( err );
    deck.show( this, "Logoff" );
  }
}
