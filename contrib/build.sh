#!/bin/bash

apt-get update

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
    echo "****** building $DISTRO $DISTVER ******"
    CleanBuild $VSERVER_BASE $CHECKOUT_DIR 

    echo "****** building $DISTRO $DISTVER libcitadel ******"
    cd $WD
    GetSource $VSERVER_BASE $CHECKOUT_DIR libcitadel
    BuildSource $VSERVER_BASE $CHECKOUT_DIR libcitadel
    InstallContrib  $VSERVER_BASE $CHECKOUT_DIR libcitadel

    echo "****** building $DISTRO $DISTVER Webcit ******"
    cd $WD
    GetSource $VSERVER_BASE $CHECKOUT_DIR webcit
    BuildSource $VSERVER_BASE $CHECKOUT_DIR webcit

    echo "****** building $DISTRO $DISTVER Citadel ******"
    cd $WD
    GetSource $VSERVER_BASE $CHECKOUT_DIR citadel
    BuildSource $VSERVER_BASE $CHECKOUT_DIR citadel

done

# put it public.
for i in `cat $TARGETS`; do 
    echo "****** installing $DISTRO $DISTVER ******"

    cd $WD
    DISTRO=`dirname $i`
    VSERVER_BASE=`basename $i`
    DISTVER=`basename $i |sed "s;CitadelBuild.;;"`
    DISTVER=`cd $APACHE_ROOT/public_html/$DISTRO/; ls -d ?$DISTVER `

    CleanApache "$DISTRO/$DISTVER"

    cd $WD
    UpperResults $VSERVER_BASE "$DISTRO/$DISTVER"
done



chroot ${VSERVER_ROOT}/apache/ /bin/bash -c "cd ${CIT_APACHE_DIR}/; ./refresh.sh"
