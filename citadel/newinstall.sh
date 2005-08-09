#!/bin/sh
# $Id$
#
#   Automatic script to install Citadel on a target system.
#   Copyright (C) 2004 Michael Hampton <error@citadel.org>
#   Copyright (C) 2004 Art Cancro <ajc@uncensored.citadel.org>
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
# Reading this script?  Here's some helpful hints:
#
# If you're seeing this in your browser, it's probably not what you want.
# You can either save it to disk and run it, or do it the easy way:
#
# wget -q -O - http://easyinstall.citadel.org/install | sh
#
# Note that this script installs software on your system and so it requires
# root privileges.  Feel free to inspect the script to make sure we didn't
# do anything stupid...
#
# We have provided you the source code according to the terms of the respective
# software licenses included in the source code packages, even if you choose
# not to keep the source code around.  You can always download it again later.
#
# We install the following versions in this release:
# Package      Version                 Status
# Citadel      6.27                    Latest
# WebCit       5.27                    Latest
# libical      0.24.RC4                Latest
# Berkeley DB  4.3.21                  Stable


###############################################################################
#
# This is the general stuff we're going to do, in order:
#
# 1. Gather information about the target system
# 2. Present the installation steps (from 1 above) to the user
# 3. Present any pre-install customizations to the user
# 4. Do the installation
#    A. Download any source code files or binary packages required
#    B. For native packaging, call the native packaging system to install
#    C. If we build our own, compile and install prerequisites then Citadel
# 5. Do post-installation setup
#
# Then call it a day.
#
###############################################################################


# Begin user customization area
#
# These two directories specify where Citadel and its private support
# libraries will be installed.  This keeps them safely tucked away from
# the rest of your system.  The defaults should be fine for most people.
# NB: When binary packages are installed, these settings are ignored!
SUPPORT=/usr/local/ctdlsupport
CITADEL=/usr/local/citadel
WEBCIT=/usr/local/webcit
WORKDIR=/tmp
BUILD=$WORKDIR/citadel-build.$$
LOG=$WORKDIR/citadel-install-log.txt
export SUPPORT CITADEL WEBCIT

MAKEOPTS=""

# End user customization area

# We're now exporting a bunch of environment variables, and here's a list:
# CITADEL_INSTALLER	Set to "web" to indicate this script
# CITADEL		Directory where Citadel is installed
# WEBCIT		Directory where WebCit is installed
# SUPPORT		Directory where support programs are installed
# SLAPD_BINARY		Location of the slapd binary
# DISTRO_MAJOR		Linux distribution name, if applicable
# DISTRO_MINOR		Linux distribution name, if applicable
# DISTRO_VERSION	Linux distribution version (major digit) if applicable
# CC			C compiler being used
# MAKE			Make program being used
# CFLAGS		C compiler flags
# LDFLAGS		Linker flags
# IS_UPGRADE		Set to "yes" if upgrading an existing Citadel
# CTDL_DIALOG		Where (if at all) the "dialog" program may be found

# Let Citadel setup recognize the Citadel installer
CITADEL_INSTALLER=web
export CITADEL_INSTALLER

DOWNLOAD_SITE=http://easyinstall.citadel.org

# Original source code packages.
DB_SOURCE=db-4.3.21.NC.tar.gz
# DB_PATCHES=db-x.x.x.patches
ICAL_SOURCE=libical-0.24.RC4.tar.gz
CITADEL_SOURCE=citadel-easyinstall.tar.gz
WEBCIT_SOURCE=webcit-easyinstall.tar.gz

SETUP="Citadel Easy Install"


##### BEGIN Functions #####

die () {
	echo Easy Install is aborting.
	echo Please report this problem to the Citadel developers.
	echo Log file: $LOG
	rm -fr $BUILD
	exit 1
}


determine_distribution () {
	# First look for Red Hat in general
	if [ -x /bin/rpm ]; then
		RELEASE_FILE=/dev/null
		if /bin/rpm -q redhat-release >/dev/null 2>&1; then
			DISTRO_MAJOR=RedHat
			RELEASE_FILE=/etc/redhat-release
		fi
		if /bin/rpm -q whitebox-release >/dev/null 2>&1; then
			DISTRO_MAJOR=WhiteBox
			RELEASE_FILE=/etc/whitebox-release
		fi
		if /bin/rpm -q fedora-release >/dev/null 2>&1; then
			DISTRO_MAJOR=RedHat
			DISTRO_MINOR=Fedora
			RELEASE_FILE=/etc/fedora-release
		fi
		# Then look for specific version
		( cat $RELEASE_FILE | grep Enterprise ) >/dev/null 2>&1 && \
			DISTRO_MINOR=Enterprise
		DISTRO_VERSION=`tr -cd "[^0-9.]" < $RELEASE_FILE | cut -c 1`
	fi

	# Check for Gentoo
	if [ -f /etc/gentoo-release ]; then
		DISTRO_MAJOR=Gentoo
	fi

	# Check for Debian
	# TODO: check for Debian
}

install_ical () {
	cd $BUILD 2>&1 >>$LOG || die
	$WGET $DOWNLOAD_SITE/libical-easyinstall.sum
	SUM=`cat libical-easyinstall.sum`
	SUMFILE=$SUPPORT/etc/libical-easyinstall.sum
	if [ -r $SUMFILE ] ; then
		OLDSUM=`cat $SUMFILE`
		if [ $SUM = $OLDSUM ] ; then
			echo "* libical does not need updating."
			return
		fi
	fi
	echo "* Downloading libical..."
	$WGET $DOWNLOAD_SITE/$ICAL_SOURCE 2>&1 >>$LOG || die
	echo "* Installing libical..."
	( gzip -dc $ICAL_SOURCE | tar -xvf - ) 2>&1 >>$LOG || die
	cd $BUILD/libical-0.24 2>&1 >>$LOG || die
	./configure --prefix=$SUPPORT 2>&1 >>$LOG || die
	$MAKE $MAKEOPTS 2>&1 >>$LOG || die
	$MAKE install 2>&1 >>$LOG || die
	echo "  Complete."
	echo $SUM >$SUMFILE
	rm -f $CITADEL/citadel-easyinstall.sum 2>/dev/null
	rm -f $WEBCIT/webcit-easyinstall.sum 2>/dev/null
}

install_db () {
	cd $BUILD 2>&1 >>$LOG || die
	$WGET $DOWNLOAD_SITE/db-easyinstall.sum
	SUM=`cat db-easyinstall.sum`
	SUMFILE=$SUPPORT/etc/db-easyinstall.sum
	if [ -r $SUMFILE ] ; then
		OLDSUM=`cat $SUMFILE`
		if [ $SUM = $OLDSUM ] ; then
			echo "* Berkeley DB does not need updating."
			return
		fi
	fi
	echo "* Downloading Berkeley DB..."
	$WGET $DOWNLOAD_SITE/$DB_SOURCE 2>&1 >>$LOG || die
	echo "* Installing Berkeley DB..."
	( gzip -dc $DB_SOURCE | tar -xvf - ) 2>&1 >>$LOG || die
	cd $BUILD/db-4.3.21.NC 2>&1 >>$LOG || die
	#patch -p0 < ../$DB_PATCHES 2>&1 >>$LOG || die
	cd $BUILD/db-4.3.21.NC/build_unix 2>&1 >>$LOG || die
	../dist/configure --prefix=$SUPPORT --disable-compat185 --disable-cxx --disable-debug --disable-dump185 --disable-java --disable-rpc --disable-tcl --disable-test --without-rpm 2>&1 >>$LOG || die
	$MAKE $MAKEOPTS 2>&1 >>$LOG || die
	$MAKE install 2>&1 >>$LOG || die
	echo "  Complete."
	echo $SUM >$SUMFILE
	rm -f $CITADEL/citadel-easyinstall.sum 2>/dev/null
}

install_prerequisites () {

	# Create the support directories if they don't already exist

	mkdir $SUPPORT		2>/dev/null
	mkdir $SUPPORT/bin	2>/dev/null
	mkdir $SUPPORT/sbin	2>/dev/null
	mkdir $SUPPORT/lib	2>/dev/null
	mkdir $SUPPORT/libexec	2>/dev/null
	mkdir $SUPPORT/include	2>/dev/null
	mkdir $SUPPORT/etc	2>/dev/null

	# Now have phun!

	if [ -z "$OK_ICAL" ]
	then
		install_ical
	fi
	if [ -z "$OK_DB" ]
	then
		install_db
	fi
}

install_sources () {
	cd $BUILD 2>&1 >>$LOG || die
	if [ -f $CITADEL/citadel.config ]
	then
		IS_UPGRADE=yes
		echo "* Upgrading your existing Citadel installation."
	else
		IS_UPGRADE=no
	fi

	CFLAGS="-I${SUPPORT}/include"
	CPPFLAGS="${CFLAGS}"
	LDFLAGS="-L${SUPPORT}/lib -Wl,--rpath -Wl,${SUPPORT}/lib"
	export CFLAGS CPPFLAGS LDFLAGS

	DO_INSTALL_CITADEL=yes
	$WGET $DOWNLOAD_SITE/citadel-easyinstall.sum
	SUM=`cat citadel-easyinstall.sum`
	SUMFILE=$CITADEL/citadel-easyinstall.sum
	if [ -r $SUMFILE ] ; then
		OLDSUM=`cat $SUMFILE`
		if [ $SUM = $OLDSUM ] ; then
			echo "* Citadel does not need updating."
			DO_INSTALL_CITADEL=no
		fi
	fi

	if [ $DO_INSTALL_CITADEL = yes ] ; then
		echo "* Downloading Citadel..."
		$WGET $DOWNLOAD_SITE/$CITADEL_SOURCE 2>&1 >>$LOG || die
		echo "* Installing Citadel..."
		cd $BUILD 2>&1 >>$LOG || die
		( gzip -dc $CITADEL_SOURCE | tar -xvf - ) 2>&1 >>$LOG || die
		cd $BUILD/citadel 2>&1 >>$LOG || die
		if [ -z "$OK_DB" ]
		then
			./configure --prefix=$CITADEL --with-db=$SUPPORT --with-pam --enable-autologin --with-libical --disable-threaded-client 2>&1 >>$LOG || die
		else
			./configure --prefix=$CITADEL --with-db=$OK_DB --with-pam --enable-autologin --with-libical --disable-threaded-client 2>&1 >>$LOG || die
		fi
		$MAKE $MAKEOPTS 2>&1 >>$LOG || die
		if [ $IS_UPGRADE = yes ]
		then
			echo "* Performing Citadel upgrade..."
			$MAKE upgrade 2>&1 >>$LOG || die
		else
			echo "* Performing Citadel install..."
			$MAKE install 2>&1 >>$LOG || die
			useradd -c "Citadel service account" -d $CITADEL -s $CITADEL/citadel citadel 2>&1 >>$LOG
		fi
		echo $SUM >$SUMFILE
	fi

	cd $BUILD 2>&1 >>$LOG || die
	DO_INSTALL_WEBCIT=yes
	$WGET $DOWNLOAD_SITE/webcit-easyinstall.sum
	SUM=`cat webcit-easyinstall.sum`
	SUMFILE=$WEBCIT/webcit-easyinstall.sum
	if [ -r $SUMFILE ] ; then
		OLDSUM=`cat $SUMFILE`
		if [ $SUM = $OLDSUM ] ; then
			echo "* WebCit does not need updating."
			DO_INSTALL_WEBCIT=no
		fi
	fi

	if [ $DO_INSTALL_WEBCIT = yes ] ; then
		echo "* Downloading WebCit..."
		$WGET $DOWNLOAD_SITE/$WEBCIT_SOURCE 2>&1 >>$LOG || die
		echo "* Installing WebCit..."
		cd $BUILD 2>&1 >>$LOG || die
		( gzip -dc $WEBCIT_SOURCE | tar -xvf - ) 2>&1 >>$LOG || die
		cd $BUILD/webcit 2>&1 >>$LOG || die
		./configure --prefix=$WEBCIT --with-libical 2>&1 >>$LOG || die
		$MAKE $MAKEOPTS 2>&1 >>$LOG || die
		$MAKE install 2>&1 >>$LOG || die
		echo "  Complete."
		echo $SUM >$SUMFILE
	fi
}


do_config () {
	echo "* Configuring your system ..."

	if [ x$IS_UPGRADE == xyes ] ; then
		echo Upgrading your existing Citadel installation.
		#$CITADEL/setup -q || die
		$CITADEL/setup </dev/tty || die
	else
		echo This is a new Citadel installation.
		$CITADEL/setup </dev/tty || die
	fi

	$WEBCIT/setup </dev/tty || die
}



##### END Functions #####

##### BEGIN main #####

# 1. Gather information about the target system

# Non-GNU make does not work.
# This probably ought to be fixed, but for now we will simply require GNU make.

MAKE=xx
if gmake -v 2>&1 | grep -i GNU ; then
	MAKE=`which gmake`
else
	if make -v 2>&1 | grep -i GNU ; then
		MAKE=`which make`
	fi
fi

if [ $MAKE == xx ] ; then
	echo
	echo 'Easy Install requires GNU Make (gmake), which was not found.'
	echo 'Please install gmake and try again.'
	echo
	exit 1
fi

export MAKE

clear

os=`uname`

#[ -n "$WGET" ] && [ -x `which wget` ] && WGET=`which wget`
#[ -z "$WGET" ] && [ -x `which curl` ] && WGET=`which curl`\ -O

[ -n "$WGET" ] && [ -x `which curl` ] && WGET=`which curl`\ -O
[ -z "$WGET" ] && [ -x `which wget` ] && WGET=`which wget`

echo MAKE is $MAKE
echo WGET is $WGET 
export MAKE WGET

# 1A. Do we use the native packaging system or build our own copy of Citadel?

if [ "$os" = "Linux" ]; then
	determine_distribution
elif [ "$os" = "FreeBSD" ]; then
	# TODO: We detect FreeBSD but the port is still out of date...
	DISTRO_MAJOR=FreeBSD
elif [ "$os" = "Darwin" ]; then
	# TODO: Deal with Apple weirdness
	DISTRO_MAJOR=Darwin
fi


rm -rf $BUILD
mkdir -p $BUILD
cd $BUILD


# 2. Present the installation steps (from 1 above) to the user
clear
if dialog --clear </dev/tty ; then
	CTDL_DIALOG=`which dialog`
	export CTDL_DIALOG
fi
clear

echo "$SETUP will perform the following actions:"
echo ""
echo "Installation:"
echo "* Download/install supporting libraries (if needed)"
echo "* Download/install Citadel (if needed)"
echo "* Download/install WebCit (if needed)"
echo ""
echo "Configuration:"
echo "* Configure Citadel"
echo "* Configure WebCit"
echo ""
echo -n "Perform the above installation steps now? "
read yesno </dev/tty

if [ "`echo $yesno | cut -c 1 | tr N n`" = "n" ]; then
	exit 2
fi

echo ""
echo "Command output will not be sent to the terminal."
echo "To view progress, see the $LOG file."
echo ""

# 3. Present any pre-install customizations to the user

# TODO: enter in the configuration dialogs

# Configure Citadel

# Configure WebCit

# 4. Do the installation

# 4A. Download any source code files or binary packages required

if [ "$prepackaged" ]; then
	download_packages

# 4B. For native packaging, call the native packaging system to install

	install_packages
else

# 4C. If we build our own, compile and install prerequisites then Citadel

	install_prerequisites
	install_sources
fi

# 5. Do post-installation setup
	rm -fr $BUILD
	do_config
##### END main #####
