import javax.swing.*;
import java.awt.*;
import java.awt.event.*;


public class citGui {
    JFrame	frame;
    CardLayout	cdLayout;
    Container	c;
    hostPanel	hp;
    loginPanel	lp;
    mainPanel	mp;
    logoffPanel	lfp;

    public citGui() {
	frame = new JFrame( "Shaggy" );
	c = frame.getContentPane();

	c.setLayout( cdLayout = new CardLayout() );
	c.add( "Host", hp = new hostPanel() );
	c.add( "Login", lp = new loginPanel() );
	c.add( "Main", mp = new mainPanel() );
	c.add( "Logoff", lfp = new logoffPanel() );

	frame.addWindowListener( new WindowAdapter() {
	    public void windowClosing( WindowEvent e ) {
		citadel.me.closeFrame();
		System.exit( 0 );
	    }
	});

	frame.pack();
	frame.setVisible( true );
    }

    public void showHostBrowser() {
	cdLayout.show( c, "Host" );
	hp.refresh();
    }

    public void showLoginPanel() {
	cdLayout.show( c, "Login" );
	lp.refresh();
    }

    public void showLoginPanel( String user, String pass ) {
	showLoginPanel();
	lp.setDefault( user, pass );
	if( (user == null) || (user.length()==0) ||
	    (pass == null) || (pass.length()==0) ) return;
	citadel.me.authenticate( user, pass );
    }

    public void showMainPanel() {
	cdLayout.show( c, "Main" );
	mp.refresh();
    }

    public void showLogoffPanel() {
	cdLayout.show( c, "Logoff" );
	lfp.refresh();
    }

    public void errMsg( String reason ) {
	JOptionPane.showMessageDialog(frame,
				      reason,
				      "Inane error",
				      JOptionPane.ERROR_MESSAGE);
    }

    public void warning( String text ) {
	JOptionPane.showMessageDialog(frame,
				      text,
				      "What is this?",
				      JOptionPane.WARNING_MESSAGE);
    }
}




