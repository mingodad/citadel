                        WEBCIT for the Citadel System
 
   Copyright (C) 1996-2012 by the authors.  Portions written by:
 
	Art Cancro
	Wilfried Goesgens
	Stefan Garthe
	Dave West
	Thierry Pasquier
	Nathan Bryant
	Nick Grossman
	Andru Luvisi
	Alessandro Fulciniti
	Dave Lindquist
	Matt Pfleger
	Martin Mouritzen

   This program is open source software released under the terms of the GNU
   General Public License, version 3. Please read COPYING.txt for more
   licensing information.
 
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

   Most of our Icons are taken from the "Essen" set by the people on
   http://pc.de/icons/. We would like to thank them for their astonishing
   work!  Their site explicitly states: "Free for commercial use as well."

   One or more icons are from Milosz Wlazlo [http://miloszwl.deviantart.com]
   whose license explicitly allows inclusion in open source projects on the
   condition of this attribution.

   WebCit bundles the CSS3PIE library [http://css3pie.com] which is offered
   under both the Apache license and the GNU General Public License.

   The Citadel logo was designed by Lisa Aurigemma.

 
 INTRODUCTION
 ------------
 
 Citadel is a sophisticated groupware platform which allows multiple
users to simultaneously access the system using a variety of user interfaces.
This package (WebCit) is a web based front end and user interface to the
Citadel system.

 What this means in practice is that after you've installed WebCit, users can
access all functions of your system using any web browser.  Since this may be
the first Citadel experience for many new users, the screens have been designed
to be attractive and easy to navigate.
 
 
 INSTALLATION
 ------------
 
 Unlike some web-based packages, WebCit contains its own standalone HTTP
engine.  As a result, you can get it running quickly without all that tedious
mucking about with Apache configuration files and directories.  WebCit is not
intended to be a general-purpose web server, however -- it *only* provides a
front end to Citadel.  If you do not have another web server running, you may
run WebCit on port 80; however, if you have Apache or some other web server
listening on port 80, you must run WebCit on another port.  If you do not
specify a port number, WebCit will bind to port 2000.
 
 To compile from source, enter the usual commands:
  ./configure --prefix=/usr/local/webcit   [or whatever directory you prefer]
  make
  make install
 
 Package/Ports Maintainers: to make webcit fit smart into LHFS-ified systems
 read on at the end of this file, Advanced configure options.

 Then to initialize it:
  cd /usr/local/webcit
  ./setup
 
 After running setup, you just point your web browser to whatever port you
specified, such as:
 
  http://your.host.name
 
 (or if you specified some other port, such as 2000 in this example...)
 
  http://your.host.name:2000
 
 ...and log in.
 
 The included "setup" program is basically just an installation helper that
asks a series of questions and then adds the appropriate init files to
start WebCit.  For most installations, this will do just fine.  If you have
special circumstances, or if you'd prefer to configure WebCit manually, you
may skip the setup program.  Instead, open /etc/inittab and add an entry
something like this:
 
 wc:2345:respawn:/usr/local/webcit/webcit
 
 
 Several command-line options are also available.  Here's the usage for
the "webcit" program:
  
 webcit [-i ip_addr] [-p http_port] [-s] [-S cipher_suite]
           [-g guest_landing_page]
           [-c] [-f] [remotehost [remoteport]]
 
   *or*
 
 webcit [-i ip_addr] [-p http_port] [-s] [-S cipher_suite]
           [-g guest_landing_page]
           [-c] [-f] uds /your/citadel/directory
 
 Explained: 
  
  -> ip_addr: the IP address on which you wish your WebCit server to run.
     You can leave this out, in which case WebCit will listen on all
     available network interfaces.  Normally this will be the case, but if
     you are running multiple Citadel systems on one host, it can be useful.
     You can also use this option to run Apache and WebCit on different IP
     addresses instead of different ports, if you have them available.
 
  -> http_port: the TCP port on which you wish your WebCit server to run.  If
     you are installing WebCit on a dedicated server, you can use the
     standard port 80.  Otherwise, if port 80 is already occupied by some
     other web service (probably Apache), then you'll need to select a
     different port.  If you do not specify a port number, WebCit will attempt
     to use port 80.
     
  -> The "guest landing page" is a location on your WebCit installation where
     unauthenticated guest users are taken when they first enter the root of
     your site.  If guest mode is not enabled on your Citadel server, they will
     be taken to a login page instead.  If guest mode is enabled but no landing
     page is defined, they will be taken to the Lobby.
 
  -> The "-c" option causes WebCit to output an extra cookie containing the
     identity of the WebCit server.  The cookie will look like this:
       Set-cookie: wcserver=your.host.name
     This is useful if you have a cluster of WebCit servers sitting behind a
     load balancer, and the load balancer has the ability to use cookies to
     keep track of which server to send HTTP requests to.
 
  -> The "-s" option causes WebCit to present an HTTPS (SSL-encrypted) web
     service.  If you want to do both HTTP and HTTPS, you can simply run two
     instances of WebCit on two different ports.

  -> The "-S" option also enables HTTPS, but must be followed by a list of
     cipher suites you wish to enable.  Please see http://openssl.org/docs/apps/ciphers.html
     for a list of cipher strings.

  -> The "-f" option tells WebCit that it is allowed to follow the
     "X-Forwarded-For:" HTTP headers which may be added if your WebCit service
     is sitting behind a front end proxy.  This will allow users in your "Who
     is online?" list to appear as connecting from their actual host address
     instead of the address of the proxy.  In addition, the
     "X-Forwarded-Host:" header from the front end proxy will also be honored,
     which will help to make automatically generated absolute URL's (for
     things like GroupDAV and mailing list subscriptions) correct.
 
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
 

 CUSTOMIZATION
 -------------

 The default WebCit installation will create an empty directory called 
"static.local". In this directory you may place a file called "webcit.css"
into the "styles" directory which, if present, is referenced *after* the
default stylesheet. If you know CSS and wish to customize your WebCit
installation, any styles you declare in static.local/styles/webcit.css
will override the styles found in static/styles/webcit.css -- and your
customizations will not be overwritten when you upgrade WebCit later.

 You may also place other files, such as images, in static.local for
further customization.

 
 CALENDAR SERVICE
 ----------------
 
 WebCit contains support for calendaring and scheduling.  In order to use it
you must have libical v0.26 (or newer) on your system.
 
 WebCit also provides iCalendar format free/busy data for calendar clients.
Unlike with some other servers, there is no need for each user to "publish"
free/busy data -- it is generated on-the-fly from the server-side calendar
of the user being queried.
  
 
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


 INTEGRATING INTO APACHE
 -----------------------

 It is best to run WebCit natively on its own HTTP port.  If, however, you wish
to have WebCit run as part of an Apache web server installation (for example,
you only have one IP address and you need to stay on port 80 or 443 in order to
maintain compatibility with corporate firewall policy), you can do this with
the "mod_proxy" Apache module.
 
 The preferred way to do this is to configure a NameVirtualHost for your WebCit
installation (for example, http://webcit.example.com) and then proxy that
virtual host through to WebCit.  The alternative way, which does work but is not
quite as robust, is to "mount" the WebCit paths as directory aliases to your
main document root.

Here is how to configure the NameVirtualHost method (recommended) :

<VirtualHost mydomain.com:443>
	#here some of your config stuff like logging, serveradmin...
	NameVirtualHost www.mydomain.com
    <location />
         allow from all
    </location>
    ProxyPass / http://127.0.0.1:2000/
    ProxyPassReverse / http://127.0.0.1:2000/
# The following line is optional.  It allows WebCit's static content
# such as images to be served directly by Apache.
    alias /static /var/lib/citadel/www/static
</VirtualHost>

Here is how to configure the "subdirectory" method (not recommended) :

<VirtualHost mydomain.com:443>
	#here some of your config stuff like logging, serveradmin...
	NameVirtualHost www.mydomain.com
    <location /webcit>
      allow from all
    </location>
    <location /listsub>
      allow from all
    </location>
    <location /groupdav>
      allow from all
    </location>
    <location /who_inner_html>
      allow from all
    </location>

    ProxyPass /webcit/ http://127.0.0.1:2000/webcit/
    ProxyPassReverse /webcit/ http://127.0.0.1:2000/webcit/
    ProxyPass /listsub/ http://127.0.0.1:2000/listsub/
    ProxyPassReverse /listsub/ http://127.0.0.1:2000/listsub/
    ProxyPass /groupdav/ http://127.0.0.1:2000/groupdav/
    ProxyPassReverse /groupdav/ http://127.0.0.1:2000/groupdav/
    ProxyPass /who_inner_html http://127.0.0.1:2000/who_inner_html
    ProxyPassReverse /who_inner_html http://127.0.0.1:2000/who_inner_html
</VirtualHost>

  
 ADVANCED CONFIGURATION OPTIONS
 ------------------------------
 
If you are building packages and prefer not to have WebCit reside entirely in
a single directory, there are several compile-time options available.

--with-wwwdir defines where webcit should locate and search its templates and images. 
--with-localedir defines where to put webcits locale files.

Also, there are possibilities to load the TinyMCE editor into a system-wide location.  WebCit 
uses this standard component to compose its messages for messages and postings. Several WebCit installations
that may differ in design but use the same TinyMCE (which is the default that WebCit ships with)
(set --with-editordir for that, it defaults to the dir the templates go)

Install targets have diversified to reflect these changes too:
 (make install-.....)
locale: the webcit .mo files for gettext & locales.
tinymce: the editor. if your system brings one, just ommit this.
wwwdata: our templates.
setupbin: if you want to use webcits setup facility... but isn't needed in case you provide own init & config scripts.
bin: the binaries.


 CONCLUSION
 ----------
 
 That's all you need to know to get started.  If you have any questions or
comments, visit the Citadel Support room on UNCENSORED! BBS, the home of Citadel:
http://uncensored.citadel.org/dotgoto?room=Citadel%20Support
