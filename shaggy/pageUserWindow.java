import javax.swing.*;
import java.awt.*;
import java.awt.event.*;

public class pageUserWindow extends JFrame {
    String	who;
    Container	c;
    boolean	done;

    public pageUserWindow() {
	c = getContentPane();
	c.setLayout( new BorderLayout() );

	JPanel	p = new JPanel();
	final JComboBox	jcb = new JComboBox();

	p.add( jcb );
	c.add( "North", jcb );
	jcb.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		who = (String)jcb.getSelectedItem();
	    } } );

	citadel.me.networkEvent( "RWHO", new CallBack() {
	    public void run( citReply r ) {
		if( !r.error() ) {
		    if( jcb.getModel().getSize() > 0 ) 
			jcb.removeAllItems();
		 
		    String	s;
		    int		i = 0;
		    while( (s = r.getLine( i++ )) != null ) {
			int	j,k;
			j = s.indexOf( "|" ) + 1;
			k = s.indexOf( "|", j );
			jcb.addItem( s.substring( j, k ) );
		    }
		}
	    } } );
	
	setUp();
    }

    public pageUserWindow( String who ) {
	this.who = who;
	c = getContentPane();
	c.setLayout( new BorderLayout() );
	c.add( "North", new JLabel( "Message to : " + who ) );

	setUp();
    }

    public void setUp() {
	JPanel	p = new JPanel();
	p.setLayout( new BorderLayout() );
	p.setBorder( BorderFactory.createTitledBorder( 
		     BorderFactory.createEtchedBorder(), "Message" ) );

	final JTextArea	t = new JTextArea();
	t.setLineWrap( true );
	t.setWrapStyleWord( true );
	p.add( "Center", t );

	c.add( "Center", p );

	p = new JPanel();
	JButton	b;
	p.add( b = new JButton( "Send" ) );
	b.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		citadel.me.networkEvent( "SEXP " + who + "|-", t.getText() );
		closeWin();
	    } } );

	p.add( b = new JButton( "Cancel" ) );
	b.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		closeWin();
	    } } );
	c.add( "South", p );

	addWindowListener( new WindowAdapter() {
	    public void windowClosing( WindowEvent e ) {
		closeWin();
	    }
	} );

	citadel.me.registerWindow( this );
	pack();
	show();
    }

    public void closeWin() {
	citadel.me.removeWindow( this );
	dispose();
    }
}
