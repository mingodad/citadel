                 RSS2CTDL -- an RSS to Citadel gateway

Main program (c)2004 by Art Cancro <http://uncensored.citadel.org>
RSS parser (c)2003-2004 by Oliver Feiler <kiza@kcore.de / http://kiza.kcore.de>
                        and Rene Puls <rpuls@gmx.net>
 
RSS2CTDL is an RSS-to-Citadel gateway.  It allows you to pull external RSS
feeds into Citadel rooms.  Feed URL's are polled whenever you run the program.
Each item is converted to a Citadel message and submitted into the network
queue.  The message-ID is derived from a unique hash of the GUID tag of
each item.  If there is no GUID tag (which, unfortunately, is the case for
the vast majority of feeds) then we hash the title/description/link tags
instead.  We then dump it all into the queue and let the loopzapper handle
the dupes.

We are distributing RSS2CTDL as a standalone utility, only as a temporary
measure.  Eventually it will be bundled with the Citadel server, and it will
be invoked by the main server process.  At that time, this standalone
distribution will be discontinued.

RSS2CTDL requires the "libxml2" library, which is probably already installed
on your host system.  If not, get it from http://www.xmlsoft.org

Here's how to make it work:

1. Run "make" to build the program.
   (There is no "configure" and there is no "make install" either.  The
   makefile will produce an "rss2ctdl" binary, which is all you need.)
2. Edit the "do_feeds.sh" script to your liking.  Tell it the feeds you
   want to receive, and the rooms you want to deposit them into.
3. Create those rooms if they do not already exist.
4. Configure your crontab to run do_feeds.sh every half hour.  (You can go
   more or less often if you wish, but once every half hour seems to be the
   frequency generally agreed upon in the community to be optimal.)

Do be aware that rss2ctdl must have write access to $CTDL/network/spoolin
in order to submit its messages into the Citadel spool.  In practice, this
generally means that it should be run by the crontab of the user under which
the Citadel service is running ... or by root if you wish.
