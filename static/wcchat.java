/*
 * Java client for Citadel/UX
 * Copyright (C) 1997 by Art Cancro - All Rights Reserved
 *
 * This module is designed to be able to run either as an application or
 * as an applet.
 */

import java.applet.*;

public class wcchat extends Applet {

String ServerHost = "uncnsrd.mt-kisco.ny.us";
int ServerPort = 504;
	
	public void init() {

		/* Unless overridden, the Citadel server is expected to be
		 * the same as the applet host.  In most cases this is all
		 * that's allowed anyway.
		 */
		if (getDocumentBase() != null) {
			ServerHost = getDocumentBase().getHost();
			}

		/* The 'host' parameter tells the client to look somewhere other
		 * than the applet host for the Citadel server.
		 */
		if (getParameter("host") != null) {
			ServerHost = getParameter("host");
			}

		/* The 'port' parameter tells the client to look on a
		 * nonstandard port for the Citadel server.
		 */
		if (getParameter("port") != null) {
			ServerPort = Integer.parseInt(getParameter("port"));
			}

		}

	public void start() {
		wcCitServer serv = new wcCitServer();
		String buf = null;

		serv.AttachToServer(ServerHost, ServerPort);
		buf = serv.ServTrans("USER " + getParameter("username"));
		if (buf.charAt(0) == '3') {
			buf = serv.ServTrans("PASS "+getParameter("password"));
			if (buf.charAt(0) == '2') {
				serv.SetUserName(wcCitUtil.Extract(buf.substring(4), 0));
				new MultiUserChat102(serv, this);
				}
			}
		else {
			System.out.println("ooops...");
			}
		}

}
