/* gotoPrompt.java
 * prmoptWindow stuff for .goto
 */

public class gotoPrompt extends promptCmd {
  public String	def;

  public gotoPrompt( String def ) {
  }

  public void one_field( String name ) {
    citadel.me.enterRoom( name );
  }

  public int getType() { return promptWindow.ONE_FIELD; }
  public String getPrompt() { return "Room name"; }
  public String getTitle() { return "Goto a room"; }

  public String firstPrompt() { return "Room:"; }
}
