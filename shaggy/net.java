
import javax.swing.*;
import java.io.*;
import java.net.*;
import java.util.*;

public class net implements Runnable {
    Queue	      	theQueue;

    Socket		theSocket;
    DataInputStream	in;
    DataOutputStream	out;

    boolean		done;

    public net() {
	theQueue = new Queue();
    }

    private void println( String s ) {
	System.out.println( ">" + s );
	try {
	    if( theSocket != null )
		out.writeBytes( s + "\n" );
	} catch( IOException ioe ) {
	    citadel.me.lostNetwork( "Connection dropped (write)" );
	    if( theSocket != null ) {
		try { theSocket.close(); }
		catch( Exception e ) {};
	    }
	    theSocket = null;
	}
    }

    private String readLine( ) {
	try {
	    if( theSocket != null ) {
		String s = in.readLine();
		System.out.println( "<" + s );
		return s;
	    }
	} catch( IOException ioe ) {
	    citadel.me.lostNetwork( "Network error: read" );
	    if( theSocket != null ) {
		try { theSocket.close(); }
		catch( Exception e ) {};
	    }
	    theSocket = null;
	}
	return null;
    }

    public String getArch() {
	try {
	    Properties	p = System.getProperties();
	    return p.get( "os.name" ) + "/" + p.get( "os.arch" );
	} catch( SecurityException se ) {
	    return "<unknown>";
	}
    }

    public String getHostName() {
	try {
	    InetAddress	me = InetAddress.getLocalHost();
	    return me.getHostName();
	} catch( Exception e ) {
	    return "dunno";
	}
    }

    public void run() {
	String	server = citadel.me.server;
	int		port = citadel.me.port;
	boolean		proxy = false;

	done = false;
	try {
	    if( proxy )
		proxy = !server.equals( "127.0.0.1" );
	  
	    if( proxy )
		theSocket = new Socket( "armstrong.cac.psu.edu", 13579 );
	    else
		theSocket = new Socket( server, port );
	    

	    in = new DataInputStream( theSocket.getInputStream() );
	    out = new DataOutputStream( theSocket.getOutputStream() );
	} catch( IOException ioe ) {
	    citadel.me.lostNetwork( "Couldn't connect to server." );
	    return;
	}
	
	if( proxy ) {
	    println( server );
	    println( ""+port );
	}

	citReply	rep = getReply();
	if( !rep.ok() ) {
	    citadel.me.lostNetwork( "Couldn't connect: " + rep.line );
	    return;
	}

	getReply( "IDEN 0|7|" + citadel.VERSION + "|" + citadel.NAME + " " +
		  citadel.VERSION + " (" + getArch() + ")|" + getHostName() );

	Thread	t = new Thread() {
	    public void run() {
		while( !citadel.me.theNet.done ) {
		    try {
			Thread.sleep( 30000 );
		    } catch( Exception e ) {}

		    System.out.println( "Idle event" );
		    if( citadel.me.wo == null ) {
		      if( theQueue.empty() )
			citadel.me.networkEvent( "NOOP" );
		      else
			System.out.println( "...events pending" );
		    }
		    else
			citadel.me.wo.refresh();
		}
	    } };

	t.start();

	MsgCmd	m;
	while( ((m = (MsgCmd)theQueue.get()) != null) && !done ) {
	    citReply	r = getReply( m.cmd, m.data );
	    m.setReply( r );
	    SwingUtilities.invokeLater( m );
	}

	try {
	if( theSocket != null )
	    theSocket.close();
	} catch( IOException ioe ) {
	}
	if( !done )
	    citadel.me.lostNetwork( "Connection closed." );

	try {
	    t.stop();
	} catch( Exception e ) {}
    }

    public void done() {
	done = true;
	theQueue.append( null );
    }

    public void append( MsgCmd m ) {
	theQueue.append( m );
    }

    public citReply getReply() {
	return getReply( (String)null, (String)null );
    }

    public citReply getReply( String cmd ) {
	return getReply( cmd, (String)null );
    }

    public citReply getReply( String cmd, String data ) {
	if( cmd != null ) println( cmd );

	citReply r = new citReply( readLine() );
	if( r.listingFollows() ) {
	    while( r.addData( readLine() ) ) ;
	}

	if( r.sendListing() ) {
	    if( data != null )
		println( data );
	    println( "000" );
	}

	if( r.expressMessage() )
	    citadel.me.expressMsg();

	return r;
    }
}
