import javax.swing.*;
import java.awt.*;
import java.awt.event.*;
import java.util.*;

public class whoOnlineWindow extends JFrame {
    DefaultListModel	users;
    JList		theList;
    boolean		refresh=false;

    public whoOnlineWindow() {
	setTitle( "Who is online" );

	JPanel	p = new JPanel();
	p.setLayout( new BorderLayout() );
	p.setBorder( BorderFactory.createTitledBorder( 
						      BorderFactory.createEtchedBorder(), "Users" ) );

	theList = new JList();
	users = new DefaultListModel();
	theList.setModel( users );

	theList.addMouseListener( new MouseAdapter() {
	    public void mouseClicked( MouseEvent e ) {
		if( refresh) return;
		int	i = theList.getSelectedIndex();
		if( i >= 0 )
		    new pageUserWindow( unpad( (String)users.elementAt( i )) );
	    } } );

	p.add( "Center", new JScrollPane( theList ) );
	
	getContentPane().setLayout( new BorderLayout() );
	getContentPane().add( "Center", p );

	p = new JPanel();
	JButton	b;
	p.add( b = new JButton( "Refresh" ) );
	b.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		refresh();
	    } } );

	p.add( b = new JButton( "Close" ) );
	b.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		closeWin();
	    } } );

	getContentPane().add( "South", p );

	addWindowListener( new WindowAdapter() {
	    public void windowClosing( WindowEvent e ) {
		closeWin();
	    } } );

	citadel.me.registerWindow( this );
	refresh();
	pack();
	show();
    }

    public void closeWin() {
	citadel.me.removeWindow( this );
	dispose();
    }

    public void refresh() {
	citadel.me.networkEvent( "RWHO", new CallBack() {
	    public void run( citReply r ) {
		refresh = true;

		users.removeAllElements();

		int		i = 0;
		String	s;
		while( (s = r.getLine( i++ )) != null ) {
		    int	j = s.indexOf( '|' ) + 1;
		    int	k = s.indexOf( '|', j );
		    int	l = s.indexOf( '|', k + 1 );
		    String	user = s.substring( j, k );
		    String	room = s.substring( k+1, l );
		    users.addElement( pad( user, room ) );
		}
	    refresh=false;
	    }
	} );
    }

    public String pad( String u, String r ) {
	StringBuffer	s = new StringBuffer( u );
	while( s.length() < 30 )
	    s.append( ' ' );
	s.append( r );
	return s.toString();
    }

    public String unpad( String p ) {
	StringBuffer	s = new StringBuffer( p.substring( 0, Math.min( 29, p.length() ) ) );
	while( s.charAt( s.length() - 1) == ' ' )
	    s.deleteCharAt( s.length() -1 );
	return s.toString();
    }
	

}
