import javax.swing.*;
import java.awt.*;
import java.awt.event.*;

public class enterPanel extends JPanel{
    roomFrame	rf;
    roomInfo	ri;

    JTextField	to;
    JTextArea	msg;

    public enterPanel( final roomFrame rf ) {
	this.rf = rf;
	setLayout( new BorderLayout() );

	JPanel	p = new JPanel();
	p.setBorder( BorderFactory.createTitledBorder(
	             BorderFactory.createEtchedBorder(), "Recipient" ) );
	p.setLayout( new BorderLayout() );

	p.add( "Center", to = new JTextField() );

	JButton	b = new JButton( "Select" );
	b.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		System.out.println( "User list bialog!" );
	    } } );

	p.add( "East", b );

	add( "North", p );

	to.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		msg.requestFocus();
	    } } );

	p = new JPanel();
	p.setBorder( BorderFactory.createTitledBorder(
	             BorderFactory.createEtchedBorder(), "Message Text" ) );
	p.setLayout( new BorderLayout() );

	p.add( "Center", new JScrollPane( msg = new JTextArea(), 
		  JScrollPane.VERTICAL_SCROLLBAR_AS_NEEDED,
		  JScrollPane.HORIZONTAL_SCROLLBAR_NEVER ) );

	msg.setLineWrap( true );
	msg.setWrapStyleWord( true );

	add( "Center", p );

	p = new JPanel();

	p.add( b = new JButton( "Send" ) );
	b.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		String cmd = "ENT0 1|";

		if( ri.mail ) cmd = cmd + to.getText();
		cmd = cmd+"|0|0|0";

		String	theMsg = wrap( msg.getText() );

		System.out.println( theMsg );
		citadel.me.networkEvent( cmd, theMsg, new CallBack() {
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

    public String wrap( String in ) {
	StringBuffer	b = new StringBuffer( in );
	int	last_space = 0, line_length=0;
	for( int i = 0; i < b.length(); i++ ) {
	    line_length++;
	    if( line_length > 76 ) {
		b.setCharAt( last_space, '\n' );
		line_length = 0;
	    }

	    if( b.charAt(i) == ' ' )
		last_space = i;
	}
	return b.toString();
    }
}
