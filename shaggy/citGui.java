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
    JMenu	cit, rooms;
    JMenuItem	logoff;

    public citGui() {
	frame = new JFrame( "Shaggy" );
	JLayeredPane	p = new JLayeredPane();
	JMenuBar	mbar = new JMenuBar();

	c = frame.getContentPane();
	c.setLayout( new BorderLayout() );
	c.add( mbar, BorderLayout.NORTH );

	c.add( p, BorderLayout.CENTER  );
	c = p;

	c.setLayout( cdLayout = new CardLayout() );
	c.add( "Host", hp = new hostPanel() );
	c.add( "Login", lp = new loginPanel() );
	c.add( "Main", mp = new mainPanel() );
	c.add( "Logoff", lfp = new logoffPanel() );

	frame.addWindowListener( new WindowAdapter() {
	    public void windowClosing( WindowEvent e ) {
		citadel.me.closeFrame();
	    }
	});

	JMenu		m;
	JMenuItem	mi;

	m = new JMenu( "File" );

	mi = new JMenuItem( "Load hosts..." );
	mi.setEnabled( false );
	m.add( mi );

	mi = new JMenuItem( "Save hosts..." );
	mi.setEnabled( false );
	m.add( mi );

	mi = new JMenuItem( "Shaggy Preferences" );
	mi.setEnabled( false );
	m.add( mi );

	m.add( new JSeparator() );

	logoff = new JMenuItem( "Logoff" );
	logoff.addActionListener( new ActionListener() {
	  public void actionPerformed( ActionEvent e ) {
	    citadel.me.logoff();
	  } } );
	m.add( logoff );
	logoff.setEnabled( false );
	logoff.setMnemonic( 'L' );

	mi = new JMenuItem( "Quit" );
	mi.addActionListener( new ActionListener() {
	  public void actionPerformed( ActionEvent e ) {
	    citadel.me.closeFrame();
	  } } );
	m.add( mi );
	mi.setMnemonic( 'Q' );

	mbar.add( m );

	cit = new JMenu( "Citadel" );
	cit.setEnabled( false );
	cit.add( mi = new JMenuItem( "Page User..." ) );
	mi.addActionListener( new ActionListener() {
	  public void actionPerformed( ActionEvent e ) {
	    new pageUserWindow();
	  } } );

	cit.add( mi = new JMenuItem( "Who is Online" ) );
	mi.addActionListener( new ActionListener() {
	  public void actionPerformed( ActionEvent e ) {
	    new whoOnlineWindow();
	  } } );

	cit.add( new JSeparator() );
	cit.add( mi = new JMenuItem( "User List" ) );
	mi.setEnabled( false );
	cit.add( mi = new JMenuItem( "User Biographies" ) );
	mi.setEnabled( false );
	cit.add( new JSeparator() );
	cit.add( mi = new JMenuItem( "User Configuration" ) );
	mi.setEnabled( false );
	cit.add( mi = new JMenuItem( "Server Information" ) );
	mi.setEnabled( false );

	mbar.add( cit );

	rooms = new JMenu( "Rooms" );
	rooms.setEnabled( false );
	rooms.add( mi = new JMenuItem( "Next Room" ) );
	mi.addActionListener( new ActionListener() {
	  public void actionPerformed( ActionEvent e ) {
	    citadel.me.enterRoom( citadel.me.rooms.nextNewRoom() );
	  } } );

	rooms.add( mi = new JMenuItem( "Goto Room" ) );
	mi.addActionListener( new ActionListener() {
	  public void actionPerformed( ActionEvent e ) {
	    citadel.me.enterRoom();
	  } } );

	rooms.add( new JSeparator() );
	rooms.add( mi = new JMenuItem( "List Zapped Rooms" ) );
	mi.setEnabled( false );
	rooms.add( mi = new JMenuItem( "Create Room..." ) );
	mi.setEnabled( false );
	rooms.add( mi = new JMenuItem( "Edit Room" ) );
	mi.setEnabled( false );

	mbar.add( rooms );

	frame.getLayeredPane().moveToFront( mbar );
	frame.getLayeredPane().moveToFront( mi );
	frame.getLayeredPane().moveToFront( cit );
	frame.getLayeredPane().moveToFront( rooms );
	frame.getLayeredPane().moveToBack( p );

	frame.pack();
	frame.setVisible( true );
    }

    public void showHostBrowser() {
	cdLayout.show( c, "Host" );
	hp.refresh();
	cit.setEnabled( false );
	rooms.setEnabled( false );
	logoff.setEnabled( false );
    }

    public void showLoginPanel() {
	cdLayout.show( c, "Login" );
	lp.refresh();
	cit.setEnabled( false );
	rooms.setEnabled( false );
	logoff.setEnabled( false );
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
	cit.setEnabled( true );
	rooms.setEnabled( true );
	logoff.setEnabled( true );
	mp.refresh();
    }

    public void showLogoffPanel() {
	cdLayout.show( c, "Logoff" );
	cit.setEnabled( false );
	rooms.setEnabled( false );
	logoff.setEnabled( false );
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
