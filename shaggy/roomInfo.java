public class roomInfo {
    String	name;
    int		total, unread, info, flags, highest, highest_read;
    boolean	mail, aide;
    int		mail_num, floor;
    room	rm;

    public roomInfo( room rm, citReply r ) {
	this.rm = rm;
	name = r.getArg( 0 );
	total = citadel.atoi( r.getArg( 1 ) );
	unread = citadel.atoi( r.getArg( 2 ) );
	info = citadel.atoi( r.getArg( 3 ) );
	flags = citadel.atoi( r.getArg( 4 ) );
	highest = citadel.atoi( r.getArg( 5 ) );
	highest_read = citadel.atoi( r.getArg( 6 ) );
	mail = citadel.atoi( r.getArg( 7 ) ) != 0;
	aide = citadel.atoi( r.getArg( 8 ) ) != 0;
	mail_num = citadel.atoi( r.getArg( 9 ) ); 
	floor = citadel.atoi( r.getArg( 10 ) );
    }
}



