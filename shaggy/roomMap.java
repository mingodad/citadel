/* roomMap.java
 * Keeps track of floors and rooms and all that stuff.
 */

import java.util.*;
import java.awt.List;

public class roomMap {
  Hashtable	floors, rooms;
  SortedVector	nrm, srm;
  Vector	floor_list;
  int		cur_floor;

  List		nrmL, srmL;
  boolean	refreshed;

  public roomMap( ) {
    nrmL = null;
    srmL = null;

    nrm = new SortedVector( new roomCmp() );
    srm = new SortedVector( new roomCmp() );
    floors = null;
    rooms = null;

    refreshed = false;
  }

  public void loadFloorInfo() {
    floors = new Hashtable();
    floor_list = new Vector();
    cur_floor = 0;
    citReply	r = citadel.me.getReply( "LFLR" );

    if( r.error() ) return;

    String	l;
    for( int i = 0; (l = r.getLine( i )) != null; i++ ) {
      floor	f = new floor( l );
      floors.put( f.num(), f );
      floor_list.addElement( f );
    }
  }

  public void setList( List n, List s ) {
    nrmL = n;
    srmL = s;
    refreshed = false;
  }

  public void clear() {
    floors = null;
    nrm = null;
    srm = null;
    refreshed = false;
  }

  public void refresh() {
    if( refreshed ) return;
    if( (nrmL == null) || (srmL == null) ) return;
    if( floors == null ) loadFloorInfo();

    if( floors != null ) {
      for( Enumeration e = floor_list.elements(); e.hasMoreElements(); ) {
	floor	f = (floor)e.nextElement();
	f.rooms = new SortedVector( new roomCmp() );
      }
    }

    rooms = new Hashtable();

    nrm = new SortedVector( new roomCmp() );
    nrmL.clear(); 
    parseRooms( nrm, nrmL, citadel.me.getReply( "LKRN" ), true );

    srm = new SortedVector( new roomCmp() );
    srmL.clear();
    parseRooms( srm, srmL, citadel.me.getReply( "LKRO" ), false );
    refreshed = true;
  }
	
  public void parseRooms( SortedVector v, List l, citReply r, boolean nmsgs ) {
    int		i=0;
    String	s;

    while( (s = r.getLine( i++) ) != null ) {
      room rm = new room( s );
      rm.setNew( nmsgs );
      if( rm.valid() ) {
	rooms.put( rm.name(), rm );
	addRoomToFloor( rm );
	v.addElement( rm );
      }
    }
 
    for( Enumeration e=v.elements(); e.hasMoreElements(); ) {
      room rm = (room)e.nextElement();
      l.addItem( rm.name() );
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
      if( nrm.size() <= 1 ) refreshed = false;

      if( citadel.me.floors() ) {
	String	name = null;
	while( name == null ) {
	  floor	f = (floor)floor_list.elementAt( cur_floor );
	  name = f.nextNewRoom();
	  if( name == null ) cur_floor++;
	}
	return name;
      }

      /* no floors */
      return ((room)nrm.firstElement()).name();

    } catch( Exception nsee ) {
      cur_floor = 0;
      refreshed = false;
      return "_BASEROOM_";
    }
  }

  public void visited( String rm ) {
    if( (nrmL == null) || (srmL == null) ) return;
    for( int i = 0; i < nrmL.countItems(); i++ ) {
      if( rm.equals( nrmL.getItem( i ) ) ) {
	nrmL.delItem( i );
	srmL.addItem( rm );
      }
    }

    room r = getRoom( rm );
    if( r == null ) return;

    r.setNew( false );
    if( nrm.removeElement( r ) != -1 )
      srm.addElement( r );
  }
}

class roomCmp extends sorter {
  public int cmp( Object o1, Object o2 ) {
    room	r1 = (room)o1;
    room	r2 = (room)o2;

    /* Do I want to sort on floors here, even if users don't use it? */

    if( r1.order < r2.order ) return -1;
    else if( r1.order == r2.order )
      return r1.name().compareTo( r2.name() );
    return 1;
  }
}

class room {
  String	name;
  int		flags, floor, order;
  boolean	nmsgs;

  public room( String l ) {
    nmsgs = false;

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

  public void  setNew( boolean nmsgs ) {
    this.nmsgs = nmsgs;
  }

  public boolean getNew() {
    return nmsgs;
  }
}

class floor {
  String	name, num;
  int		number, ref_count;
  SortedVector	rooms;

  public floor( String l ) {
    rooms = new SortedVector( new roomCmp() );

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

  public int number() {
    return number;
  }

  public String name() {
    return name;
  }

  public void addRoom( room r ) {
    if( rooms == null ) rooms = new SortedVector( new roomCmp() );
    if( !rooms.isElement( r ) )
      rooms.addElement( r );
  }

  public String nextNewRoom() {
    for( Enumeration e = rooms.elements(); e.hasMoreElements(); ) {
      room	r = (room)e.nextElement();
      if( r.getNew() ) return r.name();
    }
    return null;
  }
}
