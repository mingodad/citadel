/* SortedVector.java
 * insert-sort vector (default to strings)
 */

import java.awt.*;
import java.util.*;

public class SortedVector {
  Vector		theList;
  sorter		cmpr;
	
  public SortedVector() {
    theList = new Vector();
    cmpr = null;
  }

  public SortedVector( sorter s ) {
    theList = new Vector();
    cmpr = s;
  }
	
  public int addElement( Object theId ) {
    int		i, cmp;
    for( i = 0; i < theList.size(); i++ ) {

      if( cmpr == null )
	cmp = ((String)theList.elementAt(i)).compareTo( (String)theId );
      else
	cmp = cmpr.cmp( theList.elementAt( i ), theId );

      if( cmp == 0 ) return -1;
      if( cmp > 0 )
	break;
    }
		
    theList.insertElementAt( theId, i );
    return i;
  }

  public Object firstElement() {
    return theList.firstElement();
  }
	
  public Object removeItem( int index ) {
    if( (index < 0) || (index > theList.size()) )
      return null;
		
    Object	theItem = theList.elementAt( index );
    theList.removeElementAt( index );
    return theItem;
  }
	
  public int removeElement( Object theItem ) {
    for( int i = 0; i < theList.size(); i++ ) {
      boolean equal;
      if( cmpr == null )
	equal = ((String)theList.elementAt( i )).equalsIgnoreCase( (String)theItem );
      else
	equal = cmpr.cmp( theList.elementAt( i ), theItem ) == 0;
      if( equal ) {
	theList.removeElementAt( i );
	return i;
      }
    }
    return -1;
  }
	
  public boolean isElement( Object theItem ) {
    for( int i = 0; i < theList.size(); i++ ) {
      if( cmpr == null ) {
	if( ((String)theList.elementAt( i )).equalsIgnoreCase( (String)theItem ) )
	  return true;
      } else {
	if( cmpr.cmp( theList.elementAt( i ), theItem ) == 0 )
	  return true;
      }
    }
    return false;
  }
	
  public void removeAllElements() {
    theList.removeAllElements();
  }

  public Enumeration elements() {
    return theList.elements();
  }

  public int size() {
    return theList.size();
  }
}

