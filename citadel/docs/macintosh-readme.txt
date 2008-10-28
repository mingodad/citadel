Updated install instructions for Citadel and Webcit 7.37 (28/10/2008):
External libraries:
1. libsieve
Some patches need to be applied to the libsieve to make it compile. You can
get these from MacPorts:
http://trac.macports.org/browser/trunk/dports/mail/libsieve/
2. gettext
You will need to install gettext and add "-lintl" to LIBS in webcit's makefile
to make that compile
http://ftp.gnu.org/pub/gnu/gettext/

Older instructions from 10.4 era (2005):
Theres only a brief bit on OS X in the docs but to save some people time
perhaps this can be noted somewhere:

All instructions here assume 10.4 (Tiger). Note gcc package difference
for earlier versions.

The easiest way to get Berkeley DB installed is downloading them via
Fink ( http://fink.sourceforge.net ). Install db42 via apt-get .
Apparently OS X Server installs already have Berkeley DB.

If you haven't got them already, gcc4.0.pkg, Xcode Tools.mpkg,
DeveloperTools.pkg and SDK.pkg from Xcode need to be installed to
compile (plain old boring Darwin users should already have all of them).
Remember to export CC=gcc-4.0 as gcc isn't symlinked by default.

libical and webcit install fine, but you need some configure arguments
for citadel itself:
./configure --disable-autologin --with-db=/sw/
(--disable-autologin must be there otherwise accounts with the same name
as system users won't work! I just spent half an hour bashing my head on
this!)

If you didn't grab Berkeley DB from Fink, substitute your own path for
--with-db.

Ignore what the setup script says about Citadel failing to start, start
it yourself afterwards.

I'm not sure if inittab does anything on OS X / Darwin (AFAIK Apple
doesn't really want us to use it), so you can create your own startup
scripts in /Library/StartupItems/. See
http://www.opendarwin.org/en/articles/system_starter_howto/

Webcit works fine, seems to lock up if you try adding new Internet
domains though, add them via the citadel client itself.

Other than that: Webcit looks better than it was from when I last used
it 6 months ago :-)

- Mathew McBride

