/* zapPrompt.java
 * yes/no do you really want to zap this room?
 */

public class zapPrompt extends promptCmd {
  String	name;

  public zapPrompt( String name ) {
    this.name = name;
  }

  public int getType() { return promptWindow.YES_NO; }
  public String getPrompt() { 
    return "Are you sure you want to zap " + name + "?";
  }

  public String getTitle() {
    return "Zap Confirmation";
  }

  public void yes() {
    citReply	r = citadel.me.getReply( "FORG" );
    if( r.ok() )
      citadel.me.nextNewRoom();
  }
}
