import javax.swing.*;
import java.awt.*;
import java.awt.event.*;

public class logoffPanel extends JPanel {
  JLabel	serverBlurb;
  JTextArea	message;

  public logoffPanel() {
    setLayout( new BorderLayout() );
    add( "North", serverBlurb = new JLabel( "blah" ) );

    JPanel	p = new JPanel();
    p.setLayout( new BorderLayout() );
    p.setBorder( BorderFactory.createTitledBorder( 
		  BorderFactory.createEtchedBorder(), "Goodbye Message" ) );
    p.add( "Center", message = new JTextArea() );
    message.setLineWrap( true );
    message.setWrapStyleWord( true );

    add( "Center", p );

    p = new JPanel();
    /*     p.setLayout( new BorderLayout() ); */
    JButton	b;
    p.add( b = new JButton( "OK" ) );
    b.addActionListener( new ActionListener() {
      public void actionPerformed( ActionEvent e ) {
	citadel.me.showHostBrowser();
      }} );

    add( "South", p );
  }

  public void refresh() {
    serverBlurb.setText( citadel.me.serverInfo.blurb );
    message.setText( "" );
    citadel.me.getSystemMessage( "goodbye", new CallBack() {
      public void run( citReply r ) {
	message.setText( r.getData() );
	}
      });
  }
}
