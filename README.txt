                      WEBCIT for the Citadel/UX System
                               version 2.00
 
   Copyright (C) 1996-1999 by Art Cancro, Nathan Bryant, and Nick Grossman
This program is free software released under the terms of the GNU General
Public License.  Please read COPYING.txt for more licensing information.
 
 
 INTRODUCTION
 ------------
 
 Citadel/UX is a sophisticated BBS package which allows multiple users to
simultaneously access the system using a variety of user interfaces.  This
package (WebCit) is a "middleware" package which presents an HTML/HTTP user
interface to the Citadel system.
 
 What this means in practice is that after you've installed WebCit, users can
access all functions of your BBS using any web browser.  Since this may be the
first Citadel experience for many new users, the screens have been designed to
be attractive and easy to navigate.
 
 
 INSTALLATION
 ------------
 
 If you've installed WebCit v1.xx before, you'll be pleased to know that the
new version doesn't require all that tedious mucking about with Apache
configuration files and directories.  WebCit now contains its own standalone
webserver, which you run on another port (port 2000 by default).
 
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
  
 webserver [-p localport] [-t tracefile] [remotehost [remoteport]]
 
 Explained: 
  
  -> localport: the TCP port on which you wish your WebCit server to run.
     this can be any port number at all; there is no standard.  Naturally,
     you'll want to create a link to this port on your system's regular web
     pages (presumably on an Apache server running on port 80).
 
  -> tracefile: where you want WebCit to log to.  This can be a file, a
     virtual console, or /dev/null to suppress logging altogether.
 
  -> remotehost: the name or IP address of the host on which your Citadel/UX
     server is running.  The default is "localhost".  (NOTE: if you run
     WebCit and the Citadel/UX server on different hosts, the real-time chat
     screen will not work, due to the Java security model.  Only the chat
     window is written as a Java applet; everything else is plain HTML.)
 
  -> remoteport: the port number on which your Citadel/UX server is running.
     The default is port 504, the IANA-designated standard port for Citadel.
 
 
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
 
 
 CONCLUSION
 ----------
 
 That's all you need to know to get started.  If you have any questions or
comments, please visit UNCENSORED! BBS, the home of Citadel/UX, at one of the
following locations:
                       via Internet:   uncnsrd.mt-kisco.ny.us
                       modem dialup:   914-244-3252
