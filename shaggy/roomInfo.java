public class roomInfo {
  public final int PERMANENT=1, PRIVATE=4, PASSWORDED=8, GUESSNAME=16,
    DIRECTORY=32, UPLOAD=64, DOWNLOAD=128, VISDIR=256, ANONONLY=512,
    ANON2=1024, NETWORK=2048, PREFONLY=4096, READONLY=8192;

    String	name;
    int		total, unread, info, flags, highest, highest_read;
    boolean	mail, aide;
    int		mail_num, floor;
    room	rm;
  boolean perm, priv, pass, guess, dir, upload, download, vis, a_anon, 
      o_anon, net, pref, read;

    public roomInfo( room rm, citReply r ) {
	this.rm = rm;
	name = r.getArg( 0 );
	unread = citadel.atoi( r.getArg( 1 ) );
	total = citadel.atoi( r.getArg( 2 ) );
	info = citadel.atoi( r.getArg( 3 ) );
	flags = citadel.atoi( r.getArg( 4 ) );
	highest = citadel.atoi( r.getArg( 5 ) );
	highest_read = citadel.atoi( r.getArg( 6 ) );
	mail = citadel.atoi( r.getArg( 7 ) ) != 0;
	aide = citadel.atoi( r.getArg( 8 ) ) != 0;
	mail_num = citadel.atoi( r.getArg( 9 ) ); 
	floor = citadel.atoi( r.getArg( 10 ) );

	perm =    on( PERMANENT );
	priv =    on( PRIVATE );
	pass =    on( PASSWORDED );
	guess =   on( GUESSNAME ); 
	dir =     on( DIRECTORY );
	upload =  on( UPLOAD );
	download= on( DOWNLOAD );
	vis =     on( VISDIR );
	a_anon =  on( ANONONLY );
	o_anon =  on( ANON2 );
	net =     on( NETWORK ); 
	pref =    on( PREFONLY );
	read =    on( READONLY );
    }

  public boolean on( int bit ) {
    return (flags & bit ) == bit;
  }
}



