import javax.swing.*;
import java.awt.*;
import java.awt.event.*;

/* Quick quick quick hack */

public class enterPanel extends JPanel{
    roomFrame	rf;
    roomInfo	ri;

    JTextField	to;
    JTextArea	msg;

    public enterPanel( final roomFrame rf ) {
	this.rf = rf;
	setLayout( new BorderLayout() );

	add( "North", to = new JTextField() );
	to.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		msg.requestFocus();
	    } } );

	add( "Center", msg = new JTextArea() );

	JPanel	p = new JPanel();
	JButton	b;

	p.add( b = new JButton( "Send" ) );
	b.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		String cmd = "ENT0 1|";

		if( ri.mail ) cmd = cmd + to.getText();
		cmd = cmd+"|0|0|0";

		System.out.println( msg.getText() );
		citadel.me.networkEvent( cmd, msg.getText(), new CallBack() {
		    public void run( citReply r ) {
			if( r.error() )
			    citadel.me.warning( r.getArg(0) );
			rf.showRoom();
		    } } );
	    }
	} );

	p.add( b = new JButton( "Cancel" ) );
	b.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		rf.showRoom();
	    } } );

	add( "South", p );
    }

    public void refresh( roomInfo ri ) {
	this.ri = ri;
	msg.setText( "" );
	to.setText( "" );

	to.setEnabled( ri.mail );
    }
}
