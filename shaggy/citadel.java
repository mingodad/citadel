import javax.swing.JFrame;
import java.util.*;

public class citadel {
    public final static String	NAME="Shaggy", VERSION="0.1";
    public static citadel	me;

    String		server;
    int			port;
    Vector 		windows;

    citGui		cg;
    net			theNet;

    server		serverInfo;
    user	      	theUser;
    roomMap		rooms;
    roomFrame		rf;
    whoOnlineWindow	wo = null;

    public static void main( String args[] ) {
	new citadel();
    }

    public static int atoi( String s ) {
	if( s == null ) return 0;
	try {
	    return Integer.parseInt( s );
	} catch( NumberFormatException nfe ) {
	    return 0;
	}
    }

    public citadel() {
	me = this;
	cg = new citGui();
	theNet = new net();
	serverInfo = null;
	rf = null;
	windows = new Vector();
    }

    public void showHostBrowser() {
	cg.showHostBrowser();
    }

    public void setServer( String server, String port ) {
	int	p = atoi( port );
	if( p == 0 ) p = 504;
	setServer( server, p );
    }

    public void setServer( String server, int port ) {
	this.server = server;
	this.port = port;

	Thread	t = new Thread( theNet );
	t.start();
    }

    public void setServerInfo( server s ) {
	serverInfo = s;
    }

    public void showLoginPanel() {
	cg.showLoginPanel();
    }

    public void showLoginPanel( String user, String pass ) {
	cg.showLoginPanel( user, pass );
    }

    public void showMainPanel() {
	cg.showMainPanel();
    }

    public void expressMsg() {
	networkEvent( "GEXP", new CallBack() {
	    public void run( citReply r ) {
		if( !r.error() )
		    new expressWindow(r);
		if( atoi( r.getArg( 0 ) ) != 0 )
		    expressMsg();
	    } } );
    }

    public void gotoRoom( String name ) {
	System.out.println( "goto room:" + name );
    }

    public void lostNetwork( String reason ) {
	theNet.done = true;
	cg.errMsg( reason );
	cg.showHostBrowser();
    }

    public void warning( String text ) {
	cg.warning( text );
    }

    public void closeFrame() {
	System.out.println( "Closed the friggin frame." );
    }

    public void networkEvent( String cmd ) {
	networkEvent( cmd, null, null );
    }

    public void networkEvent( String cmd, CallBack cb ) {
	networkEvent( cmd, null, cb );
    }

    public void networkEvent( String cmd, String data ) {
	networkEvent( cmd, data, null );
    }

    public void networkEvent( String cmd, String data, CallBack cb ) {
	theNet.append( new MsgCmd( cmd, data, cb ) );
    }

    public void getServerInfo( CallBack	cb ) {
	networkEvent( "INFO", cb );
    }

    public void getSystemMessage( String f_name, CallBack cb ) {
	networkEvent( "MESG " + f_name, cb );
    }

    public void authenticate( final String user, final String pass ) {
	networkEvent( "USER " + user, new CallBack() {
	    public void run( citReply r ) {
		if( r.moreData() ) {
		    networkEvent( "PASS "+ pass, new CallBack() {
			public void run( citReply r ) {
			    if( r.error() ) {
				warning( "Wrong Password" );
			    } else {
				citadel.me.setUser( new user( r ) );
			    }
			} });
		} else {
		    warning( user + ":No such user" );
		}
	    }
	} );
    }

    public void enterRoom() {
	new enterRoomWindow();
    }

    public void enterRoom( String s ) {
	enterRoom( s, null );
    }

    public void enterRoom( room r ) {
	enterRoom( r.name, null );
    }

    public void enterRoom( final String roomName, String pass ) {
	String	cmd = "GOTO " + roomName;
	if( pass != null )
	    cmd = cmd + "|" + pass;

	networkEvent( cmd, new CallBack() {
	    public void run( citReply r ) {
		if( r.ok() ) {
		    room	rm = cg.mp.rooms.getRoom( roomName );
		    if( rm == null ) {	/* didn't know about it before */
		      rm = new room( roomName, r.getArg( 10 ) );
		      cg.mp.rooms.rooms.put( roomName, rm );
		      cg.mp.rooms.addToFloor( rm );
		    }
		    
		    rm.setNew( false );

		    roomInfo	ri = new roomInfo( rm, r );

		    /* check ri.mail and act accordingly */

		    if( rf == null )
			rf = new roomFrame();

		    rf.setRoom( ri );
		    cg.mp.updateLists( rooms.getFloor().name() );	// hack
		} else if( r.res_code == 540 ) {
		    new enterRoomWindow( roomName );
		}
	    } } );
    }

    public void zapRoom( final roomInfo ri ) {
	networkEvent( "FORG " + ri.name, new CallBack() {
	    public void run( citReply r ) {
		if( r.ok() ) {
		    enterRoom( rooms.forgotRoom( ri ) );
		    cg.mp.setFloor( rooms.getFloor() );
		} } } );
    }

    public void logoff() {
	/* close windows */
	if( rf != null )
	    rf.dispose();
	rf = null;
	for( Enumeration e = windows.elements(); e.hasMoreElements(); ) {
	    JFrame	f = (JFrame)e.nextElement();
	    f.dispose();
	}

	cg.showLogoffPanel();
	networkEvent( "QUIT", new CallBack() {
	    public void run( citReply r ) {
		theNet.done();
	    } } );
    }

    public void setUser( user theUser ) {
	this.theUser = theUser;
	showMainPanel();
	networkEvent( "CHEK", new CallBack() {
	    public void run( citReply r ) {
		int	msg = atoi( r.getArg( 0 ) );
		if( msg != 0 )
		    enterRoom( "Mail" );
	    } } );
    }

    public void registerWindow( JFrame	win ) {
	windows.addElement( win );
    }

    public void removeWindow( JFrame win ) {
	windows.removeElement( win );
    }

    public boolean floors() {
	return true;	// FIXME
    }
}


