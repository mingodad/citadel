import java.util.*;

public class roomMap {
    Vector	f_list;
    Hashtable	floors, rooms;

    floor		cur_floor;
    mainPanel	mp;

    public roomMap( mainPanel mp ) {
	f_list = new Vector();
	floors = new Hashtable();
	rooms = new Hashtable();
	this.mp = mp;
    }

    public void floors( citReply r ) {
	int		i = 0;
	String	line;

	floors = new Hashtable();
    
	while( (line = r.getLine( i++ ) ) != null ) {
	    floor	f = new floor( line );
	    floors.put( f.num, f );
	    floors.put( f.name, f );
	    f_list.addElement( f );
	}
    }

    public void new_rooms( citReply rep ) {
	int		i = 0;
	String	line;

	while( (line = rep.getLine( i++ ) ) != null ) {
	    room	r, rr = new room( line );
	    r = (room)rooms.get( rr.name() );
	    if( r == null ) {
		rr.setNew();
		rooms.put( rr.name, rr );
		addToFloor( rr );
	    } else 
		r.setNew();
	}
    }

    public void old_rooms( citReply rep ) {
	int		i = 0;
	String	line;

	while( (line = rep.getLine( i++ )) != null ) {
	    room	r = new room( line );
	    rooms.put( r.name(), r );
	    addToFloor( r );
	}
    }

    public void addToFloor( room r ) {
	floor	f = (floor)floors.get( r.fname );
	if( f != null )
	    f.addRoom( r );
    }

    public floor nextFloor() {
	int	c = f_list.indexOf( cur_floor );
	c++;

	try {
	    cur_floor = (floor)f_list.elementAt( c );
	} catch( Exception e ) {
	    cur_floor = (floor)f_list.firstElement();
	}

	setFloor( cur_floor );
	return cur_floor;
    }
    
    public floor getFloor() {
	if( cur_floor == null ) cur_floor = (floor)f_list.firstElement();

	return cur_floor;
    }

  public floor getFloor( int num ) {
    return getFloor( "" +  num );
  }

    public floor getFloor( String name ) {
	return (floor)floors.get( name );
    }

    public floor getFloor( room rm ) {
	return getFloor( rm.fname );
    }

    public void setFloor( floor f ) {
	cur_floor = f;
	mp.setFloor( f );
	mp.updateLists( f.name() );
    }

    public room nextNewRoom() {
	floor	c = getFloor();

	do {
	    for( Enumeration e = getFloor().rooms.elements(); e.hasMoreElements(); ) {
		room	r = (room)e.nextElement();
		if( r.hasNew() ) return r;
	    }
	    nextFloor();
	} while( c != getFloor() );

	System.out.println( "No more new rooms... refreshing list" );

	citadel.me.networkEvent( "LKRN", new CallBack() {
	    public void run( citReply r ) {
		new_rooms( r );
		room	rm = getRoom( "Lobby" );
		setFloor( getFloor( rm ) );
	
	    } } );

	room	r = getRoom( "Lobby" );
	setFloor( getFloor( r ) );

	return r;
    }

    public room getRoom( String name ) {
	return (room)rooms.get(name);
    }

    public room forgotRoom( roomInfo ri ) {
	room	r = (room)rooms.remove( ri.name );
	if( r == null ) return nextNewRoom();
	floor	f = getFloor( ri.rm );
	if( f == null ) return nextNewRoom();
	f.rooms.removeElement( ri.rm );
	return nextNewRoom();
    }
    
}





