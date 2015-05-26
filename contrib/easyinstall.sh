#!/bin/sh
#
# Automatic script to install Citadel on a target system.
# Copyright (C) 2004 Michael Hampton <error@citadel.org>
# Copyright (C) 2004-2015 Art Cancro <ajc@citadel.org>
#
# This program is open source software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License version 3.
#
# Our favorite operating system is called Linux.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# Reading this script?  Here's some helpful hints:
#
# If you're seeing this in your browser, it's probably not what you want.
# You can either save it to disk and run it, or do it the easy way:
#
# curl http://easyinstall.citadel.org/install | sh
#
# Note that this script installs software on your system and so it requires
# root privileges.  Feel free to inspect the script to make sure we didn't
# do anything stupid...
#
# We have provided you the source code according to the terms of the respective
# software licenses included in the source code packages, even if you choose
# not to keep the source code around.  You can always download it again later.
#
# We install the "latest" or "stable" versions of these packages:
#
# Citadel server, WebCit, libcitadel, libical, Berkeley DB, libSieve, Expat
#
# Do *not* attempt to do anything with the UNATTENDED_BUILD mode.  This is
# for our use only and is not only unsupported, but we will deliberately break
# it from time to time in order to prevent you from trying to use it.

###############################################################################
#
# This is the general stuff we're going to do, in order:
#
# 1. Gather information about the target system
# 2. Present the installation steps (from 1 above) to the user
# 3. Present any pre-install customizations to the user
# 4. Do the installation
#    A. Download any source code files packages required
#    B. If we build our own, compile and install prerequisites then Citadel
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

SUPPORT=/usr/local/ctdlsupport
CITADEL=/usr/local/citadel
WEBCIT=/usr/local/webcit
WORKDIR=/tmp
BUILD=$WORKDIR/citadel-build.$$
LOG=$WORKDIR/citadel-install-log.txt
export SUPPORT CITADEL WEBCIT
unset LANG

MAKEOPTS=""

# End user customization area

# We're now exporting a bunch of environment variables, and here's a list:
# CITADEL_INSTALLER	Set to "web" to indicate this script
# CITADEL		Directory where Citadel is installed
# WEBCIT		Directory where WebCit is installed
# SUPPORT		Directory where support programs are installed
# DISTRO_MAJOR		Linux distribution name, if applicable
# DISTRO_MINOR		Linux distribution name, if applicable
# DISTRO_VERSION	Linux distribution version (major digit) if applicable
# CC			C compiler being used
# MAKE			Make program being used
# CFLAGS		C compiler flags
# LDFLAGS		Linker flags
# IS_UPGRADE		Set to "yes" if upgrading an existing Citadel
# CTDL_DIALOG		Where (if at all) the "whiptail" or "dialog" program may be found

# Let Citadel setup recognize the Citadel installer
CITADEL_INSTALLER=web
export CITADEL_INSTALLER

SETUP="Citadel Easy Install"
DOWNLOAD_SITE=http://easyinstall.citadel.org

# Original source code packages.
DB_SOURCE=db-5.1.29.NC.tar.gz
ICAL_SOURCE=libical-easyinstall.tar.gz
LIBSIEVE_SOURCE=libsieve-2.2.7-ctdl2.tar.gz
EXPAT_SOURCE=expat-2.0.1.tar.gz
LIBCURL_SOURCE=curl-7.26.0.tar.gz
LIBEV_SOURCE=libev-4.11.tar.gz
CARES_SOURCE=c-ares-1.7.5.tar.gz
LIBDISCOUNT_SOURCE=discount-2.1.8.tar.gz
LIBCITADEL_SOURCE=libcitadel-easyinstall.tar.gz
CITADEL_SOURCE=citadel-easyinstall.tar.gz
WEBCIT_SOURCE=webcit-easyinstall.tar.gz
TEXTCLIENT_SOURCE=textclient-easyinstall.tar.gz
INCADD=
LDADD=

case `uname -s` in
	*BSD)
		LDADD="-L/usr/local/lib"
		INCADD="-I/usr/local/include"
	;;
esac

##### BEGIN Functions #####

GetVersionFromFile()
{
	VERSION=`cat $1 | tr "\n" ' ' | sed s/.*VERSION.*=\ // `
}


die () {
	echo
	echo $SETUP is aborting.
	echo
	echo A log file has been written to $LOG
	echo Reading this file may tell you what went wrong.  If you
	echo need to ask for help on the support forum, please post the
	echo last screenful of text from this log.
	echo
	echo Operating system: ${OSSTR}
	echo Operating system: ${OSSTR} >>$LOG
	cd $WORKDIR
	rm -fr $BUILD
	exit 1
}



test_build_dir() {
	tempfilename=test$$.sh

	echo '#!/bin/sh' >$tempfilename
	echo '' >>$tempfilename
	echo 'exit 0' >>$tempfilename
	chmod 700 $tempfilename

	[ -x $tempfilename ] || {
		echo Cannot write to `pwd`
		echo 'Are you not running this program as root?'
		die
	}

	./$tempfilename || {
		echo Cannot execute a script.
		echo 'If /tmp is mounted noexec, please change this before continuing.'
		die
	}

}



download_this () {
	WGET=`which wget 2>/dev/null`
	CURL=`which curl 2>/dev/null`
	if [ -n "${WGET}" -a -x "${WGET}" ]; then
		$WGET $DOWNLOAD_SITE/$FILENAME >/dev/null 2>>$LOG || die
	else
		if [ -n "${CURL}" -a -x "${CURL}" ]; then
			$CURL $DOWNLOAD_SITE/$FILENAME >$FILENAME 2>>$LOG || die
		else
			echo Unable to find a wget or curl command.
			echo $SETUP cannot continue.
			die;
		fi
	fi
}



install_ical () {
	cd $BUILD >>$LOG 2>&1 || die
	FILENAME=libical-easyinstall.sum ; download_this
	SUM=`cat libical-easyinstall.sum`
	SUMFILE=$SUPPORT/etc/libical-easyinstall.sum
	if [ -r $SUMFILE ] ; then
		OLDSUM=`cat $SUMFILE`
		if [ "$SUM" = "$OLDSUM" ] ; then
			echo "* libical does not need updating."
			return
		fi
	fi
	echo "* Downloading libical..."
	FILENAME=$ICAL_SOURCE ; download_this
	echo "* Installing libical..."
	( gzip -dc $ICAL_SOURCE | tar -xf - ) >>$LOG 2>&1 || die
	cd $BUILD/libical >>$LOG 2>&1 || die
	./configure --prefix=$SUPPORT >>$LOG 2>&1 || die
	$MAKE $MAKEOPTS >>$LOG 2>&1 || die
	$MAKE install >>$LOG 2>&1 || die
	echo "  Complete."
	echo $SUM >$SUMFILE
	rm -f $CITADEL/citadel-easyinstall.sum 2>/dev/null
	rm -f $WEBCIT/webcit-easyinstall.sum 2>/dev/null
}

install_libsieve () {
	cd $BUILD >>$LOG 2>&1 || die
	FILENAME=libsieve-easyinstall.sum ; download_this
	SUM=`cat libsieve-easyinstall.sum`
	SUMFILE=$SUPPORT/etc/libsieve-easyinstall.sum
	if [ -r $SUMFILE ] ; then
		OLDSUM=`cat $SUMFILE`
		if [ "$SUM" = "$OLDSUM" ] ; then
			echo "* libsieve does not need updating."
			return
		fi
	fi
	echo "* Downloading libsieve..."
	FILENAME=$LIBSIEVE_SOURCE ; download_this
	echo "* Installing libsieve..."
	( gzip -dc $LIBSIEVE_SOURCE | tar -xf - ) >>$LOG 2>&1 || die
	cd $BUILD/libsieve-2.2.7/src >>$LOG 2>&1 || die
	./configure --prefix=$SUPPORT >>$LOG 2>&1 || die
	$MAKE $MAKEOPTS >>$LOG 2>&1 || die
	$MAKE install >>$LOG 2>&1 || die
	echo "  Complete."
	echo $SUM >$SUMFILE
	rm -f $CITADEL/citadel-easyinstall.sum 2>/dev/null
}

install_expat () {
	cd $BUILD >>$LOG 2>&1 || die
	FILENAME=expat-easyinstall.sum ; download_this
	SUM=`cat expat-easyinstall.sum`
	SUMFILE=$SUPPORT/etc/expat-easyinstall.sum
	if [ -r $SUMFILE ] ; then
		OLDSUM=`cat $SUMFILE`
		if [ "$SUM" = "$OLDSUM" ] ; then
			echo "* expat does not need updating."
			return
		fi
	fi
	echo "* Downloading expat..."
	FILENAME=$EXPAT_SOURCE ; download_this
	echo "* Installing Expat..."
	( gzip -dc $EXPAT_SOURCE | tar -xf - ) >>$LOG 2>&1 || die
	cd $BUILD/expat-2.0.1 >>$LOG 2>&1 || die
	./configure --prefix=$SUPPORT >>$LOG 2>&1 || die
	$MAKE $MAKEOPTS >>$LOG 2>&1 || die
	$MAKE install >>$LOG 2>&1 || die
	echo "  Complete."
	echo $SUM >$SUMFILE
	rm -f $CITADEL/citadel-easyinstall.sum 2>/dev/null
}


install_libcurl () {
	cd $BUILD >>$LOG 2>&1 || die
	FILENAME=libcurl-easyinstall.sum ; download_this
	SUM=`cat libcurl-easyinstall.sum`
	SUMFILE=$SUPPORT/etc/libcurl-easyinstall.sum
	if [ -r $SUMFILE ] ; then
		OLDSUM=`cat $SUMFILE`
		if [ "$SUM" = "$OLDSUM" ] ; then
			echo "* libcurl does not need updating."
			return
		fi
	fi
	echo "* Downloading libcurl..."
	FILENAME=$LIBCURL_SOURCE ; download_this
	echo "* Installing libcurl..."
	( gzip -dc $LIBCURL_SOURCE | tar -xf - ) >>$LOG 2>&1 || die
	CFLAGS="${CFLAGS} -I${SUPPORT}/include ${INCADD} -g"
	CPPFLAGS="${CFLAGS}"
	LDFLAGS="-L${SUPPORT}/lib -Wl,--rpath -Wl,${SUPPORT}/lib ${LDADD}"
	export CFLAGS CPPFLAGS LDFLAGS

	cd $BUILD/curl-7.26.0 >>$LOG 2>&1 || die
	./configure --prefix=$SUPPORT --disable-file --disable-ldap --disable-ldaps \
		--disable-dict --disable-telnet --disable-tftp --disable-manual \
		--enable-thread --disable-sspi --disable-crypto-auth --disable-cookies \
		--without-libssh2 --without-ca-path --without-libidn \
		--enable-ares=$SUPPORT \
		>>$LOG 2>&1 || die
	$MAKE $MAKEOPTS >>$LOG 2>&1 || die
	$MAKE install >>$LOG 2>&1 || die
	echo "  Complete."
	echo $SUM >$SUMFILE
	rm -f $CITADEL/citadel-easyinstall.sum 2>/dev/null
}


install_libev () {
	cd $BUILD >>$LOG 2>&1 || die
	FILENAME=libev-easyinstall.sum ; download_this
	SUM=`cat libev-easyinstall.sum`
	SUMFILE=$SUPPORT/etc/libev-easyinstall.sum
	if [ -r $SUMFILE ] ; then
		OLDSUM=`cat $SUMFILE`
		if [ "$SUM" = "$OLDSUM" ] ; then
			echo "* libev does not need updating."
			return
		fi
	fi
	echo "* Downloading libev..."
	FILENAME=$LIBEV_SOURCE ; download_this
	echo "* Installing libev..."
	( gzip -dc $LIBEV_SOURCE | tar -xf - ) >>$LOG 2>&1 || die
	cd $BUILD/libev-4.11 >>$LOG 2>&1 || die
	./configure --prefix=$SUPPORT >>$LOG 2>&1 || die
	$MAKE $MAKEOPTS >>$LOG 2>&1 || die
	$MAKE install >>$LOG 2>&1 || die
	echo "  Complete."
	echo $SUM >$SUMFILE
	rm -f $CITADEL/citadel-easyinstall.sum 2>/dev/null
}


install_cares () {
	cd $BUILD >>$LOG 2>&1 || die
	FILENAME=c-ares-easyinstall.sum ; download_this
	SUM=`cat c-ares-easyinstall.sum`
	SUMFILE=$SUPPORT/etc/c-ares-easyinstall.sum
	if [ -r $SUMFILE ] ; then
		OLDSUM=`cat $SUMFILE`
		if [ "$SUM" = "$OLDSUM" ] ; then
			echo "* c-ares does not need updating."
			return
		fi
	fi
	echo "* Downloading c-ares..."
	FILENAME=$CARES_SOURCE ; download_this
	echo "* Installing c-ares..."
	( gzip -dc $CARES_SOURCE | tar -xf - ) >>$LOG 2>&1 || die
	cd $BUILD/c-ares-1.7.5 >>$LOG 2>&1 || die
	./configure --prefix=$SUPPORT >>$LOG 2>&1 || die
	$MAKE $MAKEOPTS >>$LOG 2>&1 || die
	$MAKE install >>$LOG 2>&1 || die
	echo "  Complete."
	echo $SUM >$SUMFILE
	rm -f $SUPPORT/etc/libcurl-easyinstall.sum 2>/dev/null
	rm -f $CITADEL/citadel-easyinstall.sum 2>/dev/null
}

install_discount () {
	cd $BUILD >>$LOG 2>&1 || die
	cp /home/willi/Downloads/discount*z .
	FILENAME=discount-easyinstall.sum ; download_this
	SUM=`cat discount-easyinstall.sum`
	SUMFILE=$SUPPORT/etc/discount-easyinstall.sum
	if [ -r $SUMFILE ] ; then
		OLDSUM=`cat $SUMFILE`
		if [ "$SUM" = "$OLDSUM" ] ; then
			echo "* discount does not need updating."
			return
		fi
	fi
	echo "* Downloading discount..."
	FILENAME=$LIBDISCOUNT_SOURCE ; download_this
	echo "* Installing discount..."
	( gzip -dc $LIBDISCOUNT_SOURCE | tar -xf - ) >>$LOG 2>&1 || die
	cd $BUILD/discount-2.1.8 >>$LOG 2>&1 || die
        ./configure.sh --shared    \
	    --prefix=$SUPPORT      \
            --with-id-anchor       \
            --with-github-tags     \
            --with-fenced-code     \
            --with-dl=both         \
	    ||die

	$MAKE $MAKEOPTS >>$LOG 2>&1 || die
	$MAKE install >>$LOG 2>&1 || die
	echo "  Complete."
	echo $SUM >$SUMFILE
	rm -f $SUPPORT/etc/discount-easyinstall.sum 2>/dev/null
	rm -f $CITADEL/citadel-easyinstall.sum 2>/dev/null
}

install_libcitadel () {
	cd $BUILD >>$LOG 2>&1 || die
	FILENAME=libcitadel-easyinstall.sum ; download_this
	SUM=`cat libcitadel-easyinstall.sum`
	SUMFILE=$SUPPORT/etc/libcitadel-easyinstall.sum
	if [ -r $SUMFILE ] ; then
		OLDSUM=`cat $SUMFILE`
		if [ "$SUM" = "$OLDSUM" ] ; then
			echo "* libcitadel does not need updating."
			return
		fi
	fi
	echo "* Downloading libcitadel..."
	FILENAME=$LIBCITADEL_SOURCE ; download_this
	echo "* Installing libcitadel..."
	( gzip -dc $LIBCITADEL_SOURCE | tar -xf - ) >>$LOG 2>&1 || die
	cd $BUILD/libcitadel >>$LOG 2>&1 || die
	./configure --prefix=$SUPPORT >>$LOG 2>&1 || die
	$MAKE $MAKEOPTS >>$LOG 2>&1 || die
	$MAKE install >>$LOG 2>&1 || die
	echo "  Complete."
	echo $SUM >$SUMFILE
	# Upgrading libcitadel forces the upgrade of programs which link to it
	rm -f $CITADEL/citadel-easyinstall.sum 2>/dev/null
	rm -f $CITADEL/webcit-easyinstall.sum 2>/dev/null
	rm -f $CITADEL/textclient-easyinstall.sum 2>/dev/null
}

install_db () {
	cd $BUILD >>$LOG 2>&1 || die
	FILENAME=db-easyinstall.sum ; download_this
	SUM=`cat db-easyinstall.sum`
	SUMFILE=$SUPPORT/etc/db-easyinstall.sum
	if [ -r $SUMFILE ] ; then
		OLDSUM=`cat $SUMFILE`
		if [ "$SUM" = "$OLDSUM" ] ; then
			echo "* Berkeley DB does not need updating."
			return
		fi
	fi
	echo "* Downloading Berkeley DB..."
	FILENAME=$DB_SOURCE ; download_this
	echo "* Installing Berkeley DB..."
	( gzip -dc $DB_SOURCE | tar -xf - ) >>$LOG 2>&1 || die
	cd $BUILD/db-5.1.29.NC/build_unix >>$LOG 2>&1 || die
	../dist/configure --prefix=$SUPPORT --disable-compat185 --disable-cxx --disable-debug --disable-dump185 --disable-java --disable-tcl --disable-test --without-rpm >>$LOG 2>&1 || die
	$MAKE $MAKEOPTS >>$LOG 2>&1 || die
	$MAKE install >>$LOG 2>&1 || die
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
	if [ -z "$OK_LIBSIEVE" ]
	then
		install_libsieve
	fi
	if [ -z "$OK_DB" ]
	then
		install_db
	fi
	if [ -z "$OK_EXPAT" ]
	then
		install_expat
	fi
	if [ -z "$OK_LIBEV" ]
	then
		install_libev
	fi
	if [ -z "$OK_CARES" ]
	then
		install_cares
	fi
	if [ -z "$OK_DISCOUNT" ]
	then
		install_discount
	fi
	if [ -z "$OK_LIBCURL" ]
	then
		install_libcurl
	fi
}

install_sources () {

	install_libcitadel

	cd $BUILD >>$LOG 2>&1 || die
	if [ x$IS_UPGRADE = xyes ]
	then
		echo "* Upgrading your existing Citadel installation."
	fi

	CFLAGS="${CFLAGS} -I${SUPPORT}/include ${INCADD} -g"
	CPPFLAGS="${CFLAGS}"
	LDFLAGS="-L${SUPPORT}/lib -Wl,--rpath -Wl,${SUPPORT}/lib ${LDADD}"
	export CFLAGS CPPFLAGS LDFLAGS

	DO_INSTALL_CITADEL=yes
	FILENAME=citadel-easyinstall.sum ; download_this
	SUM=`cat citadel-easyinstall.sum`
	SUMFILE=$CITADEL/citadel-easyinstall.sum
	if [ -r $SUMFILE ] ; then
		OLDSUM=`cat $SUMFILE`
		if [ "$SUM" = "$OLDSUM" ] ; then
			echo "* Citadel does not need updating."
			DO_INSTALL_CITADEL=no
		fi
	fi

	if [ $DO_INSTALL_CITADEL = yes ] ; then
		echo "* Downloading Citadel..."
		FILENAME=$CITADEL_SOURCE ; download_this
		echo "* Installing Citadel..."
		cd $BUILD >>$LOG 2>&1 || die
		( gzip -dc $CITADEL_SOURCE | tar -xf - ) >>$LOG 2>&1 || die
		cd $BUILD/citadel >>$LOG 2>&1 || die
		if [ -z "$OK_DB" ]
		then
			./configure --prefix=$CITADEL --with-db=$SUPPORT --with-pam --with-libical --disable-threaded-client >>$LOG 2>&1 || die
		else
			./configure --prefix=$CITADEL --with-db=$OK_DB --with-pam --with-libical --disable-threaded-client >>$LOG 2>&1 || die
		fi
		$MAKE $MAKEOPTS >>$LOG 2>&1 || die
		if [ x$IS_UPGRADE = xyes ]
		then
			echo "* Performing Citadel upgrade..."
			$MAKE upgrade >>$LOG 2>&1 || die
		else
			echo "* Performing Citadel install..."
			$MAKE install >>$LOG 2>&1 || die
			useradd -c "Citadel service account" -d $CITADEL -s $CITADEL/citadel citadel >>$LOG 2>&1
		fi
		echo $SUM >$SUMFILE
	fi

	## begin webcit install

	cd $BUILD >>$LOG 2>&1 || die
	DO_INSTALL_WEBCIT=yes
	FILENAME=webcit-easyinstall.sum ; download_this
	SUM=`cat webcit-easyinstall.sum`
	SUMFILE=$WEBCIT/webcit-easyinstall.sum
	if [ -r $SUMFILE ] ; then
		OLDSUM=`cat $SUMFILE`
		if [ "$SUM" = "$OLDSUM" ] ; then
			echo "* WebCit does not need updating."
			DO_INSTALL_WEBCIT=no
		fi
	fi

	if [ $DO_INSTALL_WEBCIT = yes ] ; then
		echo "* Downloading WebCit..."
		FILENAME=$WEBCIT_SOURCE ; download_this
		echo "* Installing WebCit..."
		cd $BUILD >>$LOG 2>&1 || die
		( gzip -dc $WEBCIT_SOURCE | tar -xf - ) >>$LOG 2>&1 || die
		cd $BUILD/webcit >>$LOG 2>&1 || die
		./configure --prefix=$WEBCIT --with-libical >>$LOG 2>&1 || die
		$MAKE $MAKEOPTS >>$LOG 2>&1 || die
		rm -fr $WEBCIT/static 2>&1
		$MAKE install >>$LOG 2>&1 || die
		echo "  Complete."
		echo $SUM >$SUMFILE
	fi

	## begin text client install

	cd $BUILD >>$LOG 2>&1 || die
	DO_INSTALL_TEXTCLIENT=yes
	FILENAME=textclient-easyinstall.sum ; download_this
	SUM=`cat textclient-easyinstall.sum`
	SUMFILE=$CITADEL/webcit-easyinstall.sum
	if [ -r $SUMFILE ] ; then
		OLDSUM=`cat $SUMFILE`
		if [ "$SUM" = "$OLDSUM" ] ; then
			echo "* Citadel text mode client does not need updating."
			DO_INSTALL_TEXTCLIENT=no
		fi
	fi

	if [ $DO_INSTALL_TEXTCLIENT = yes ] ; then
		echo "* Downloading the Citadel text mode client..."
		FILENAME=$TEXTCLIENT_SOURCE ; download_this
		echo "* Installing the Citadel text mode client..."
		cd $BUILD >>$LOG 2>&1 || die
		( gzip -dc $TEXTCLIENT_SOURCE | tar -xf - ) >>$LOG 2>&1 || die
		cd $BUILD/textclient >>$LOG 2>&1 || die
		./configure --prefix=$CITADEL >>$LOG 2>&1 || die
		$MAKE $MAKEOPTS >>$LOG 2>&1 || die
		$MAKE install >>$LOG 2>&1 || die
		echo "  Complete."
		echo $SUM >$SUMFILE
	fi
}


do_config () {
	echo "* Configuring your system ..."

	if [ x$IS_UPGRADE = xyes ]
	then
		echo Upgrading your existing Citadel installation.
	else
		echo This is a new Citadel installation.
	fi

	if [ -x /etc/init.d/citadel ] ; then
		echo Stopping any previously running Citadel server...
		/etc/init.d/citadel stop
	fi

	# If we are running a Linux operating system we try even harder to stop citserver
	if uname -a | grep -i linux ; then
		if ps ax | grep citserver | grep -v grep ; then
			echo citserver still running ... trying again to terminate it
			killall citserver
			sleep 3
		fi
	fi

	# If we are running a Linux operating system we try even harder to stop citserver
	if uname -a | grep -i linux ; then
		if ps ax | grep citserver | grep -v grep ; then
			echo citserver still running ... trying again to terminate it
			killall -9 citserver
			sleep 3
		fi
	fi

	FILENAME=citadel-init-scripts.tar ; download_this
	cat citadel-init-scripts.tar | (cd / ; tar xvf - )
	echo Starting Citadel server...
	/etc/init.d/citadel start

	$CITADEL/setup </dev/tty || die
	$WEBCIT/setup </dev/tty || die
}



##### END Functions #####

##### BEGIN main #####


# 0. Test to make sure we're running as root

PERMSTESTDIR=/usr/local/ctdltest.$$
mkdir $PERMSTESTDIR || {
	echo
	echo 'Easy Install is unable to create subdirectories in /usr/local.'
	echo 'Did you forget to run the install command as the root user?'
	echo 'Please become root (with a command like "su" or "sudo su") and'
	echo 'try again.'
	echo
	exit 1
}
rmdir $PERMSTESTDIR 2>/dev/null

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

echo MAKE is $MAKE
export MAKE
rm -f $LOG 2>/dev/null


# Determine which operating system we are running on

OS=`uname -s`
REV=`uname -r`
MACH=`uname -m`

if [ "${OS}" = "SunOS" ] ; then
	OS=Solaris
	ARCH=`uname -p`	
	OSSTR="${OS} ${REV}(${ARCH} `uname -v`)"
elif [ "${OS}" = "AIX" ] ; then
	OSSTR="${OS} `oslevel` (`oslevel -r`)"
elif [ "${OS}" = "Linux" ] ; then
	KERNEL=`uname -r`
	if [ -f /etc/redhat-release ] ; then
		DIST='RedHat'
		PSUEDONAME=`cat /etc/redhat-release | sed s/.*\(// | sed s/\)//`
		REV=`cat /etc/redhat-release | sed s/.*release\ // | sed s/\ .*//`
	elif [ -f /etc/SUSE-release ] ; then
		DIST=`cat /etc/SUSE-release | tr "\n" ' '| sed s/VERSION.*//`
		REV=`cat /etc/SUSE-release | tr "\n" ' ' | sed s/.*=\ //`
	elif [ -f /etc/mandrake-release ] ; then
		DIST='Mandrake'
		PSUEDONAME=`cat /etc/mandrake-release | sed s/.*\(// | sed s/\)//`
		REV=`cat /etc/mandrake-release | sed s/.*release\ // | sed s/\ .*//`
	elif [ -f /etc/debian_version ] ; then
		DIST="Debian `cat /etc/debian_version`"
		REV=""

	fi
	if [ -f /etc/UnitedLinux-release ] ; then
		DIST="${DIST}[`cat /etc/UnitedLinux-release | tr "\n" ' ' | sed s/VERSION.*//`]"
	fi
	
	OSSTR="${OS} ${DIST} ${REV}(${PSUEDONAME} ${KERNEL} ${MACH})"

fi


rm -rf $BUILD
mkdir -p $BUILD
cd $BUILD


# 2. Present the installation steps (from 1 above) to the user
clear
if whiptail --infobox "Welcome to Citadel Easy Install" 10 70 2>/dev/null
then
	CTDL_DIALOG=`which whiptail`
	export CTDL_DIALOG
fi
clear

test_build_dir

echo "Welcome to $SETUP"
echo Running on: ${OSSTR}
echo "We will perform the following actions:"
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
if [ x$UNATTENDED_BUILD = "xYOU_BETCHA" ] ; then

	yesno=yes
else
	read yesno </dev/tty
fi

if [ "`echo $yesno | cut -c 1 | tr N n`" = "n" ]; then
	exit 2
fi


FILENAME=gpl.txt ; download_this
cat $FILENAME
echo ""
echo "Do you accept the terms of this license?"
echo "If you do not accept the General Public License, Easy Install will exit."
echo -n "Enter Y or Yes to accept: "
if [ x$UNATTENDED_BUILD = "xYOU_BETCHA" ] ; then
	yesno=yes
else
	read yesno </dev/tty
fi

if [ "`echo $yesno | cut -c 1 | tr N n`" = "n" ]; then
	exit 2
fi

echo
if [ -f $CITADEL/citadel.config ]
then
	IS_UPGRADE=yes
	echo "* Upgrading your existing Citadel installation."
else
	IS_UPGRADE=no
	echo "* This is a NEW Citadel installation."
fi

if [ x$IS_UPGRADE = xyes ]
then
	echo
	echo "This appears to be an upgrade of an existing Citadel installation."
	echo -n "Have you performed a FULL BACKUP of your programs and data? "
	if [ x$UNATTENDED_BUILD = "xYOU_BETCHA" ] ; then
		yesno=yes
	else
		read yesno </dev/tty
	fi

	if [ "`echo $yesno | cut -c 1 | tr N n`" = "n" ]; then
		echo
		echo "citadel.org does not provide emergency support for sites"
		echo "which broke irrecoverably during a failed upgrade."
		echo "Easy Install will now exit."
		exit 2
	fi
fi

clear

echo ""
echo "Installation will now begin."
echo "Command output will not be sent to the terminal."
echo "To view progress, see the $LOG file."
echo ""

# 3. Do the installation

install_prerequisites
install_sources

# 4. Do post-installation setup

if [ x$UNATTENDED_BUILD = "xYOU_BETCHA" ] ; then
	echo skipping config
else
	do_config
fi
rm -fr $BUILD

##### END main #####


