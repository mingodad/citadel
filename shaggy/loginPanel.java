/* loginPanel.java
 * login screen
 */

import java.awt.*;

public class loginPanel extends Panel {
  TextArea	text;
  TextField	user, pass;
  Button	login;

  public loginPanel() {
    setLayout( new BorderLayout() );

    String	blurb = citadel.me.getBlurb();
    if( blurb.length() == 0 ) blurb = "Citadel";

    add( "North", new Label( blurb ) );
    add( "Center", text = new TextArea() );

    text.setFont( new Font( "Courier", Font.PLAIN, 12 ) );

    Panel	p = new Panel();
    p.setLayout( new BorderLayout() );

    PairPanel	pp = new PairPanel();

    pp.addLeft( new Label( "Username:" ) );
    pp.addRight( user = new TextField( 10 ) );
    pp.addLeft( new Label( "Password:" ) );
    pp.addRight( pass = new TextField( 10 ) );
    pass.setEchoCharacter( '.' );

    p.add( "Center", pp );

    Panel	ppp = new Panel();
    ppp.add( login = new Button( "Login" ) );

    p.add( "East", ppp );

    add( "South", p );
  }

  public boolean handleEvent( Event e ) {
    if( e.id == Event.GOT_FOCUS ) {
      if( (e.target != user) && (e.target != pass) )
	user.requestFocus();
    }
    return super.handleEvent( e );
  }

  public boolean action( Event e, Object o ) {
    if( e.target == user ) {
      pass.requestFocus();
    }
    if( (e.target == login) || (e.target == pass) ) {
      String	u = user.getText(), p=pass.getText();

      if( u.length() == 0 ) {
	text.append( "\nNeed to enter in username.\n" );
	user.requestFocus();
      } else if (p.length() == 0 ) {
	text.append( "\nNeed to enter in password.\n" );
	pass.requestFocus();
      } else {
	citReply r = citadel.me.getReply( "USER " + u );
	if( r.moreData() ) {
	  r = citadel.me.getReply( "PASS " + p );
	  if( r.error() ) {
	    text.append( "<bad password>" + r.getArg( 0 ) );
	  } else {
	    citadel.me.loggedIn( r );
	  }
	} else {
	  text.append( "<bad user>" + r.getArg( 0 ) );
	}
      }
    }
    return super.action( e, o );
  }

  public void refresh() {
    text.setText( citadel.me.getSystemMessage( "hello" ) );
    user.setText( "" );
    pass.setText( "" );
  }
}


