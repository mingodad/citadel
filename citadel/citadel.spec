# $Id$
Summary: Citadel, the flexible, powerful way to build online communities
Name: citadel
Version: 6.80
Release: 1
Copyright: GPL
Group: Applications/Communications
Source0: http://easyinstall.citadel.org/citadel-%{PACKAGE_VERSION}.tar.gz
Buildroot: /var/tmp/citadel-%{PACKAGE_VERSION}-root
#Icon: citux-64x64.xpm
Vendor: Citadel Development Team
URL:  http://www.citadel.org

# I don't think we should actually Require this because we can be configured
# not to connect to an LDAP server. So we really only require the client
# libraries, and the runtime dependency on that is detected automatically by
# RPM.
#Requires: openldap-servers

# Note some of these BuildRequire's are Linux specific and possibly even
# distribution specific, but I am adding them FOR NOW just to document the
# dependencies and make it more likely that everyone is building identical
# packages.
BuildRequires: gcc
BuildRequires: bison
BuildRequires: glibc-devel
BuildRequires: openldap-devel
BuildRequires: db4-devel >= 4.1
BuildRequires: pam-devel

# Newt is not supported by redhat for binary compatibility with future
# distributions, so we've disabled it.
#BuildRequires: newt-devel

BuildRequires: openssl-devel
BuildRequires: ncurses-devel
BuildRequires: zlib-devel

# debuginfo packages don't get built unless this packages is installed:
# NB: We do not DISTRIBUTE the debuginfo packages - error
BuildRequires: redhat-rpm-config

#Autoprov: false
ExcludeOS: hpux

%description
An advanced messaging system which can be used for BBS, groupware, and
online community applications.  It is multithreaded, client/server, database
driven, and accessible via a growing selection of front ends.

%prep
%setup -n citadel

%build
CFLAGS="$RPM_OPT_FLAGS" ./configure --with-pam --enable-autologin --with-ldap --without-newt --with-libical
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/etc/pam.d
make root=$RPM_BUILD_ROOT install
touch $RPM_BUILD_ROOT/usr/local/citadel/.hushlogin
# Things that shouldn't be "installed" but were anyway
rm -f $RPM_BUILD_ROOT/"/usr/local/citadel/help/?"
rm -f $RPM_BUILD_ROOT/usr/local/citadel/README.txt
rm -rf $RPM_BUILD_ROOT/usr/local/citadel/docs
rm -rf $RPM_BUILD_ROOT/usr/local/citadel/techdoc

%clean
rm -rf "$RPM_BUILD_ROOT"

%package server
Summary: Citadel, the flexible, powerful way to build online communities
Group: System Environment/Daemons
Requires: citadel-data
Obsoletes: citadel
Obsoletes: citadel-ux
Obsoletes: citadel-smtp
Obsoletes: citadel-pop3
Obsoletes: citadel-imap
Obsoletes: citadel-mrtg
Obsoletes: citadel-calendar
%description server
Citadel is an advanced messaging system which can be used for BBS, email,
groupware and online community applications.  It is multithreaded,
client/server, database driven, and accessible via a growing selection of
front ends.
%defattr(-,root,root)
%files server
/etc/pam.d/citadel
%doc docs/*
%doc techdoc
%doc README.txt
%dir /usr/local/citadel/bio
%dir /usr/local/citadel/bitbucket
%dir /usr/local/citadel/files
%dir /usr/local/citadel/images
%dir /usr/local/citadel/info
%dir /usr/local/citadel/userpics
/usr/local/citadel/aidepost
/usr/local/citadel/base64
%attr(4755,root,root) /usr/local/citadel/chkpwd
/usr/local/citadel/citmail
/usr/local/citadel/citserver
/usr/local/citadel/msgform
/usr/local/citadel/sendcommand
/usr/local/citadel/setup
/usr/local/citadel/stress
/usr/local/citadel/userlist
/usr/local/citadel/utilsmenu
%pre server
# Add the Citadel user
/usr/sbin/useradd -c "Citadel" -s /bin/false -r -d /usr/local/citadel \
		citadel 2> /dev/null || :
%post server
if [ -f /etc/inittab ]; then
	if ! grep 'citserver' /etc/inittab > /dev/null; then
		echo "c1:2345:respawn:/usr/local/citadel/citserver -h/usr/local/citadel -x7 -llocal4" >> /etc/inittab
	fi
fi
if [ -f /etc/services ]; then
	if ! grep '^citadel' /etc/services > /dev/null; then
		echo "citadel		504/tcp		# citadel" >> /etc/services
	fi
fi
cd /usr/local/citadel
/usr/local/citadel/setup -q
%postun server
if [ -f /etc/inittab ]; then
	grep -v 'citserver' < /etc/inittab > /etc/inittab.new && \
	mv -f /etc/inittab.new /etc/inittab
	/sbin/init q
fi

%package data
Summary: Data files for the Citadel messaging system.
Group: System Environment/Daemons
%description data
Default data files for the Citadel messaging system.  These files are
required by the Citadel server.
%defattr(-,root,root)
%files data
/usr/local/citadel/.hushlogin
#%dir /usr/local/citadel/netconfigs
%dir /usr/local/citadel/network
%dir /usr/local/citadel/network/spoolin
%dir /usr/local/citadel/network/spoolout
%dir /usr/local/citadel/network/systems
%config(noreplace) /usr/local/citadel/network/mail.aliases
%config(noreplace) /usr/local/citadel/public_clients
%config /usr/local/citadel/help/aide
%config /usr/local/citadel/help/software
%config /usr/local/citadel/help/floors
%config(noreplace) /usr/local/citadel/help/hours
%config /usr/local/citadel/help/intro
%config /usr/local/citadel/help/mail
%config /usr/local/citadel/help/network
%config /usr/local/citadel/help/nice
%config(noreplace) /usr/local/citadel/help/policy
%config /usr/local/citadel/help/summary
%config(noreplace) /usr/local/citadel/messages/changepw
%config /usr/local/citadel/messages/aideopt
%config(noreplace) /usr/local/citadel/messages/entermsg
%config /usr/local/citadel/messages/dotopt
%config /usr/local/citadel/messages/mainmenu
%config /usr/local/citadel/messages/entopt
%config(noreplace) /usr/local/citadel/messages/goodbye
%config(noreplace) /usr/local/citadel/messages/hello
%config /usr/local/citadel/messages/help
%config(noreplace) /usr/local/citadel/messages/register
%config(noreplace) /usr/local/citadel/messages/newuser
%config /usr/local/citadel/messages/readopt
%config(noreplace) /usr/local/citadel/messages/roomaccess
%config(noreplace) /usr/local/citadel/messages/unlisted
%post data
# Yes, this is supposed to be executed twice; as ? might not yet exist
# but we want it to be listed.  It's kludgey; sue me.
/bin/ls /usr/local/citadel/help > "/usr/local/citadel/help/?"
/bin/ls /usr/local/citadel/help > "/usr/local/citadel/help/?"

%package client
Summary: Client for the Citadel messaging system
Group: Applications/Communications
%description client
This is the text client software for the Citadel messaging system.
Install this software if you need to connect to a Citadel server.
%defattr(-,root,root)
%files client
/usr/local/citadel/citadel
/usr/local/citadel/citadel.rc
/usr/local/citadel/whobbs
%post client
if [ -f /etc/services ]; then
	if ! grep '^citadel' /etc/services > /dev/null; then
		echo "citadel		504/tcp		# citadel" >> /etc/services
	fi
fi
