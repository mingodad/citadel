/*
 * wcCitServer.java
 *
 * This module handles the tasks involving establishing a connection to a
 * Citadel/UX server and moving data across the connection.  It also handles
 * server "keepalives", the dispatching of "express messages", the 
 * registration & termination of multiple threads, and a semaphore mechanism
 * to prevent multiple threads from executing server commands at the same
 * time.
 */

import java.net.*;
import java.io.*;
import java.util.*;

/*
 * We send 'keepalive' commands (actually just a NOOP) to the server every
 * fifteen seconds, as a separate thread.  This prevents the server session
 * from timing out, and also allows the client to be notified of any incoming
 * express messages if the user isn't doing anything.
 */
class wcKeepAlive extends Thread {

	wcCitServer serv;		/* Pointer to server connection class */
	boolean FullKeepAlives;	/* TRUE for full keepalives, FALSE for half keepalives */

	wcKeepAlive() {
		}

	public void PointToServer(wcCitServer which_serv, boolean WhatKindOfKeepAlives) {
		serv = which_serv;
		serv.AddClientThread(this);
		FullKeepAlives = WhatKindOfKeepAlives;
		}

	public void run() {
		String buf;
		while(true) {

			/* Sleep for sixty seconds between keepalives. */
			try {
				sleep(60000);
				}
			catch (InterruptedException e) {
				}
			/* Full keepalives - send a NOOP, wait for a reply, then retrieve
			 * express messages if the server said there are any.
			 */
			if (FullKeepAlives) {
				buf = serv.ServTrans("NOOP") + "    ";
				}

			/* Half keepalives - blindly send a NOOP and we're done. */
			else {
				serv.ServPuts("NOOP");
				}

			}
		}
		
	}



/*
 * the wcCitServer class handles communication with the server.
 */
public class wcCitServer {
	int connected = 0;
	DataOutputStream ofp;
	DataInputStream ifp;
	Socket sock;
	String TransBuf;
	wcKeepAlive ka;
	boolean in_trans = false;
	Vector ClientThreads = new Vector();
	Vector ServerInfo = new Vector();
	String PrimaryServerHost;
	int PrimaryServerPort;
	String PrimaryServerUser;
	String PrimaryServerPassword;

	public void SetTransBuf(String bufstring) {
		TransBuf = bufstring;
		}

	public String GetTransBuf() {
		return TransBuf;
		}

/* attach to the server */
	private void BuildConnection(String ServerHost, int ServerPort, boolean WhichKA) {

		String buf;

		try {
			sock = new Socket(ServerHost, ServerPort);
			ofp = new
				DataOutputStream(sock.getOutputStream());
			ifp = new
				DataInputStream(sock.getInputStream());
			}
		catch(UnknownHostException e) {
			System.out.println(e);
			}
		catch(IOException e) {
			System.out.println(e);
			}

		/* Connection established.  At this point, this function
		 * has the server connection all to itself, so we can do
		 * whatever we want with it. */
	
		/* Get the 'server ready' message */	
		buf = ServGets();

		/* Identify the client software to the server */
		ServTrans("IDEN 0|5|001|Cit/UX Java Client|");
		
		/* Download various information about the server */
		SetTransBuf("");
		buf = ServTrans("INFO");
		StringTokenizer InfoST = new StringTokenizer(TransBuf,"\n");
		while (InfoST.hasMoreTokens()) {
			ServerInfo.addElement(InfoST.nextToken());
			}


		/* At this point, all server accesses must cooperate with
		 * server accesses from other threads by using the BeginTrans
		 * and EndTrans semaphore mechanism.
		 */
		ka = new wcKeepAlive();
		ka.PointToServer(this, WhichKA);
		ka.start();

		}


/*
 * Attach to server command for the primary socket
 */
	public void AttachToServer(String ServerHost, int ServerPort) {
		
		/* Connect to the server */
/* NOTE ... we've changed the primary connection keepalives to HALF because we're using the
 * primary connection to jump right into a chat window.
 */
		BuildConnection(ServerHost, ServerPort, false);

		/* And remember where we connected to, in case we have to
		 * build any piggyback connections later on.
		 */
		PrimaryServerHost = ServerHost;
		PrimaryServerPort = ServerPort;
		}


/* 
 * Learn info about the primary connection for setting up piggybacks
 */
	public String GetServerHost() {
		return PrimaryServerHost;
		}

	public int GetServerPort() {	
		return PrimaryServerPort;
		}


/*
 * Set up a piggyback connection
 */
	public void Piggyback(wcCitServer PrimaryServ, boolean KeepAliveType) {
		PrimaryServerHost = PrimaryServ.GetServerHost();
		PrimaryServerPort = PrimaryServ.GetServerPort();
		PrimaryServerUser = PrimaryServ.GetUserName();
		PrimaryServerPassword = PrimaryServ.GetPassword();
		BuildConnection(PrimaryServerHost, PrimaryServerPort,
				KeepAliveType);
		
		}



/* return info about the site we're currently connected to */
	public String ServInfo(int index) {
		String retbuf;
		if (index >= ServerInfo.size()) {
			return("");
			}
		else {
			retbuf = ServerInfo.elementAt(index).toString();
			return(retbuf);
			}
		}


/* read a line from the server */
	public String ServGets() {
		
		String buf = "";

		try {
			buf = ifp.readLine();
			}
		catch(IOException e) {
			System.out.println(e);
			}
		return buf;
		}


/* write a line to the server */
	public void ServPuts(String buf) {

		try {
			ofp.writeBytes(buf + "\n");
			}
		catch(IOException e) {
			System.out.println(e);
			}
		}


/* lock the server connection so other threads don't screw us up */
	public synchronized void BeginTrans() {
		while(in_trans) {
			try {
				System.out.println("-sleeping-"); /* trace */
				Thread.sleep(100);
				}
			catch (InterruptedException e) {
				}
			}

		in_trans = true;
		}

/* release the lock */
	public void EndTrans() {
		in_trans = false;
		}


/* perform an autonomous server transaction */
	public String ServTrans(String ServCmd) {
		String buf; 
		BeginTrans();
		buf = DataTrans(ServCmd);
		EndTrans();
		return buf;
		}

/* perform a non-autonomous server transaction */
	public String DataTrans(String ServCmd) {
		String buf = "";
		String inbuf = "";
		String expbuf = "";

		/* perform the transaction */

		System.out.println(">"+ServCmd);	/* trace */
		ServPuts(ServCmd);
		buf = ServGets();
		System.out.println("<"+buf);		/* trace */

	    try {
		if (buf.startsWith("4")) {
			ofp.writeBytes(TransBuf);
			if (!TransBuf.endsWith("\n")) {
				ofp.writeBytes("\n");
				}
			ofp.writeBytes("000");
			TransBuf = "";
			}

		if (buf.startsWith("1")) {
			TransBuf = "";
			inbuf = "";
			do {
				inbuf = ServGets();
				if (!TransBuf.equals("")) {
					TransBuf = TransBuf + "\n";
					}
				if (!inbuf.equals("000")) {
					TransBuf = TransBuf + inbuf;
					}
				} while (!inbuf.equals("000"));
			}
		    }
	    catch(IOException e) {
		System.out.println(e);
		}

		return(buf);
		}

	public void AddClientThread(Thread ct) {
		ClientThreads.addElement(ct);
		System.out.println("--new thread registered--");
		}

	public void RemoveClientThread(Thread ct) {
		ClientThreads.removeElement(ct);
		System.out.println("--thread removed--");
		}



	/* The following four functions take care of keeping track of the
	 * user name and password being used on the primary server connection
	 * in case we want to use them for a piggyback connection.
	 */
	public void SetUserName(String U) {
		PrimaryServerUser = U;
		}
	
	public void SetPassword(String P) {
		PrimaryServerPassword = P;
		}

	public String GetUserName() {
		return PrimaryServerUser;
		}

	public String GetPassword() {
		return PrimaryServerPassword;
		}



	}


