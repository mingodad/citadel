import java.util.*;

public class room {
  String	name, fname;
  int		flags, floor, order;
  boolean	nmsgs;

  public room( String l ) {
    nmsgs = false;

    int	i = l.indexOf( '|' );
    name = l.substring( 0, i ); 

    int	j = l.indexOf( '|', ++i );
    flags = citadel.atoi( l.substring( i, j ) );

    i = l.indexOf( '|', ++j );
    floor = citadel.atoi( fname = l.substring( j, i ) );
    order = citadel.atoi( l.substring( i+1 ) );

    /*    System.out.println( "room name: " + name );
    System.out.println( "flags    : " + flags );
    System.out.println( "floor    : " + floor );
    System.out.println( "order    : " + order );*/
  }

  public room( String name, String floor ) {
    this.name = name;
    this.fname = floor;
    this.floor = citadel.atoi( floor );
  }

  public boolean hasNew() {
    return nmsgs;
  }

  public void setNew() {
    setNew( true );
  }

  public void setNew( boolean val) {
    nmsgs = val;
  }

  public String name() {
    return name;
  }
}
