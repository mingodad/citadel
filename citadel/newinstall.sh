#!/bin/sh
# $Id$
#
#   Automatic script to install Citadel on a target system.
#   Copyright (C) 2004 Michael Hampton <error@citadel.org>
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
# wget -q -O - http://my.citadel.org/install | sh
#
# Note that this script installs software on your system and so it requires
# root privileges.  Feel free to inspect the script to make sure I didn't
# do anything stupid...
#
# We have provided you the source code according to the terms of the respective
# software licenses included in the source code packages, even if you choose
# not to keep the source code around.  You can always download it again later.
#
# We install the following versions in this release:
# Package      Version                 Status
# Citadel      6.24                    Latest
# WebCit       5.22                    Latest
# libical      0.24.RC4                Latest
# Berkeley DB  4.1.25 + 2 patches      Stable
# OpenLDAP     2.1.30 stable-20040329  Stable


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
BUILD=/tmp/citadel-build.$$
export SUPPORT CITADEL WEBCIT

# Change the number of jobs to one plus the number of CPUs for best
# performance when compiling software.
MAKEOPTS="-j2"

# End user customization area

# We're now exporting a bunch of environment variables, and here's a list:
# CITADEL_INSTALLER	Set to "web" to indicate this script
# CITADEL		Directory where Citadel is installed
# SUPPORT		Directory where support programs are installed
# LDAP_CONFIG		Location of the slapd.conf file
# DISTRO_MAJOR		Linux distribution name, if applicable
# DISTRO_MINOR		Linux distribution name, if applicable
# DISTRO_VERSION	Linux distribution version (major digit) if applicable
# CC			C compiler being used
# MAKE			Make program being used
# CFLAGS		C compiler flags
# LDFLAGS		Linker flags

# Let Citadel setup recognize the Citadel installer
CITADEL_INSTALLER=web
export CITADEL_INSTALLER

DOWNLOAD_SITE=http://my.citadel.org/download

# Original source code packages.
DB_SOURCE=db-4.1.25.tar.gz
DB_PATCHES=db-4.1.25.patches
ICAL_SOURCE=libical-0.24.RC4.tar.gz
LDAP_SOURCE=openldap-stable-20040329.tgz
CITADEL_SOURCE=citadel-6.24.tar.gz
WEBCIT_SOURCE=webcit-5.22.tar.gz

SETUP="Citadel Easy Install"

LOG=$BUILD/log.txt

##### BEGIN Functions #####

die () {
	echo Easy Install is aborting.
	echo Please report this problem to the Citadel developers.
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

download_sources () {
	echo "* Downloading Berkeley DB..."
	wget -c $DOWNLOAD_SITE/$DB_SOURCE 2>&1 >>$LOG || die
	echo "* Downloading libical..."
	wget -c $DOWNLOAD_SITE/$ICAL_SOURCE 2>&1 >>$LOG || die
	echo "* Downloading OpenLDAP..."
	wget -c $DOWNLOAD_SITE/$LDAP_SOURCE 2>&1 >>$LOG || die
	echo "* Downloading Citadel..."
	wget -c $DOWNLOAD_SITE/$CITADEL_SOURCE 2>&1 >>$LOG || die
	echo "* Downloading WebCit..."
	wget -c $DOWNLOAD_SITE/$WEBCIT_SOURCE 2>&1 >>$LOG || die
}

install_ical () {
	echo "* Installing libical..."
	cd $BUILD 2>&1 >>$LOG || die
	( gzip -dc $ICAL_SOURCE | tar -xvf - ) 2>&1 >>$LOG || die
	cd $BUILD/libical-0.24 2>&1 >>$LOG || die
	./configure --prefix=$SUPPORT 2>&1 >>$LOG || die
	$MAKE $MAKEOPTS 2>&1 >>$LOG || die
	$MAKE install 2>&1 >>$LOG || die
	echo "  Complete."
}

install_db () {
	echo "* Installing Berkeley DB..."
	cd $BUILD 2>&1 >>$LOG || die
	( gzip -dc $DB_SOURCE | tar -xvf - ) 2>&1 >>$LOG || die
	cd $BUILD/db-4.1.25 2>&1 >>$LOG || die
	#patch -p0 < ../$DB_PATCHES 2>&1 >>$LOG || die
	cd $BUILD/db-4.1.25/build_unix 2>&1 >>$LOG || die
	../dist/configure --prefix=$SUPPORT --disable-compat185 --disable-cxx --disable-debug --disable-dump185 --disable-java --disable-rpc --disable-tcl --disable-test --without-rpm 2>&1 >>$LOG || die
	$MAKE $MAKEOPTS 2>&1 >>$LOG || die
	$MAKE install 2>&1 >>$LOG || die
	echo "  Complete."
}

install_ldap () {
	echo "* Installing OpenLDAP..."
	CFLAGS="${CFLAGS} -I${SUPPORT}/include"
	CPPFLAGS="${CFLAGS}"
	LDFLAGS="-L${SUPPORT}/lib -Wl,--rpath -Wl,${SUPPORT}/lib"
	export CFLAGS CPPFLAGS LDFLAGS
	cd $BUILD 2>&1 >>$LOG || die
	( gzip -dc $LDAP_SOURCE | tar -xvf - ) 2>&1 >>$LOG || die
	cd $BUILD/openldap-2.1.29 2>&1 >>$LOG || die
	./configure --prefix=$SUPPORT --enable-bdb 2>&1 >>$LOG || die
	$MAKE $MAKEOPTS 2>&1 >>$LOG || die
	LDAP_CONFIG=$SUPPORT/etc/openldap/slapd.conf
	export LDAP_CONFIG
	$MAKE install 2>&1 >>$LOG || die
	echo "  Complete."
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
	if [ -z "$OK_LDAP" ]
	then
		install_ldap
	fi
}

install_sources () {
	echo "* Installing Citadel..."

	CFLAGS="${CFLAGS} -I${SUPPORT}/include"
	CPPFLAGS="${CFLAGS}"
	LDFLAGS="-L${SUPPORT}/lib -Wl,--rpath -Wl,${SUPPORT}/lib"
	export CFLAGS CPPFLAGS LDFLAGS

	cd $BUILD 2>&1 >>$LOG || die
	( gzip -dc $CITADEL_SOURCE | tar -xvf - ) 2>&1 >>$LOG || die
	cd $BUILD/citadel 2>&1 >>$LOG || die
	if [ -z "$OK_DB" ]
	then
		./configure --prefix=$CITADEL --with-db=$SUPPORT --with-pam --enable-autologin --with-ldap --with-libical --disable-threaded-client 2>&1 >>$LOG || die
	else
		./configure --prefix=$CITADEL --with-db=$OK_DB --with-pam --enable-autologin --with-ldap --with-libical --disable-threaded-client 2>&1 >>$LOG || die
	fi
	$MAKE $MAKEOPTS 2>&1 >>$LOG || die
	if [ -f $CITADEL/citadel.config ]
	then
		$MAKE upgrade 2>&1 >>$LOG || die
		$CITADEL/setup -q
	else
		$MAKE install 2>&1 >>$LOG || die
		useradd -c Citadel -s /bin/false -r -d $CITADEL citadel 2>&1 >>$LOG || die
		$CITADEL/setup
	fi

	echo "* Installing WebCit..."
	cd $BUILD 2>&1 >>$LOG || die
	( gzip -dc $WEBCIT_SOURCE | tar -xvf - ) 2>&1 >>$LOG || die
	cd $BUILD/webcit 2>&1 >>$LOG || die
	./configure --prefix=$WEBCIT --with-libical 2>&1 >>$LOG || die
	$MAKE $MAKEOPTS 2>&1 >>$LOG || die
	$MAKE install 2>&1 >>$LOG || die
	echo "  Complete."
}

##### END Functions #####

##### BEGIN main #####

# 1. Gather information about the target system

[ x$MAKE == x ] && MAKE=`which gmake`
[ x$MAKE == x ] && MAKE=`which make`
clear

os=`uname`

# 1A. Do we use the native packaging system or build our own copy of Citadel?

if [ "$os" = "Linux" ]; then
	determine_distribution
elif [ "$os" = "FreeBSD" ]; then
	# TODO: We detect FreeBSD but the port is still out of date...
	DISTRO_MAJOR=FreeBSD
fi

# 2. Present the installation steps (from 1 above) to the user

echo "$SETUP will perform the following actions:"
echo ""
echo "Configuration:"
echo "* Configure Citadel"
echo "* Configure WebCit"
echo ""
echo "Installation:"

if [ "$prepackaged" ]; then
	show_packages_to_install
else
	echo "* Install supporting libraries"
	echo "* Install Citadel"
	echo "* Install WebCit"
fi

echo ""
echo -n "Perform the above installation steps now? (yes) "

read junk
if [ "`echo $junk | cut -c 1 | tr N n`" = "n" ]; then
	exit 2
fi

rm -rf $BUILD
mkdir -p $BUILD
cd $BUILD

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

	download_sources
	install_prerequisites
	install_sources
fi

# 5. Do post-installation setup
	cd $CITADEL
	./setup || die

	cd $WEBCIT
	./setup || die

	rm -fr $BUILD
##### END main #####
