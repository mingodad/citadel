/* net.java
 * chilly@alumni.psu.edu
 *
 * Object to talk the network talk with cit/ux server.
 * Dispatches events.
 */

import java.io.*;
import java.net.*;

public class net {
  Socket		theServer;

  DataInputStream	in;
  DataOutputStream	out;

  public net() {
    theServer = null;
  }

  public void println( String s ) {
    System.out.println( ">" + s );
    try {
      if( theServer != null )
	out.writeBytes( s + "\n" );
    } catch( IOException ioe ) {
      citadel.me.lostNetwork( "Connection dropped (write)" );
      if( theServer != null ) {
	try { theServer.close(); theServer = null; }
	catch( Exception e ) {};
      }
    }
  }

  public String readLine( ) {
    try {
      if( theServer != null ) {
	String s = in.readLine();
	System.out.println( "<" + s );
	return s;
      }
    } catch( IOException ioe ) {
      citadel.me.lostNetwork( "Network error: reead" );
      if( theServer != null ) {
	try { theServer.close(); theServer = null; }
	catch( Exception e ) {};
      }
    }
    return null;
  }

  public boolean connect( ) {
    return connect( "127.0.0.1" );
  }

  public boolean connect( String serverName ) {
    return connect( serverName, 504 );
  }

  public boolean connect( String serverName, int port ) {
    try {
      theServer = new Socket( serverName, port );

      in = new DataInputStream( theServer.getInputStream() );
      out = new DataOutputStream( theServer.getOutputStream() );

    } catch( IOException ioe ) {
      citadel.me.lostNetwork( "Couldn't connect to server" );
      return false;
    }

    citReply rep = getReply();

    if( rep.ok() ) return true;
    return false;
  }

  public citReply getReply() {
    return getReply( (String)null );
  }

  public citReply getReply( String cmd ) {
    if( cmd != null ) println( cmd );

    citReply r = new citReply( readLine() );
    if( r.listingFollows() ) {
      while( r.addData( readLine() ) ) ;
    }

    if( r.expressMessage() )
      citadel.me.expressMsg();

    return r;
  }
}




