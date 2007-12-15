#!/bin/bash
source include.sh
apt-get update
WD=`pwd`
# get everything built.
for i in `cat $TARGETS`; do 
    cd $WD
    DISTRO=`dirname $i`
    VSERVER_BASE=`basename $i`
    DISTVER=`basename $i |sed "s;CitadelBuild.;;"`
    DISTVER=`cd $APACHE_ROOT/public_html/$DISTRO/; ls -d ?$DISTVER `
    echo "****** upgrading $DISTRO ******"

    UpgradeBuildTarget $i


done
