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
  private citPanel			cp;

  boolean			floors;
  whoWindow			wo;
  roomMap			rooms;

  public static citadel	me;

  public static void main( String args[] ) {
    citadel cb = new citadel( false );
    if( args.length > 0 )
      cb.openConnection( args[0] );
    else
      cb.openConnection();
    citFrame cf = new citFrame();
  }

  public static int atoi( String s ) {
    try {
      return Integer.parseInt( s );
    } catch( Exception e ) {
      return 0;
    }
  }

  public citadel( boolean applet ) {
    me = this;
    this.applet = false;
    theUser = null;
    wo = null;
    rooms = new roomMap();
  }

  public void setCitPanel( citPanel cp ) {
    this.cp = cp;
  }

  public void lostNetwork( String reason ) {
    theNet = null;
    if( cp == null )
      System.out.println( "lost network connection:" + reason );
    else
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
    citReply rep = getReply( "MESG " + name );
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
    return getReply( s, (String)null );
  }

  public citReply getReply( String s, String d ) {
    if( theNet == null ) return null;

    return theNet.getReply( s,d  );
  }

  public void enterRoom( String room ) {
    enterRoom( room, null );
  }

  public void gotoRoom( ) {
    gotoRoom( null, false );
  }

  public void gotoRoom( String name, boolean flag ) {
    /* TODO: prompt for room name */
    System.out.println( "This is where I would ask you for the room's name" );
  }

  public void enterRoom( String room, String pass ) {
    String cmd = "GOTO " + room;
    if( pass != null )
      cmd = cmd + " " + pass;
    citReply	r=getReply( cmd );
    if( r.ok() ) {
      rooms.visited( room );
      cp.enterRoom( r );
    } else if( r.res_code == 540 ) /* ERROR+PASSWORD_REQUIRED */
      new passwordWindow( room );
  }

  public void showMsgPane() {
    cp.deck.show( cp, "Message" );
  }

  public void login() {
    cp.login();
  }

  public void mainMenu() {
    cp.mainMenu();
  }

  public void enterMsg( String room ) {
    cp.enterMsg( room );
  }

  public void nextNewRoom() {
    enterRoom( rooms.nextNewRoom() );
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

    citReply	r = getReply( cmd, body );
    if( r.error() )
      error( r );
  }

  public void logoff() {
    cp.logoff(null);
    getReply( "QUIT" );
  }

  public void who_online() {
    if( wo == null )
      wo = new whoWindow();
    else
      wo.show();
  }

  public void page_user() {
    page_user( null );
  }

  public void page_user( String who ) {
    new pageWindow( who );
  }

  public void error( citReply r ) {
    System.out.println( r.line );
  }
}
