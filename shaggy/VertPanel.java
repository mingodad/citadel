import java.awt.*;

public class VertPanel extends Panel {
  GridBagLayout		gbLayout;
  GridBagConstraints	cur;
  boolean		top = true;

  public VertPanel() {
    setLayout( gbLayout = new GridBagLayout() );

    cur = new GridBagConstraints();
    cur.fill = GridBagConstraints.BOTH;
    cur.gridwidth = GridBagConstraints.REMAINDER;
    cur.ipadx = 0;
    cur.ipady = 0;
    cur.anchor = GridBagConstraints.NORTH;
  }

  public Component add( Component c ) {
    if( top ) {
      cur.weighty = 1;
      top = false;
    } 

    gbLayout.setConstraints( c, cur );
    return super.add( c );
  }
}
