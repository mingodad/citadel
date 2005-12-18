#
#  citadel-rss2ctdl cron.
#
# this should run every half hour.  (You can go
# more or less often if you wish, but once every 
# half hour seems to be the frequency generally 
# agreed upon in the community to be optimal.)
#
# m h      dom mon dow user      command
0 0-23/2    *   *   *  citadel   /usr/sbin/feeds.cron

