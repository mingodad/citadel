#!/bin/bash
source include.sh

# First get the chroot up and running with: 
#apt-get install bzip2 perl-doc lib32stdc++6 manpages-dev autoconf automake1.9 libtool flex   libc6-dev-i386 lib32gcc1  groff debhelper po-debconf bison autotools-dev libdb4.3-dev  libldap2-dev libncurses5-dev libpam0g-dev  libssl-dev cdbs g++ locales dpatch fakeroot patchutils
WD=`pwd`

for i in `cat $TARGETS`; do 
    cd $WD
    DISTRO=`dirname $i`
    VSERVER_BASE=`basename $i`
    DISTVER=`basename $i |sed "s;CitadelBuild.;;"`
    DISTVER=`cd $APACHE_ROOT/public_html/$DISTRO/; ls -d ?$DISTVER `
     echo "*** building $DISTRO $DISTVER ical *****"
   
    CleanBuild $VSERVER_BASE $CONTRIB_DIR

    cd $WD
    if grep -q $VSERVER_BASE libical_targets; then
	GetSource $VSERVER_BASE $CONTRIB_DIR libical
	BuildSource $VSERVER_BASE $CONTRIB_DIR libical
	InstallContrib  $VSERVER_BASE $CONTRIB_DIR libical
    fi

    echo "*** building $DISTRO $DISTVER sieve *****"
    cd $WD
    if grep -q $VSERVER_BASE libsieve_targets; then
 	GetSource $VSERVER_BASE $CONTRIB_DIR libsieve
 	BuildSource $VSERVER_BASE $CONTRIB_DIR libsieve
 	InstallContrib  $VSERVER_BASE $CONTRIB_DIR libsieve
    fi
    echo "**** done. ****"
    echo "*** building $DISTRO $DISTVER tinymce *****"
    cd $WD
    if grep -q $VSERVER_BASE tinymce_targets; then
 	GetSource $VSERVER_BASE $CONTRIB_DIR tinymce
 	BuildSource $VSERVER_BASE $CONTRIB_DIR tinymce
 	InstallContrib  $VSERVER_BASE $CONTRIB_DIR tinymce
    fi
    echo "**** done. ****"
done

#for i in `cat $TARGETS`; do 
#    echo "*** installing $DISTRO *****"
#    
#done


chroot ${VSERVER_ROOT}/apache/ /bin/bash -c "cd ${CIT_APACHE_DIR}/; ./refresh.sh"
