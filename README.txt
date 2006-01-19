                        WEBCIT for the Citadel System
                               version 6.71
 
   Copyright (C) 1996-2006 by the authors.  Portions written by:
	Art Cancro
	Nathan Bryant
	Wilifried Goesgens
	Nick Grossman
	Andru Luvisi
	Dave Lindquist
	Martin Mouritzen

   This program is open source software released under the terms of the GNU
   General Public License.  Please read COPYING.txt for more licensing
   information.
 
   WebCit bundles the Prototype JavaScript Framework, writen by Sam
   Stephenson [http://prototype.conio.net].  These components are licensed to
   you under the terms of an MIT-style license.

   WebCit bundles the script.aculo.us JavaScript library, written by
   Thomas Fuchs [http://script.aculo.us, http://mir.aculo.us].  These
   components are licensed to you under the terms of an MIT-style license.

   WebCit bundles the TinyMCE text editor, written by Moxiecode Systems AB
   (http://tinymce.moxiecode.com/tinymce/docs/credits.html).  This component
   is licensed to you under the terms of the GNU Lesser General Public
   License.

   The Citadel logo was designed by Lisa Aurigemma.

 
 INTRODUCTION
 ------------
 
 Citadel is a sophisticated groupware and BBS package which allows multiple
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
  ./configure --prefix=/usr/local/webcit   [or whatever directory you prefer]
  make
  make install
 
 Then to initialize it:
  cd /usr/local/webcit
  ./setup
 
 After running setup, you just point your web browser to whatever port you
specified, such as:
 
  http://your.host.name:2000
 
 ...and log in.
 
 The included "setup" program is basically just an installation helper that
asks a series of questions and then adds the appropriate line to inittab to
start WebCit.  For most installations, this will do just fine.  If you have
special circumstances, or if you'd prefer to configure WebCit manually, you
may skip the setup program.  Instead, open /etc/inittab and add an entry
something like this:
 
 wc:2345:respawn:/usr/local/webcit/webserver
 
 
 Several command-line options are also available.  Here's the usage for
the "webserver" program:
  
 webserver [-i ip_addr] [-p http_port] [-s] [-t tracefile]
           [-c] [-f] [remotehost [remoteport]]
 
   *or*
 
 webserver [-i ip_addr] [-p http_port] [-s] [-t tracefile]
           [-c] [-f] uds /your/citadel/directory
 
 Explained: 
  
  -> ip_addr: the IP address on which you wish your WebCit server to run.
     You can leave this out, in which case WebCit will listen on all
     available network interfaces.  Normally this will be the case, but if
     you are running multiple Citadel systems on one host, it can be useful.
 
  -> http_port: the TCP port on which you wish your WebCit server to run.  If
     you are installing WebCit on a dedicated server, you can use the
     standard port 80.  Otherwise, if port 80 is already occupied by some
     other web service (probably Apache), then you'll need to select a
     different port.  If you do not specify a port number, WebCit will attempt
     to use port 2000.
     
  -> tracefile: where you want WebCit to log to.  This can be a file, a
     virtual console, or /dev/null to suppress logging altogether.
 
  -> The "-c" option causes WebCit to output an extra cookie containing the
     identity of the WebCit server.  The cookie will look like this:
       Set-cookie: wcserver=your.host.name
     This is useful if you have a cluster of WebCit servers sitting behind a
     load balancer, and the load balancer has the ability to use cookies to
     keep track of which server to send HTTP requests to.
 
  -> The "-s" option causes WebCit to present an HTTPS (SSL-encrypted) web
     service.  If you want to do both HTTP and HTTPS, you can simply run two
     instances of WebCit on two different ports.

  -> The "-f" option tells WebCit that it is allowed to follow the
     "X-Forwarded-For:" HTTP headers which may be added if your WebCit service
     is sitting behind a front end proxy.  This will allow users in your "Who
     is online?" list to appear as connecting from their actual host address
     instead of the address of the proxy.
 
  -> remotehost: the name or IP address of the host on which your Citadel
     server is running.  The default is "localhost".
 
  -> remoteport: the port number on which your Citadel server is running.
     The default is port 504, the IANA-designated standard port for Citadel.
 
  -> "uds" is a keyword which tells WebCit that you wish to connect to a
     Citadel server running on the same computer, rather than using a TCP/IP
     socket.  /your/citadel/directory should be set to the actual name of the
     directory in which you have Citadel installed
     (such as /usr/local/citadel).  If you run Citadel and WebCit on the same
     computer, this is recommended, as it will run much faster.
 
 
 GRAPHICS
 --------
 
 WebCit contains graphics, templates, JavaScript code, etc. which are kept
in its "static" subdirectory.  All site-specific graphics, however, are
fetched from the Citadel server.
 
 The "images" directory on a Citadel system contains these graphics.  The
ones which you may be interested in are:
 
 -> background.gif: a background texture displayed under all web pages
 -> hello.gif: your system's logo.  It is displayed along with the logon
    banner, and on the top left corner of each page.
 
 If you would like to deploy a "favicon.ico" graphic, please put it in
the static/ directory.  WebCit will properly serve it from there.
 
 
 CALENDAR SERVICE
 ----------------
 
 WebCit contains support for calendaring and scheduling.  In order to use it
you must have libical v0.24 (or newer) on your system.  You must also be
running a Citadel server with calendaring support.  The calendar service will
be automatically configured and installed if your host system supports it.
 
 WebCit also provides Kolab-compatible free/busy data for calendar clients.
Unlike the Kolab server, however, there is no need for each user to "publish"
free/busy data -- it is generated on-the-fly from the server-side calendar
of the user being queried.  Note: in order to support Kolab clients, you must
have WebCit running in HTTPS mode on port 443, because that is what Kolab
clients will be expecting.
  
 
 HTTPS (encryption) SUPPORT
 --------------------------
 
 WebCit now supports HTTPS for encrypted connections.  When a secure server
port is specified via the "-s" flag, an HTTPS service is enabled.
 
 The service will look in the "keys" directory for the following files:
 
 citadel.key   (your server's private key)
 citadel.csr   (a certificate signing request)
 citadel.cer   (your server's public certificate)
 
 If any of these files are not found, WebCit will first attempt to link to the
SSL files in the Citadel service's directory (if Citadel is running on the
same host as WebCit), and if that does not succeed, it will automatically
generate a key and certificate.
 
 It is up to you to decide whether to use an automatically generated,
self-signed certificate, or purchase a certificate signed by a well known
authority.
  
 
 CONCLUSION
 ----------
 
 That's all you need to know to get started.  If you have any questions or
comments, please visit UNCENSORED! BBS, the home of Citadel, at
uncensored.citadel.org.
