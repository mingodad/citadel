import javax.swing.*;
import java.awt.*;
import java.awt.event.*;

public class hostPanel extends JPanel {
  JTextField	h_name, dns_name, port, user, pass;

  public hostPanel() {
    super( new BorderLayout() );

    String[]	data = { 
      "127.0.0.1",
      "uncnsrd.mt-kisco.ny.us"
    };

    JPanel	p = new JPanel();
    p.setBorder( BorderFactory.createTitledBorder( 
		  BorderFactory.createEtchedBorder(), "Nodes" ) );

    final JList	theList;

    p.setLayout( new BorderLayout() );
    p.add( "Center", new JScrollPane( theList = new JList( data ) ) );
    add( "West", p );

    MouseListener mouseListener = new MouseAdapter() {
      public void mouseClicked(MouseEvent e) {
	String	where = (String)theList.getSelectedValue();
	if( where == null ) return;
	dns_name.setText( where );
	if (e.getClickCount() == 2) {
	  citadel.me.setServer( where, port.getText() );
	  citadel.me.showLoginPanel( user.getText(), pass.getText() );
	}
      }
    };
    theList.addMouseListener(mouseListener);


    //    theList.setPrototypeCellValue("1 and 1 and 1 is 3");

    PairPanel	pp = new PairPanel(3, 10);
    pp.addLeft( new JLabel( "BBS Name:" ) );
    pp.addRight( h_name = new JTextField( 20 ) );

    pp.addLeft( new JLabel( "IP Address:" ) );
    pp.addRight( dns_name = new JTextField( 20 ) );
    dns_name.setText( "127.0.0.1" );
   
    pp.addLeft( new JLabel( "Port:" ) );
    pp.addRight( port = new JTextField( "504" ) );

    pp.addLeft( new JLabel( "Username:" ) );
    pp.addRight( user = new JTextField( 10 ) );

    pp.addLeft( new JLabel( "Password:" ) );
    pp.addRight( pass = new JPasswordField( 10 ) );
    add( "Center", pp );

    JButton	but = new JButton( "Connect" );
    but.addActionListener(new ActionListener() {
      public void actionPerformed( ActionEvent e ) {
	citadel.me.setServer( dns_name.getText(), port.getText() );
	citadel.me.showLoginPanel( user.getText(), pass.getText() );
      }
    });

    JPanel	pane = new JPanel();
    pane.setBorder(BorderFactory.createEmptyBorder(30, //top
						   30, //left	
						   10, //bottom
						   30) );//right

    pane.add( but );
    add( "South", pane );
  }

  public void refresh() {
  }
}


