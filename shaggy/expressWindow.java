import javax.swing.*;
import java.awt.*;
import java.awt.event.*;

public class expressWindow extends JFrame {
    String	who;

    public expressWindow( citReply r ) {
	who = r.getArg( 3 );
	setTitle( who + " : express message" );

	JPanel	p = new JPanel();
	p.setLayout( new BorderLayout() );
	p.setBorder( BorderFactory.createTitledBorder( 
		     BorderFactory.createEtchedBorder(), "Message" ) );

	JTextArea	t = new JTextArea( r.getData() );
	t.setLineWrap( true );
	t.setWrapStyleWord( true );

	p.add( "Center", new JScrollPane( t ) );

	Container	c = getContentPane();
	c.setLayout( new BorderLayout() );
	c.add( "Center", p );

	p = new JPanel();

	JButton	b;
	p.add( b = new JButton( "Reply" ) );
	b.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		new pageUserWindow( who );
		closeWin();
	    } } );

	p.add( b = new JButton( "Close" ) );
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

	pack();
	show();
	citadel.me.registerWindow( this );
    }

    public void closeWin() {
	citadel.me.removeWindow( this );
	dispose();
    }
}
