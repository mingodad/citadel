import java.util.*;

public class floor {
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

    public void addRoom( room r ) {
	/*    System.out.println( "adding " + r.name() + " to " + name ); */
	rooms.addElement( r );
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
}


