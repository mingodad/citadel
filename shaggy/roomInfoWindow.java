import javax.swing.*;
import java.awt.*;
import java.awt.event.*;

public class roomInfoWindow extends JFrame {
    roomInfo	ri;
    JTextArea	info;


    public roomInfoWindow( roomInfo ri ) {
	Container	c;

	setTitle( "Info for " + ri.name );
	this.ri = ri;

	JPanel	p = new JPanel();
	p.setLayout( new BorderLayout() );
	p.setBorder( BorderFactory.createTitledBorder( 
						      BorderFactory.createEtchedBorder(), ri.name ) );

	p.add( "Center", 
	       new JScrollPane( info = new JTextArea(),
				JScrollPane.VERTICAL_SCROLLBAR_AS_NEEDED,
				JScrollPane.HORIZONTAL_SCROLLBAR_NEVER ) );

	info.setEditable( false );
	info.setLineWrap( true );
	info.setWrapStyleWord( true );
	
	c = getContentPane();
	c.setLayout( new BorderLayout() );
	c.add( "Center", p );
	
	p = new JPanel();
	JButton	b;
	p.add( b = new JButton( "Close" ) );
	c.add( "South", p );
	b.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		closeWin();
	    } } );

	addWindowListener( new WindowAdapter() {
	    public void windowClosing( WindowEvent e ) {
		closeWin();
	    } } );

	pack();
	show();

	citadel.me.registerWindow( this );
	citadel.me.networkEvent( "RINF", new CallBack() {
	    public void run( citReply r ) {
		if( r.error() ) info.setText( r.getArg(0) );
		else info.setText( r.getData() );
	    } } );
    }

    public void closeWin() {
	citadel.me.removeWindow( this );
	dispose();
    }

}







