/*
 * MultiUserChat102.java
 *
 * Chat mode...
 *
 */

import java.awt.*;
import java.util.*;

/*
 * ReceiveChat implements a thread which listens on the server socket and
 * display anything which comes across the wire in the Panel specified
 * to the constructor.
 */
class ReceiveChat extends Thread {
	Panel TheArea;
	wcCitServer serv;
	String boof;
	String cUser;
	String cText;
	String ThisLine;
	String LastLineUser;
	String MyName;
	StringTokenizer ST;
	MultiUserChat102 ParentMUC;
	 Label[] Linez = new Label[25];
	int a;

	 ReceiveChat(MultiUserChat102 muc, Panel t, wcCitServer s,
		     String n) {
		TheArea = t;
		serv = s;
		MyName = n;
		ParentMUC = muc;
		serv.AddClientThread(this);

		TheArea.setLayout(new GridLayout(25, 1));

		for (a = 0; a < 25; ++a) {
			Linez[a] = new Label(" ");
			Linez[a].setBackground(Color.black);
			Linez[a].setForeground(Color.black);
			TheArea.add(Linez[a]);
		} TheArea.validate();

	}

	private void ScrollIt(String TheNewLine, Color NewLineColor) {
		for (a = 0; a < 24; ++a) {
			Linez[a].setText(Linez[a + 1].getText());
			Linez[a].setForeground(Linez[a + 1].
					       getForeground());
		}
		Linez[24].setText(TheNewLine);
		Linez[24].setForeground(NewLineColor);
	}


	public void run() {
		Color UserColor;
		int a;

		LastLineUser = "  ";
		while (true) {
			boof = serv.ServGets();

			if (boof.equals("000")) {
				serv.ServPuts("QUIT");
				ParentMUC.dispose();
				stop();
				destroy();
			}


			ST = new StringTokenizer(boof, "|");
			if (ST.hasMoreTokens()) {
				cUser = ST.nextToken();
			} else {
				cUser = ":";
			}
			if (ST.hasMoreTokens()) {
				cText = ST.nextToken();
			} else {
				cText = " ";
			}
			if (!cText.startsWith("NOOP")) {
				if (!LastLineUser.equals(cUser)) {
					ScrollIt("", Color.black);
					ThisLine = cUser + ": ";
				} else {
					ThisLine =
					    "                                  ".
					    substring(0,
						      cUser.length() + 2);
				}
				ThisLine = ThisLine + cText;
				UserColor = Color.green;
				if (cUser.equals(":")) {
					UserColor = Color.red;
				}
				if (cUser.equalsIgnoreCase(MyName)) {
					UserColor = Color.yellow;
				}
				ScrollIt(ThisLine, UserColor);
				LastLineUser = cUser;
			}
		}
	}

}





public class MultiUserChat102 extends Frame {

	wcCitServer serv;
	ReceiveChat MyReceive;
	Panel AllUsers;
	TextField SendBox;
	wcchat ParentApplet;


	 MultiUserChat102(wcCitServer PrimaryServ, wcchat P) {
		super("Multiuser Chat");

		String boof;

		/* Set up a new server connection as a piggyback to the first. */
		 serv = PrimaryServ;
		 ParentApplet = P;

		 resize(600, 400);
		 setLayout(new BorderLayout());

		 boof = "This is the buffer before the chat command.";
		 serv.ServPuts("CHAT");
		 boof = serv.ServGets();

		if (boof.charAt(0) != '8') {
			add("Center", new Label("ERROR: " + boof));
			show();
		}

		else {
			DoChat(PrimaryServ.GetUserName());
		}

	}


/*
 * Do the actual chat stuff
 */
	private void DoChat(String MyName) {
		String boof;

		SendBox = new TextField(80);

		AllUsers = new Panel();

		add("Center", AllUsers);
		add("South", SendBox);
		show();

		MyReceive = new ReceiveChat(this, AllUsers, serv, MyName);
		MyReceive.start();

		SendBox.requestFocus();
	}


	public boolean handleEvent(Event evt) {
		int LastSpace;

		if ((evt.target == SendBox)
		    && (evt.id == Event.ACTION_EVENT)) {
			serv.ServPuts(SendBox.getText());
			SendBox.setText("");
		}

		else if (evt.target == SendBox) {
			if (SendBox.getText().length() +
			    serv.GetUserName().length() > 78) {
				LastSpace =
				    SendBox.getText().lastIndexOf(' ');
				if (LastSpace < 0) {
					serv.ServPuts(SendBox.getText());
					SendBox.setText("");
				} else {
					serv.ServPuts(SendBox.getText().
						      substring(0,
								LastSpace));
					SendBox.setText(SendBox.getText().
							substring
							(LastSpace));
					if (SendBox.getText().charAt(0) ==
					    ' ') {
						SendBox.setText(SendBox.
								getText().
								substring
								(1));
					}
				}
			}
		}

		return super.handleEvent(evt);
	}


}
