import javax.swing.*;
import java.awt.*;
import java.awt.event.*;

public class roomInfoWindow extends JFrame {
    roomInfo	ri;
    JTextArea	info;


    public roomInfoWindow( roomInfo ri ) {
	Container	c;

	setTitle( "Info for " + ri.name );
	this.ri = ri;

	JPanel	p = new JPanel();
	p.setLayout( new BorderLayout() );
	p.setBorder( BorderFactory.createTitledBorder( 
						      BorderFactory.createEtchedBorder(), ri.name ) );

	p.add( "Center", 
	       new JScrollPane( info = new JTextArea(),
				JScrollPane.VERTICAL_SCROLLBAR_AS_NEEDED,
				JScrollPane.HORIZONTAL_SCROLLBAR_NEVER ) );

	info.setEditable( false );
	info.setLineWrap( true );
	info.setWrapStyleWord( true );

	VertPanel	vp = new VertPanel();
	vp.add( new JLabel( ri.total + " messages" ) );
	vp.add( new JLabel( "Floor: " + citadel.me.rooms.getFloor( ri.floor ).name() ) );
	JCheckBox	cb;
	vp.add( cb = new JCheckBox( "Permanent", ri.perm ) );
	cb.setEnabled( false );
	vp.add( cb = new JCheckBox( "Private", ri.priv ) );
	cb.setEnabled( false );
	vp.add( cb = new JCheckBox( "Password", ri.pass ) );
	cb.setEnabled( false );
	vp.add( cb = new JCheckBox( "Directory", ri.dir ) );
	cb.setEnabled( false );
	vp.add( cb = new JCheckBox( "Networked", ri.net ) );
	cb.setEnabled( false );
	p.add( "East", vp );

	c = getContentPane();
	c.setLayout( new BorderLayout() );
	c.add( "Center", p );
	
	JPanel	pp = new JPanel();
	final JButton	change = new JButton( "Change Info" );
	pp.add( change );
	change.setEnabled( false );
	change.addActionListener( new ActionListener() {
	  public void actionPerformed( ActionEvent e ) {
	    citadel.me.networkEvent( "EINF 1", info.getText(), new CallBack() {
	      public void run( citReply r ) {
		refresh();
	      } } );
	  } } );

	JButton	b;

	pp.add( b = new JButton( "Close" ) );
	b.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		closeWin();
	    } } );

	c.add( "South", pp );

	addWindowListener( new WindowAdapter() {
	    public void windowClosing( WindowEvent e ) {
		closeWin();
	    } } );

	setSize( 400, 400 );
	show();

	citadel.me.registerWindow( this );
	refresh();
	citadel.me.networkEvent( "EINF 0", new CallBack() {
	  public void run( citReply r ) {
	    if( r.ok() ) {
	      info.setEditable( true );
	      change.setEnabled( true );
	    }
	  } } );

    }

    public void refresh() {
	citadel.me.networkEvent( "RINF", new CallBack() {
	    public void run( citReply r ) {
		if( r.error() ) info.setText( r.getArg(0) );
		else info.setText( r.getData() );
	    } } );
    }	

    public void closeWin() {
	citadel.me.removeWindow( this );
	dispose();
    }

}







