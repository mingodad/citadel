#!/bin/bash

# Temporary RSS feed suck-o-matic script for Uncensored.
#
# This script is UNSUPPORTED.  It is part of a technology preview for
# functionality which will eventually ship as part of the Citadel system. 

# Paths to the RSS2CTDL binary and to the Citadel directory
PROG=/usr/local/rss2ctdl/rss2ctdl
CTDL=/appl/citadel

# Do one of these for each feed.  You need the URL of the feed, the name
# of the room to dump it into, and a domain name to stamp onto messages
# and message ID's.
#
$PROG http://lxer.com/module/newswire/headlines.rss LXer lxer.com $CTDL
$PROG http://slashdot.org/index.rss Slashdot slashdot.org $CTDL
$PROG http://www.groklaw.net/backend/GrokLaw.rdf Groklaw groklaw.net $CTDL
$PROG http://www.ioerror.us/feed/rss2/ Lizard ioerror.us $CTDL
