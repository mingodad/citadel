                      WEBCIT for the Citadel/UX System
                               version 4.10
 
   Copyright (C) 1996-2002 by the authors.  Portions written by:
	Art Cancro
	Nathan Bryant
	Nick Grossman
	Andru Luvisi
 
This program is free software released under the terms of the GNU General
Public License.  Please read COPYING.txt for more licensing information.
 
 
 INTRODUCTION
 ------------
 
 Citadel/UX is a sophisticated BBS and groupware package which allows multiple
users to simultaneously access the system using a variety of user interfaces.
This package (WebCit) is a "middleware" package which presents an HTML/HTTP
user interface to the Citadel system.
 
 What this means in practice is that after you've installed WebCit, users can
access all functions of your system using any web browser.  Since this may be
the first Citadel experience for many new users, the screens have been designed
to be attractive and easy to navigate.
 
 
 INSTALLATION
 ------------
 
 Unline some web-based packages, WebCit contains its own standalone HTTP
engine.  As a result, you can get it running quickly without all that tedious
mucking about with Apache configuration files and directories.  WebCit is not
intended to replace your Apache server, however -- it *only* provides a front
end to Citadel.  If you do not have another web server running, you may run
WebCit on port 80; however, in the more likely situation that you have Apache
or some other web server listening on port 80, you must run WebCit on another
port.  The default is port 2000.
 
 To compile from source, enter the usual commands:
  ./configure
  make
 
 Then to test it, simply run the webserver:
  ./webserver
 
 You'll see a bunch of diagnostic messages on the screen.  At this time you
can try it out.  Point your web browser to WebCit using a URL such as:
 
  http://your.host.name:2000
 
 ...and log in.  When you're satisfied that the program is working the way you
want it to, you should set it up to be automatically started by the system at
boot time.  The recommended way to do this is with an entry in /etc/inittab,
because init can then automatically restart WebCit if it happens to crash for
any reason.  Open /etc/inittab and add an entry something like this:
 
 wc:2345:respawn:/usr/local/webcit/webserver
 
 
 Several command-line options are also available.  Here's the usage for
the "webserver" program:
  
 webserver [-p localport] [-t tracefile] [-c] [remotehost [remoteport]]
 
   *or*
 
 webserver [-p localport] [-t tracefile] [-c] uds /your/citadel/directory
 
 Explained: 
  
  -> localport: the TCP port on which you wish your WebCit server to run.
     this can be any port number at all; there is no standard.  Naturally,
     you'll want to create a link to this port on your system's regular web
     pages (presumably on an Apache server running on port 80).
 
  -> tracefile: where you want WebCit to log to.  This can be a file, a
     virtual console, or /dev/null to suppress logging altogether.
 
  -> The "-c" option causes WebCit to output an extra cookie containing the
     identity of the WebCit server.  The cookie will look like this:
       Set-cookie: wcserver=your.host.name
     This is useful if you have a cluster of WebCit servers sitting behind a
     load balancer, and the load balancer has the ability to use cookies to
     keep track of which server to send HTTP requests to.
 
  -> remotehost: the name or IP address of the host on which your Citadel/UX
     server is running.  The default is "localhost".  (NOTE: if you run
     WebCit and the Citadel/UX server on different hosts, the real-time chat
     screen will not work, due to the Java security model.  Only the chat
     window is written as a Java applet; everything else is plain HTML.)
 
  -> remoteport: the port number on which your Citadel/UX server is running.
     The default is port 504, the IANA-designated standard port for Citadel.
 
  -> "uds" is a keyword which tells WebCit that you wish to connect to a
     Citadel server running on the same computer, rather than using a TCP/IP
     socket.  /your/citadel/directory should be set to the actual name of the
     directory in which you have Citadel installed
     (such as /usr/local/citadel).  If you run Citadel and WebCit on the same
     computer, this way may run a bit faster, but you may experience trouble
     with the real-time chat facility.
 
 
 GRAPHICS
 --------
 
 WebCit contains a small amount of graphics (buttons, etc.) which are kept
in its "static" subdirectory.  All site-specific graphics, however, are
fetched from the Citadel server.
 
 The "images" directory on a Citadel/UX system contains these graphics.  The
ones which you may be interested in are:
 
 -> background.gif: a background texture displayed under all web pages
 -> hello.gif: your system's logo.  It is displayed along with the logon
    banner, and on the top left corner of each page.
 
 
 CALENDAR SERVICE
 ----------------
 
 WebCit contains support for calendaring and scheduling.  In order to use it
you must have libical v0.24 (or newer) on your system.  You must also be
running a Citadel server with calendaring support.  The calendar service will
be automatically configured and installed if your host system supports it.
 
 
 CONCLUSION
 ----------
 
 That's all you need to know to get started.  If you have any questions or
comments, please visit UNCENSORED! BBS, the home of Citadel/UX, at
uncensored.citadel.org.
