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
