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
    access = citadel.atoi( r.getArg( 1 ) );
    call = citadel.atoi( r.getArg( 2 ) );
    msg = citadel.atoi( r.getArg( 3 ) );
    flags = citadel.atoi( r.getArg( 4 ) );
    num = citadel.atoi( r.getArg( 5 ) );
  }

  public boolean floors() {
    return (flags & FLOORS)==FLOORS;
  }
}
