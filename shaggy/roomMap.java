/* roomMap.java
 * Keeps track of floors and rooms and all that stuff.
 */

import java.util.*;
import java.awt.List;

public class roomMap {
  Hashtable	floors, rooms;
  Vector	nrm, srm;
  List		nrmL, srmL;
  boolean	refreshed;

  public roomMap( ) {
    nrmL = null;
    srmL = null;

    nrm = new Vector();
    srm = new Vector();
    floors = null;
    rooms = null;

    refreshed = false;
  }

  public void loadFloorInfo() {
    floors = new Hashtable();
    citReply	r = citadel.me.getReply( "LFLR" );

    String	l;
    for( int i = 0; (l = r.getLine( i )) != null; i++ ) {
      floor	f = new floor( l );
      floors.put( f.num(), f );
    }
  }

  public void setList( List n, List s ) {
    nrmL = n;
    srmL = s;
    refreshed = false;
  }

  public void refresh() {
    if( refreshed ) return;
    if( (nrmL == null) || (srmL == null) ) return;
    if( floors == null ) loadFloorInfo();

    rooms = new Hashtable();

    nrm = new Vector();
    nrmL.clear(); 
    parseRooms( nrm, nrmL, citadel.me.getReply( "LKRN" ) );

    srm = new Vector();
    srmL.clear();
    parseRooms( srm, srmL, citadel.me.getReply( "LKRO" ) );
    refreshed = true;
  }
	
  public void parseRooms( Vector v, List l, citReply r ) {
    int		i=0;
    String	s;

    while( (s = r.getLine( i++) ) != null ) {
      room rm = new room( s );
      if( rm.valid() ) {
	rooms.put( rm.name(), rm );
	addRoomToFloor( rm );
	l.addItem( rm.name() );
	v.addElement( rm );
      }
    }
  }

  public void addRoomToFloor( room theRoom ) {
    if( floors == null ) return;

    floor f = getFloor( theRoom.getFloor() );
    if( f == null ) return;
    f.addRoom( theRoom );
  }

  public floor getFloor( int i ) {
    return getFloor( ""+i );
  }

  public floor getFloor( String name ) {
    return (floor)floors.get( name );
  }

  public room getRoom( String name ) {
    return (room)rooms.get( name );
  }

  public String getRoomsFloorName( String name ) {
    room	rm = getRoom( name );
    if( rm == null ) return "<bad room>";
    floor	f = getFloor( rm.getFloor() );
    if( f == null ) return "<bad floor>";
    return f.name();
  }

  public String nextNewRoom() {
    try {
      return ((room)nrm.firstElement()).name();
    } catch( Exception nsee ) {
      refreshed = false;
      return "_BASEROOM_";
    }
  }

  public void visited( String room ) {
    if( (nrmL == null) || (srmL == null) ) return;
    for( int i = 0; i < nrmL.countItems(); i++ ) {
      if( room.equals( nrmL.getItem( i ) ) ) {
	nrmL.delItem( i );
	srmL.addItem( room );
      }
    }

    for( Enumeration e = nrm.elements(); e.hasMoreElements(); ) {
      room r = (room)e.nextElement();
      if( r.is( room ) ) {
	nrm.removeElement( r );
	srm.addElement( r );
      }
    }
  }
}

class room {
  String	name;
  int		flags, floor, order;

  public room( String l ) {
    int	i = l.indexOf( '|' );
    name = l.substring( 0, i ); 

    int	j = l.indexOf( '|', ++i );
    flags = citadel.atoi( l.substring( i, j ) );

    i = l.indexOf( '|', ++j );
    floor = citadel.atoi( l.substring( j, i ) );
    order = citadel.atoi( l.substring( i+1 ) );

    /*    System.out.println( "room name: " + name );
    System.out.println( "flags    : " + flags );
    System.out.println( "floor    : " + floor );
    System.out.println( "order    : " + order );*/
  }

  public String name() {
    return name;
  }

  public int getFloor() {
    return floor;
  }

  public boolean is( String name ) {
    return this.name.equalsIgnoreCase( name );
  }

  public boolean valid() {
    return true;
  }
}

class floor {
  String	name, num;
  int		number, ref_count;
  Vector	rooms;

  public floor( String l ) {
    rooms = new Vector();

    int	i = l.indexOf( '|' );
    num = l.substring( 0, i );
    number = citadel.atoi( num );

    int	j = l.indexOf( '|', ++i );
    name = l.substring( i, j );

    ref_count = citadel.atoi( l.substring( j+1 ) );

    /*    System.out.println( "floor name: " + name );
    System.out.println( "number    : " + number );
    System.out.println( "ref_count : " + ref_count );*/
  }

  public String num() {
    return num;
  }

  public String name() {
    return name;
  }

  public void addRoom( room r ) {
    rooms.addElement( r );
  }
}
