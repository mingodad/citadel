public class MsgCmd implements Runnable {
  String	cmd, data;
  CallBack	cb;
  citReply	r;

  public MsgCmd( String cmd, String data, CallBack cb ) {
    this.cmd = cmd;
    this.data = data;
    this.cb = cb;
  }

  public void setReply( citReply r ) {
    this.r = r;
  }

  public void run() {
    if( cb != null )
      cb.run( r );
  }
}
