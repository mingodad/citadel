import javax.swing.*;
import java.awt.*;
import java.awt.event.*;

public class enterRoomWindow extends JFrame {
    JTextField	room, pass;

    public enterRoomWindow() {
	this( null );
    }

    public enterRoomWindow( String def ) {
	Container	c = getContentPane();
	pass = null;

	addWindowListener( new WindowAdapter() {
	    public void windowClosing( WindowEvent e ) {
		closeWin();
	    } } );

	c.setLayout( new BorderLayout() );

	PairPanel	pp = new PairPanel();
	pp.addLeft( new JLabel( "Name:" ) );
	pp.addRight( room = new JTextField(10) );
	room.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		pass.requestFocus();
	    } } );

	if( def != null ) {
	    room.setText( def );

	    pp.addLeft( new JLabel( "Password: " ) );
	    pp.addRight( pass = new JPasswordField(10) );

	    pass.addActionListener( new ActionListener() {
		public void actionPerformed( ActionEvent e ) {
		    enterRoom();
		} } );
	}

	c.add( "Center", pp );

	JPanel	p = new JPanel();
	JButton	b = new JButton( "Go" );
	p.add( b );

	b.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		enterRoom();
	    } } );

	p.add( b = new JButton( "Cancel" ) );
	b.addActionListener( new ActionListener() {
	    public void actionPerformed( ActionEvent e ) {
		closeWin();
	    } } );

	c.add( "South", p );

	citadel.me.registerWindow( this );
	pack();
	show();
    }

    public void closeWin() {
	citadel.me.removeWindow( this );
	dispose();
    }

    public void enterRoom() {
	String	r, p=null;
	r = room.getText();
	if( pass != null ) p = pass.getText();
	citadel.me.enterRoom( r, p );
	closeWin();
    }
}
