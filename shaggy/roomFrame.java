import javax.swing.*;
import java.awt.*;
import java.awt.event.*;

public class roomFrame extends JFrame {
    CardLayout		cdLayout;
    Container		c;

    roomInfo		ri;

    messagePanel	mp;
    enterPanel		ep;

    public roomFrame() {
	ri = null;
	setTitle( "Title of this window" );

	c = getContentPane();

	c.setLayout( cdLayout = new CardLayout() );
	c.add( "Messages", mp = new messagePanel(this) );
	c.add( "Compose", ep = new enterPanel(this) );

	addWindowListener( new WindowAdapter() {
	    public void windowClosing( WindowEvent e ) {
		citadel.me.rf = null;
		dispose();
	    }
	} );

	JMenuBar	mbar = new JMenuBar();
	JMenu		m = new JMenu( "Rooms" );
	mbar.add( m );
	JMenuItem	mi = new JMenuItem( "Next Room" );
	m.add( mi );
	mi.addActionListener( new ActionListener() {
	  public void actionPerformed( ActionEvent e ) {
	    citadel.me.enterRoom( citadel.me.rooms.nextNewRoom() );
	  } } );

	m.add( mi = new JMenuItem( "Goto Room" ) );
	mi.addActionListener( new ActionListener() {
	  public void actionPerformed( ActionEvent e ) {
	    citadel.me.enterRoom();
	  } } );

	m.add( mi = new JMenuItem( "Zap Room" ) );
	mi.addActionListener( new ActionListener() {
	  public void actionPerformed( ActionEvent e ) {
	    if( JOptionPane.showConfirmDialog( null, "Zap " + mp.ri.name + "?" ) == 0 )
	      citadel.me.zapRoom( mp.ri );
	  } } );

	m.add( new JSeparator() );

	m.add( mi = new JMenuItem( "Room Info" ) );
	mi.addActionListener( new ActionListener() {
	  public void actionPerformed( ActionEvent e ) {
	    new roomInfoWindow( mp.ri );
	  } } );

	mbar.add( m = new JMenu( "Messages" ) );
	m.add( mi = new JMenuItem( "Next Message" ) );
	mi.addActionListener( new ActionListener() {
	  public void actionPerformed( ActionEvent e ) {
	    mp.nextMessage();
	  } } );

	m.add( mi = new JMenuItem( "Previous Message" ) );
	mi.addActionListener( new ActionListener() {
	  public void actionPerformed( ActionEvent e ) {
	    mp.prevMessage();
	  } } );

	m.add( mi = new JMenuItem( "Enter Message" ) );
	mi.addActionListener( new ActionListener() {
	  public void actionPerformed( ActionEvent e ) {
	    enterMessage();
	  } } );

	setJMenuBar( mbar );
	pack();
	show();
    }

    public void setRoom( roomInfo ri ) {
	this.ri = ri;

	String	title = ri.name;
	if( citadel.me.floors() ) {
	    title = title + " (" + citadel.me.rooms.getFloor( ri.rm ).name + ")";
	}

	setTitle( title );
	cdLayout.show( c, "Messages" );
	mp.refresh( ri );
	if( ri.info != 0)
	    new roomInfoWindow( ri );
    }

    public void showRoom() {
	cdLayout.show( c, "Messages" );
    }

    public void enterMessage() {
	cdLayout.show( c, "Compose" );
	ep.refresh( ri );
    }
}
