#/bin/sh
#
# Please don't add this to the Makefile
#
cc -O2 -Wall -D_REENTRANT -fPIC -c serv_upgrade.c
cc -shared -o /appl/develcit/citadel/modules/serv_upgrade.so serv_upgrade.o
