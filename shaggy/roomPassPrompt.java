/* roomPassPrompt.java
 * when a room has a password
 */

public class roomPassPrompt extends promptCmd {
  String	room;

  public roomPassPrompt( String room ) {
    this.room = room;
  }

  public int getType() { return promptWindow.ONE_FIELD; }
  public String getTitle() { return "Password for \"" + room + "\""; }
  public String getPrompt() { return room+ "'s password"; }

  public String firstPrompt() { return  "Password:"; };
  public boolean firstEcho() { return false; }

  public void one_field( String pass ) {
    citadel.me.enterRoom( room, pass );
  }
}
