/* citReply.java
 *
 * Object to parse the reply from the server, so I don't have to think
 * about it when I write code.
 */

import java.util.*;

public class citReply {
  public static final int	LISTING_FOLLOWS=100, OK=200, MORE_DATA=300,
    SEND_LISTING=400, ERROR=500, BINARY_FOLLOWS=600, SEND_BINARY=700,
    START_CHAT_MODE=800;

  int		res_code;
  String	line,data;
  Vector	args, listing;
  boolean	expressmsg;

  public citReply( String line ) {
    this.line = line;

    args = new Vector();
    listing = null;
    data = null;
    res_code = ERROR;
    expressmsg = false;

    if( line != null )
      parseLine();
  }

  public void parseLine() {
    try {
      res_code = Integer.parseInt( line.substring( 0, 3 ) );
    } catch( Exception e ) {};

    StringBuffer	s = new StringBuffer();

    if( (line.length() > 3) && (line.charAt( 3 ) == '*') )
      expressmsg = true;

    for( int i = 4; i < line.length(); i++ ) {
      char	c = line.charAt( i );
      if( c == '|' ) {
	args.addElement( s.toString() );
	s = new StringBuffer();
      }
      else
	s.append( c );
    }
    if( s.length() != 0 ) args.addElement( s.toString() );

  }

  public boolean expressMessage() {
    return expressmsg;
  }

  public boolean listingFollows() {
    return res_code/100 == LISTING_FOLLOWS/100;
  }

  public boolean ok() {
    return res_code/100 == OK/100;
  }

  public boolean moreData() {
    return res_code/100 == MORE_DATA/100;
  }

  public boolean sendListing() {
    return res_code/100 == SEND_LISTING/100;
  }

  public boolean error() {
    return res_code/100 == ERROR/100;
  }

  public  boolean binaryFollows() {
    return res_code/100 == BINARY_FOLLOWS/100;
  }

  public boolean sendBinary() {
    return res_code/100 == SEND_BINARY/100;
  }

  public boolean startChatMode() {
    return res_code/100 == START_CHAT_MODE/100;
  }

  public boolean addData( String s ) {
    if( s.equals( "000" ) )
      return false;

    if( listing == null ) listing = new Vector();
    listing.addElement( s );
    return true;
  }

  public String getLine( int i ) {
    if( listing == null ) return null;

    if( (i<0) || (i>=listing.size()) ) return null;

    return (String)listing.elementAt( i );
  }

  public String getData() {
    if( data != null ) return data;

    StringBuffer	s = new StringBuffer();

    for( Enumeration e=listing.elements(); e.hasMoreElements(); ) {
      s.append( (String)e.nextElement() );
      s.append( "\n" );
    }

    data = s.toString();

    return data;
  }

  public String getArg( int i ) {
    if( args == null ) return "";

    if( (i<0) || (i>=args.size()) ) return "";

    return (String)args.elementAt( i );
  } 
}

