 Citadel/UX release notes -- version 5.50
  
 ALL FURTHER CHANGES WILL BE IN THE "ChangeLog" FILE.
 
 Please view that file for further information.
 
 

 Citadel/UX release notes -- version 5.01 ("Scooby")
  
 Major overhaul.  The server is now multithreaded; you run one copy of it
when you bring up your system, rather than having inetd start a separate
server process for each session.
  
 Access level 0, which was formerly "marked for deletion," has been changed
to "deleted."  When a user record is marked access level 0, it is then 
considered a vacant space in the userlog rather than a user entry.  When
new users are added to the system, the server first searches for these vacant
slots before appending to the end of the file.
 
 The setup program has been overhauled.  It can now operate in three different
modes: a curses-based mode, a mode based on Savio Lam's "dialog" program, and
of course dumb-terminal mode.
 
 All system messages (hello, newuser, etc.) can now be edited from a client
program.  We now also have support for graphics images, including graphics
tied to various system objects (such as pictures of each user).
 
 Much more support for server graphics is in place.  This is currently used
in WebCit; expect other clients to do more with graphics in the future.
 
 There really is too much to list here.  More documentation will be written
as time permits.
  
 
 Citadel/UX release notes -- version 4.11
  
 Smarter usage of external editors.  EDITOR_EXIT is no longer needed.  Instead,
the program examines temp files before and after they are edited, in order to
determine whether the user saved the file.  Modified files are saved, unchanged
files are aborted.
  
 External editors work for Bio and Room Info files as well.
 
 The TCP/IP based client (citatcp, linked against ipc_c_tcp.o) now supports
connection to a Citadel server from behind a SOCKS v4 firewall without having
to "socksify" the program first.
 
 Added a few more commands to the chat server.
 
 
 Citadel/UX release notes -- version 4.10
  
 Floors now fully supported in both client and server.
 
 Now supports "bio" files for each user - free-form text files in which 
users may record personal info for others to browse.
 
 <.G>oto and <.S>kip now have more advanced logic for partial matches.  A
left-aligned match carries a heavier weight than a mid-string match.  For
example, <.G> TECH would prefer "Tech Area>" over "Biotech/Ethics>".
 
 
 Citadel/UX release notes -- version 4.03
 
 There are now commands available for users with their own copy of the client
to upload and download directly to their local disk, without having to use
a protocol such as Zmodem.  These commands are disabled by default, but can
easily be enabled by changing citadel.rc.
 
 Multiuser <C>hat is now fully integrated into both the client and server.
 
 New <P>aging functionality allows users to send each other near-real-time
"express" messages.

 
 Citadel/UX release notes -- version 4.02
 
 The "rnews" program has been renamed to "rcit".  Its usage has not changed;
it still accepts UseNet-style news by default.  Use the -c option to read in
Citadel (IGnet) format data.
  
 The text client now compiles and works on systems using BSD-style <sgtty.h>
in addition to SysV-style <termio.h>.  This should be a big help for many.
 
 
 Citadel/UX release notes -- version 4.01
 
 Remember to always run setup again when using a new version of the code, to
bring your data files up to date!  No data will be lost.
 
 This release is primarily to clean up loose ends and fix assorted small
bug reports from the users of 4.00.  Also, the code is slowly being reworked
to compile cleanly under ANSI C compilers even when warning messages are
set to the highest level.
  
 -> The client now sends periodic "keep-alive" messages to the server during
message entry, with both internal and external editors.  This should keep
sessions from locking up when a user hits <S>ave after typing for a long time.
 
 -> Stats now has a "-b" option for batch mode (see utils.doc)
    It also has a "-p" option to only print the post-to-call ratios.
 
 
 Citadel/UX release notes -- version 4.00
 
 This is the long-awaited client/server version of Citadel.  Feedback is
encouraged!
 
 NOTE: if you are upgrading to 4.00 from a 3.2x release, you should run setup
to bring your data files up to date.  Setup will prompt you to run the
"conv_32_40" program; go ahead and do that.
 
 If you are upgrading to 4.00 from a 3.1x release, setup will not tell you
what to do!  Here is the correct procedure:
  1. Run the "conv_31_32" program
  2. Run setup
  3. Run "conv_32_40"
 
 If you are currently running a version earlier than 3.10, you must erase your
data files and bring up a new system.
 
 A new series of commands to manipluate files in a room's directory:
   <.A>ide <F>ile <D>elete   -- delete it
   <.A>ide <F>ile <M>ove     -- move it to another room
   <.A>ide <F>ile <S>end     -- send it over the network
  
 Changed the way the main Citadel program sends network mail, both to other
Citadels and through the RFC822 gateway.  Everything now gets fed through
netproc, which is responsible for figuring out the routing and doing the
actual processing.  It also gets rid of quite a bit of redundant code.
  
  
 Citadel/UX release notes -- version 3.23
 
 NOTE: if you are upgrading to 3.23 from another 3.2x release, you MUST run
setup to bring your data files up to date.  Run setup and answer "no" when it
asks you if you want to create the various files.  It will see your old files
and bring them up to date.  (If you are doing a new installation, of course,
you'll be creating all new files anyway.)
 
 The built-in message editor has been completely rewritten, and is now the
preferable editor to use.  It generates messages that will be formatted to
the reader's screen width, as in other implementations of Citadel (and as
Citadel/UX used to do until a few versions ago -- there was a need to put
that functionality back in).
 
 Users are now prompted for their screen width AND height.
 
 We can now talk to C86NET compliant networks.  Get the "cit86net" package
if you wish to do this.
 
 Rooms can now optionally be "read only," which means that only Aides and
utilities such as the networker and aidepost can post to the room.
 
 Kernel-based file locking is now fully supported for the purpose of locking
the message base during writes.  If your kernel supports the flock() system
call, you can tell the compiler to use it by setting a flag in the Makefile.
If your kernel supports file locking using fcntl(), this will automatically be
detected and utilized.  If your kernel supports neither, the program will use
a fully portable (but less reliable) file locking scheme.
 
 An external editor is no longer required for the .AI command.
  
 
 Citadel/UX release notes -- version 3.22
 
 The "setup" program is now a curses-based, full-screen utility that's very
easy to use.  Of course, if you have trouble compiling curses-based programs
on your system, you can compile it to run the old way.
  
   
 Citadel/UX release notes -- version 3.21
   
 I'm now running my system under Linux, since that seems to be what everyone
is using these days.  As a result, you'll find that version 3.21 will compile
very cleanly under Linux.
  
    
 Citadel/UX release notes -- version 3.20
  
 Lots of improvements and new features are here.  It seems that assorted
hacks and variations of Citadel/UX have percolated around the country --
this 3.2 release should supersede them and get everyone running (hopefully)
off the same code.  I've looked around at the various mods people have made
to Citadel/UX and tried to implement the most-often-added and most-requested
features to the stock distribution.  If there's a feature you want/need that
still isn't here, drop me a line and I'll see what I can do about adding it
to the next release.




 For more information, visit the Citadel/UX web site at UNCENSORED! BBS
 http://uncnsrd.mt-kisco.ny.us


