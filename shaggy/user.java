/* user.java
 * user structure
 */

public class user {
  public static final int NEEDVALID=1, PERM=4, LASTOLD=16, EXPERT=32,
    UNLISTED=64, NOPROMPT=128, DISAPPEAR=512, REGIS=1024, PAGINATOR=2048,
    INTERNET=4096, FLOORS=8192;

  String	username;
  int		access, call, msg, num;
  long		flags;

  public user( citReply r ) {
    username = r.getArg( 0 );
    access = atoi( r.getArg( 1 ) );
    call = atoi( r.getArg( 2 ) );
    msg = atoi( r.getArg( 3 ) );
    flags = atoi( r.getArg( 4 ) );
    num = atoi( r.getArg( 5 ) );
  }

  public boolean floors() {
    return (flags & FLOORS)==FLOORS;
  }

  public int atoi( String s ) {
    if( s == null ) return 0;

    try {
      return Integer.parseInt( s );
    } catch( NumberFormatException nfe ) {};
    return 0;
  }
}
