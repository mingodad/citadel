/* server.java
 * server info data structure
 */

public class server {
  int		session_id;
  String	node_name, human_name, fqdn, server_name;
  int		rev_level;
  String	geo_local, sysadmin;
  int		server_type;
  String	page_prompt;
  int		floor_flag, page_level;

  String	blurb;

  public server( citReply r ) {
    session_id = citadel.atoi( r.getLine( 0 ) );
    node_name = r.getLine( 1 );
    human_name = r.getLine( 2 );
    fqdn = r.getLine( 3 );
    server_name = r.getLine( 4 );
    rev_level = citadel.atoi( r.getLine( 5 ) );
    geo_local = r.getLine( 6 );
    sysadmin = r.getLine( 7 );
    server_type = citadel.atoi( r.getLine( 8 ) );
    page_prompt = r.getLine( 9 );
    floor_flag = citadel.atoi( r.getLine( 10 ) );
    page_level = citadel.atoi( r.getLine( 11 ) );

    blurb = server_name + " " + human_name + " " + geo_local;
    System.out.println( blurb );
  }
}
