import javax.swing.*;
import java.awt.*;
import java.awt.event.*;

public class roomFrame extends JFrame {
    CardLayout		cdLayout;
    Container		c;

    roomInfo		ri;

    messagePanel	mp;
    enterPanel		ep;

    public roomFrame() {
	ri = null;
	setTitle( "Title of this window" );

	c = getContentPane();

	c.setLayout( cdLayout = new CardLayout() );
	c.add( "Messages", mp = new messagePanel(this) );
	c.add( "Compose", ep = new enterPanel(this) );

	addWindowListener( new WindowAdapter() {
	    public void windowClosing( WindowEvent e ) {
		citadel.me.rf = null;
		dispose();
	    }
	} );

	this.pack();
	this.show();
    }

    public void setRoom( roomInfo ri ) {
	this.ri = ri;

	String	title = ri.name;
	if( citadel.me.floors() ) {
	    title = title + " (" + citadel.me.rooms.getFloor( ri.rm ).name + ")";
	}

	setTitle( title );
	cdLayout.show( c, "Messages" );
	mp.refresh( ri );
    }

    public void showRoom() {
	cdLayout.show( c, "Messages" );
    }

    public void enterMessage() {
	cdLayout.show( c, "Compose" );
	ep.refresh( ri );
    }
}
