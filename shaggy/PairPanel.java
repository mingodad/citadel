/* PairPanel.java
 * Utility class so I don't have to think about GridBagLayout
 */

import java.awt.*;

public class PairPanel extends Panel {
  GridBagLayout           gbLayout;
  GridBagConstraints      gbLeft, gbRight;

  PairPanel() {
    this( 0, 0 );
  }

  PairPanel( int x, int y ) {
    Insets     	i = null;

    if( x + y != 0 )
      i = new Insets( x, y, x, y );

    setLayout( gbLayout = new GridBagLayout() );

    gbLeft = new GridBagConstraints();
    gbLeft.gridwidth = 1;
    //    gbLeft.ipadx = x;
    //    gbLeft.ipady = y;
    if( i != null )
      gbLeft.insets = i;
    gbLeft.anchor = GridBagConstraints.EAST;

    gbRight = new GridBagConstraints();
    //    gbRight.ipadx = x;
    //    gbRight.ipady = y;
    if( i != null )
      gbRight.insets = i;
    gbRight.gridwidth = GridBagConstraints.REMAINDER;
    gbRight.anchor = GridBagConstraints.WEST;
  }

  public void addLeft( Component c ) {
    gbLayout.setConstraints( c, gbLeft );
    add( c );
  }

  public void addRight( Component c ) {
    gbLayout.setConstraints( c, gbRight );
    add( c );
  }
}
