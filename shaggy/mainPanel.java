import javax.swing.*;
import java.awt.*;
import java.awt.event.*;
import java.util.*;

public class mainPanel extends JPanel {
    JComboBox	jcb;
    DefaultListModel	newLM, seenLM;
    roomMap	rooms;

    boolean	jcb_update = false;

    public mainPanel() {
	setLayout( new BorderLayout() );

	JPanel	p = new JPanel();
	p.setLayout( new BorderLayout() );
	/*	p.setBorder( BorderFactory.createTitledBorder( 
		BorderFactory.createEtchedBorder(), "Cmds" ) ); */

	VertPanel	vp = new VertPanel();
	p.add( "Center", vp );
	JButton	b;

	vp.add( b = new JButton( "Next Room" ) );
	b.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		citadel.me.enterRoom( rooms.nextNewRoom() );
	    }});

	vp.add( b = new JButton( "Goto Room" ) );
	b.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
	      citadel.me.enterRoom();
	    }});

	vp.add( b = new JButton( "Page User" ) );
	b.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		new pageUserWindow();
	    }});

	vp.add( b = new JButton( "Who Online" ) );
	b.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		new whoOnlineWindow();
	    }});

	vp.add( b = new JButton( "Options" ) );
	b.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
	    }});

	b.setEnabled( false );

	vp.add( b = new JButton( "Server Info" ) );
	b.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
	    }});

	b.setEnabled( false );

	vp.add( b = new JButton( "Logoff" ) );
	b.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		citadel.me.logoff();
	    } } );
    
	add( "West", p );

	p = new JPanel();
	p.setLayout( new BorderLayout() );

	PairPanel	pp = new PairPanel();
	jcb = new JComboBox();
	jcb.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		if( !jcb_update )
		    updateLists( (String)jcb.getSelectedItem() );
	    } } );
	
	pp.addLeft( new JLabel( "Floor: " ) );
	pp.addRight( jcb );

	p.add( "North", pp );

	JPanel	ppp = new JPanel();
	ppp.setLayout( new GridLayout( 1, 0 ) );
	newLM = makeList( "New Rooms", ppp );
	seenLM = makeList( "Old Rooms", ppp );

	p.add( "Center", ppp );

	add( "Center", p );
    }

    public DefaultListModel makeList( String name, JPanel pp ) {
	final JList	jl;
	DefaultListModel	lm;

	JPanel	p = new JPanel();
	p.setLayout( new BorderLayout() );
	p.setBorder( BorderFactory.createTitledBorder( 
						      BorderFactory.createEtchedBorder(), name ) );

	lm = new DefaultListModel();

	p.add( "Center", new JScrollPane( jl = new JList(lm) ) );

	pp.add( p );

	jl.addMouseListener( new MouseAdapter() {
	    public void mouseClicked( MouseEvent e ) {
		String	what = (String)jl.getSelectedValue();
		if( what == null ) return;

		if( e.getClickCount() == 2 )
		    citadel.me.enterRoom( what );
	    }
	} );

	return lm;
    } 

    public void refresh() {
	rooms = new roomMap( this );
	citadel.me.rooms = rooms;
	citadel.me.networkEvent( "LFLR", new CallBack() {
	    public void run( citReply r ) {
		rooms.floors( r );
		jcb_update = true;
		if( jcb.getModel().getSize() > 0 )
		    jcb.removeAllItems();
		for( Enumeration e = rooms.f_list.elements(); e.hasMoreElements(); ) {
		    floor	f = (floor)e.nextElement();
		    jcb.addItem( f.name() );
		}
		jcb_update = false;
		setFloor( rooms.getFloor() );
	    } } );

	citadel.me.networkEvent( "LKRN", new CallBack() {
	    public void run( citReply r ) {
		rooms.new_rooms( r );
	    } } );

	citadel.me.networkEvent( "LKRO", new CallBack() {
	    public void run( citReply r ) {
		rooms.old_rooms( r );

		setFloor( rooms.getFloor() );
		updateLists( rooms.getFloor().name );
	    } } );
    }

    public void setFloor( String flr ) {
	if( flr == null ) return;
	setFloor( rooms.getFloor( flr ) );
    }
    
    public void setFloor( floor f ) {
	if( f == null ) return;
	jcb.setSelectedItem( f.name() );
    }

    public void updateLists( String name  ) {
	floor 	f = rooms.getFloor( name );
	if( f == null ) return;

	rooms.cur_floor = f;
	newLM.removeAllElements();
	seenLM.removeAllElements();
	for( Enumeration e = f.rooms.elements(); e.hasMoreElements(); ) {
	    room	rm = (room)e.nextElement();
	    if( rm.hasNew() )
		newLM.addElement( rm.name () );
	    else
		seenLM.addElement( rm.name() );
	}
    }
}
