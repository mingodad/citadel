import javax.swing.*;
import java.awt.*;
import java.awt.event.*;
import java.util.*;

public class messagePanel extends JPanel {
    roomFrame	parent;
    JTextField	blurb;
    JTextArea	message;

    roomInfo	ri;
    Vector	msgs;
    int		cur_pos = 0;

    JComboBox	jcb;
    JButton	next_msg, prev_msg;
    boolean	ignore_box = false;

    public messagePanel( final roomFrame parent ) {
	this.parent = parent;

	setLayout( new BorderLayout() );
	
	JPanel	p = new JPanel();
	p.setLayout( new BorderLayout() );
	p.setBorder( BorderFactory.createTitledBorder( 
		  BorderFactory.createEtchedBorder(), "Cmds" ) );

	VertPanel	vp = new VertPanel();
	p.add( "Center", vp );
	JButton		b;

	vp.add( b = new JButton( "Next Room" ) );
	b.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		citadel.me.enterRoom( citadel.me.rooms.nextNewRoom() );
	    } } );

	vp.add( b = new JButton( "Enter Message" ) );
	b.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		parent.enterMessage();
	    } } );

	vp.add( b = new JButton( "Zap Room" ) );
	b.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		System.out.println( "Zap room" );
	    } } );


	jcb = new JComboBox();
	jcb.addItem( "Read New" );
	jcb.addItem( "Read All" );
	jcb.addItem( "Last 5" );
	jcb.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		if( !ignore_box )
		    refresh();
	    }
	} );

	vp.add( jcb );

	add( "West", p );

	p = new JPanel();

	p.add( prev_msg = new JButton( "Prev Message" ) );
	prev_msg.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		cur_pos--;
		displayMessage();
	    } } );

	p.add( next_msg = new JButton( "Next Message" ) );
	next_msg.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		cur_pos++;
		displayMessage();
	    } } );


	add( "South", p );


	p = new JPanel();
	p.setLayout( new BorderLayout() );

	p.add( "North", blurb = new JTextField() );
	blurb.setEditable( false );
	p.add( "Center", new JScrollPane( message = new JTextArea() )  );
	message.setEditable( false );
	add( "Center", p );
    }

    public void refresh() {
	refresh( ri );
    }

    public void refresh( roomInfo ri ) {
	this.ri = ri;

	cur_pos = 0;
	msgs = new Vector();

	/* add new, all, last, etc. */

	String	cmd = "new";
	switch( jcb.getSelectedIndex() ) {
	case	0:	// new
	    break;
	case 1:		// All
	    cmd = "all";
	    break;
	case 2:		// Last - 5
	    cmd = "last|5";
	    break;
	}

	next_msg.setEnabled( false );
	prev_msg.setEnabled( false );

	citadel.me.networkEvent( "MSGS " + cmd, new CallBack() {
	    public void run( citReply	r ) {
		msgs = r.listing;
		displayMessage();
	    } } );
    }

    public void displayMessage() {
	if( (msgs == null) || (msgs.size() == 0) ) {
	    blurb.setText( "" );
	    message.setText( "<no messages>" );
	    return;
	}

	if( cur_pos <= 0 ) {
	    cur_pos = 0;
	    prev_msg.setEnabled( false );
	} else
	    prev_msg.setEnabled( true );

	if( cur_pos >= msgs.size()-1 ) {
	    cur_pos = msgs.size()-1;
	    next_msg.setEnabled( false );
	} else
	    next_msg.setEnabled( true );

	final String	num = (String)msgs.elementAt( cur_pos );

	citadel.me.networkEvent( "MSG0 " + num + "|0", new CallBack() {
	    public void run( citReply r ) {
		if( r.error() ) {
		    blurb.setText( "<error>" );
		    message.setText( r.getArg(0) );
		} else {
		    message msg = new message( msgs.size(), cur_pos, r );
		    blurb.setText( msg.getBlurb() );
		    message.setText( msg.getText() );
		    int n = citadel.atoi( num );
		    if( n  > ri.highest_read ) {
			citadel.me.networkEvent( "SLRP " + num );
			ri.highest_read = n;
		    }
		}
	    }
	} );
    }
}
