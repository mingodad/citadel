/* promptCmd.java
 * interface to make easy prompt windows
 */

public abstract class promptCmd {
  public abstract String getTitle();	/* Return the title of the window */
  public abstract String getPrompt();	/* Return the text for label */
  public abstract int getType();	/* Get type of query */

  /* actions */
  // type == YES_NO
  public void yes() {}
  // type == ONE_FIELD
  public void one_field( String s ) {}
  // type == TWO_FIELD
  public void two_fields( String s1, String s2 ) {}

  /* Override these as necessary */

  /* type == ONE_FIELD || TWO_FIELD */
  public String firstPrompt() { return ""; }
  public boolean firstEcho() { return true; }

  /* type == TWO_FIELD */
  public String secondPrompt() { return ""; }
  public boolean secondEcho() { return true; }

  /* type == YES_NO */
  public String affirm() { return "Yes"; }
  public String negate() { return "No"; }
}
