/* promptWindow.java
 * Generic framework for prompting users for stuff
 */

import java.awt.*;

public class promptWindow extends Frame {
  public static int	ONE_FIELD=1, TWO_FIELD=2, YES_NO=3;

  promptCmd	cmd;

  /* fields */
  TextField	text1, text2;
  Button	go, stop;

  /* Yes/No */
  Button	yes, no;

  public promptWindow( promptCmd	cmd ) {
    super( cmd.getTitle() );

    this.cmd = cmd;

    go = stop = yes = no = null;
    text1 = text2 = null;

    setLayout( new BorderLayout() );
    add( "North", new Label( cmd.getPrompt() ) );

    Panel	p = new Panel();

    if( cmd.getType() != YES_NO ) {
      PairPanel pp = new PairPanel();
      add( "Center", pp );
      pp.addLeft( new Label( cmd.firstPrompt() ) );
      pp.addRight( text1 = new TextField(10) );
      if( !cmd.firstEcho() )
	text1.setEchoCharacter( '.' );
      if( cmd.getType() == TWO_FIELD ) {
	pp.addLeft( new Label( cmd.secondPrompt() ) );
	pp.addRight( text2 = new TextField(10) );
	if( cmd.secondEcho() )
	  text2.setEchoCharacter( '.' );
      }

      p.add( go = new Button( cmd.affirm() ) );
      p.add( stop = new Button( cmd.negate() )  );
      add( "South", p );
    } else {
      p.add( yes = new Button( "Yes" ) );
      p.add( no = new Button( "No" ) );
      add( "Center", p );
    }

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
    if( (e.target == stop) || (e.target == no) ) {
      dispose();

    } else  if (e.target == yes ) {
      dispose();
      if( cmd.getType() == YES_NO )
	cmd.yes();

    } else if( e.target == go ) {
      String	t1 = null, t2 = null;

      if( text1 != null ) t1 = text1.getText();
      else if( text2 != null ) t2 = text2.getText();

      dispose();
      if( cmd.getType() == ONE_FIELD )
	cmd.one_field( t1 );
      else
	cmd.two_fields( t1, t2 );
    }
    return super.action( e, o );
  }
}
