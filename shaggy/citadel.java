/* citadel.java
 *
 * the "main" object
 */

public class citadel {
  String			host;
  boolean			applet;
  net				theNet;
  server			serverInfo;
  user				theUser;
  citPanel			cp;

  boolean			floors;

  public static citadel	me;

  public static void main( String args[] ) {
    citadel cb = new citadel( false );
    if( args.length > 0 )
      cb.openConnection( args[0] );
    else
      cb.openConnection();
    citFrame cf = new citFrame();
  }

  public citadel( boolean applet ) {
    me = this;
    this.applet = false;
    theUser = null;
  }

  public void lostNetwork( String reason ) {
    theNet = null;
    cp.logoff( "lost network connection: " + reason );
  }

  public boolean openConnection( ) {
    return openConnection( "127.0.0.1" );
  }

  public boolean openConnection( String host ) {
    this.host = host;
    if( theNet == null ) theNet = new net( );

    if( theNet.connect(host) ) {
      System.out.println( "Connected to server." );

      citReply rep = theNet.getReply( "INFO" );
      if( rep.listingFollows() )
	serverInfo = new server( rep );

      return true;
    }
    else {
      System.out.println( "Couldn't connect to server." );
    }
    return false;
  }

  public String getBlurb() {
    if( serverInfo != null ) return serverInfo.blurb;
    return "";
  }

  public String getSystemMessage( String name ) {
    citReply rep = theNet.getReply( "MESG " + name );
    if( rep.listingFollows() )
      return rep.getData();
    else
      return "Couldn't find " + name;
  }

  public void loggedIn( citReply r ) {
    theUser = new user( r );
    floors = serverInfo.floor_flag != 0;
    floors &= theUser.floors();
    cp.mainMenu();
  }

  public citReply getReply( String s ) {
    if( theNet == null ) return null;

    return theNet.getReply( s );
  }

  public void enterRoom( String room ) {
    enterRoom( room, null );
  }

  public void enterRoom( String room, String pass ) {
    String cmd = "GOTO " + room;
    if( pass != null )
      cmd = cmd + " " + pass;
    citReply	r=getReply( cmd );
    if( r.ok() ) {
      cp.mp.visited( room );
      cp.enterRoom( r );
    } else if( r.res_code == 540 ) /* ERROR+PASSWORD_REQUIRED */
      new passwordWindow( room );
  }

  public void enterMsg( String room ) {
    cp.enterMsg( room );
  }

  public void nextNewRoom() {
    enterRoom( cp.mp.nextNewRoom() );
  }

  public void expressMsg() {
    citReply	r=getReply( "GEXP" );
    System.out.println( "EXPRESS MSG" );
    if( !r.error() ) new expressWindow( r );
  }

  public void sendMessage( String body, String rec, boolean mail ) {
    String cmd = "ENT0 1|";
    if( mail ) cmd = cmd + rec;
    cmd = cmd + "|0|0|0";

    citReply	r = getReply( cmd );
    if( r.sendListing() ) {
      theNet.println( body );
      theNet.println( "000" );
    }
  }

  public void logoff() {
    cp.logoff(null);
    theNet.println( "quit" );
  }

  public void who_online() {
    System.out.println( "Who, pray tell, is online?" );
  }

  public void page_user() {
    page_user( null );
  }

  public void page_user( String who ) {
    new pageWindow( who );
  }
}
