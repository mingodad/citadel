import javax.swing.*;
import java.awt.*;
import java.awt.event.*;

public class loginPanel extends JPanel {
  JLabel	serverBlurb;
  JTextArea	message;
  JTextField	user, pass;
  JButton	login;

  public loginPanel() {
    setLayout( new BorderLayout() );
    add( "North", serverBlurb = new JLabel( "this is where you'd have server info" ) );

    JPanel	p = new JPanel();
    p.setLayout( new BorderLayout() );
    p.setBorder( BorderFactory.createTitledBorder( 
		  BorderFactory.createEtchedBorder(), "Welcome Message" ) );
    p.add( "Center", message = new JTextArea() );
    message.setLineWrap( true );
    message.setWrapStyleWord( true );
    add( "Center", p );

    p = new JPanel();
    p.setLayout( new BorderLayout() );

    PairPanel	pp = new PairPanel( 4, 4 );
    pp.addLeft( new JLabel( "User:" ) );
    pp.addRight( user = new JTextField( 10 ) );
    pp.addLeft( new JLabel( "Pass:" ) );
    pp.addRight( pass = new JPasswordField( 10 ) );

    user.addActionListener( new ActionListener() {
      public void actionPerformed( ActionEvent e ) {
	pass.requestFocus();
      }
    } );

    pass.addActionListener( new ActionListener() {
      public void actionPerformed( ActionEvent e ) {
	citadel.me.authenticate( user.getText(), pass.getText() );
      } } );

    p.add( "Center", pp );

  
    p.add( "East", login = new JButton( "Login" ) );
    login.addActionListener( new ActionListener() {
      public void actionPerformed( ActionEvent e ) {
	citadel.me.authenticate( user.getText(), pass.getText() );
      } } );

    add( "South", p );
  }

  public void refresh() {
    serverBlurb.setText( "Connecting..." );
    message.setText( "" );
    login.setEnabled( true );
    citadel.me.getServerInfo( new CallBack() {
      public void run( citReply r ) {
	server	sI = new server( r );
	serverBlurb.setText( sI.blurb );       
	citadel.me.setServerInfo( sI );
      }});
    citadel.me.getSystemMessage( "hello", new CallBack() {
      public void run( citReply r ) {
	message.setText( r.getData() );
	}
      });
  }

  public void setDefault( String user, String pass ) {
    this.user.setText( user );
    this.pass.setText( pass );
  }
}


