# $Id$
Summary: Citadel/UX, the flexible, powerful way to build online communities
Name: citadel
Version: 5.90
Release: 1
Copyright: GPL
Group: Applications/Communications
Source0: http://uncensored.citadel.org/pub/citadel/citadel-ux-%{PACKAGE_VERSION}.tar.gz
Buildroot: /var/tmp/citadel-%{PACKAGE_VERSION}-root
Icon: citux-64x64.xpm
Vendor: Citadel/UX Development Team
URL:  http://uncensored.citadel.org/citadel/
#Autoprov: false
ExcludeOS: hpux

%description
An advanced messaging system which can be used for BBS, groupware, and
online community applications.  It is multithreaded, client/server, database
driven, and accessible via a growing selection of front ends.

%prep
%setup -n citadel

%build
CFLAGS="$RPM_OPT_FLAGS" ./configure --with-pam
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/etc/pam.d
make root=$RPM_BUILD_ROOT install
touch $RPM_BUILD_ROOT/usr/local/citadel/.hushlogin

%clean
rm -rf "$RPM_BUILD_ROOT"

%package server
Summary: Citadel/UX, the flexible, powerful way to build online communities
Group: System Environment/Daemons
Requires: citadel-data
Obsoletes: citadel
Obsoletes: citadel-ux
%description server
Citadel/UX is an advanced messaging system which can be used for BBS,
groupware, and online community applications.  It is multithreaded,
client/server, database driven, and accessible via a growing selection of
front ends.  Remember to run /usr/local/citadel/setup after installing or
upgrading this package.
%defattr(-,root,root)
%files server
/etc/pam.d/citadel
%doc docs/chat.txt
%doc docs/citadel-with-berkeley-db.txt
%doc docs/COPYING.txt
%doc docs/copyright.txt
%doc docs/import-export.txt
%doc docs/inetsiteconfig.txt
%doc docs/mailinglists.txt
%doc docs/room-sharing-howto.txt
%doc docs/siteconfig.txt
%doc docs/sysop.txt
%doc docs/upgrading.txt
%doc docs/utils.txt
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
/usr/local/citadel/libcitserver.so
/usr/local/citadel/libcitserver.la
/usr/local/citadel/migratenet
/usr/local/citadel/msgform
/usr/local/citadel/readlog
/usr/local/citadel/sendcommand
/usr/local/citadel/setup
/usr/local/citadel/stats
/usr/local/citadel/userlist
/usr/local/citadel/utilsmenu
/usr/local/citadel/weekly
/usr/local/citadel/modules/libchat.so
/usr/local/citadel/modules/libchat.la
/usr/local/citadel/modules/libvcard.so
/usr/local/citadel/modules/libvcard.la
/usr/local/citadel/modules/libupgrade.so
/usr/local/citadel/modules/libupgrade.la
/usr/local/citadel/modules/libnetwork.so
/usr/local/citadel/modules/libnetwork.la
/usr/local/citadel/modules/libpas2.so
/usr/local/citadel/modules/libpas2.la
/usr/local/citadel/modules/libinetcfg.so
/usr/local/citadel/modules/libinetcfg.la
/usr/local/citadel/modules/librwho.so
/usr/local/citadel/modules/librwho.la
/usr/local/citadel/modules/libmoderate.so
/usr/local/citadel/modules/libmoderate.la
/usr/local/citadel/modules/libbio.so
/usr/local/citadel/modules/libbio.la
/usr/local/citadel/modules/libexpire.so
/usr/local/citadel/modules/libexpire.la
/usr/local/citadel/modules/libvandelay.so
/usr/local/citadel/modules/libvandelay.la
/usr/local/citadel/modules/libical.so
/usr/local/citadel/modules/libical.la
%pre server
# Add the "bbs" user
/usr/sbin/useradd -c "Citadel" -s /bin/false -r -d /usr/local/citadel \
		citadel 2> /dev/null || :
%post server
if [ -f /etc/inittab ]; then
	if ! grep 'citserver' /etc/inittab > /dev/null; then
		echo "c1:2345:/usr/local/citadel/citserver -h/usr/local/citadel -t/usr/local/citadel/citserver.trace" >> /etc/inittab
	fi
fi
if [ -f /etc/services ]; then
	if ! grep '^citadel' /etc/services > /dev/null; then
		echo "citadel		504/tcp		# citadel" >> /etc/services
	fi
fi
/sbin/ldconfig -n /usr/local/citadel /usr/local/citadel/modules
cd /usr/local/citadel
/usr/local/citadel/setup -q
%postun server
if [ -f /etc/inittab ]; then
	grep -v 'citserver' < /etc/inittab > /etc/inittab.new
	mv /etc/inittab.new /etc/inittab
	/sbin/init q
fi

%package data
Summary: Data files for the Citadel/UX messaging system.
Group: System Environment/Daemons
%description data
Default data files for the Citadel/UX messaging system.  These files are
required by the Citadel/UX server.
%defattr(-,root,root)
%files data
/usr/local/citadel/.hushlogin
#%dir /usr/local/citadel/netconfigs
%dir /usr/local/citadel/network
%dir /usr/local/citadel/network/spoolin
%dir /usr/local/citadel/network/spoolout
%dir /usr/local/citadel/network/systems
%config(noreplace) /usr/local/citadel/network/filterlist
%config(noreplace) /usr/local/citadel/network/mail.aliases
%config(noreplace) /usr/local/citadel/network/mailinglists
%config(noreplace) /usr/local/citadel/network/rnews.xref
%config(noreplace) /usr/local/citadel/public_clients
# KLUDGE!!!!  This catches help/? otherwise RPM barfs on it
# Drawback, it's not marked as a config file, oh well
/usr/local/citadel/help
#%config /usr/local/citadel/help/?
#%config /usr/local/citadel/help/aide
#%config /usr/local/citadel/help/software
#%config /usr/local/citadel/help/floors
#%config(noreplace) /usr/local/citadel/help/hours
#%config /usr/local/citadel/help/intro
#%config /usr/local/citadel/help/mail
#%config /usr/local/citadel/help/network
#%config /usr/local/citadel/help/nice
#%config(noreplace) /usr/local/citadel/help/policy
#%config /usr/local/citadel/help/summary
%config /usr/local/citadel/messages/changepw
%config /usr/local/citadel/messages/aideopt
%config /usr/local/citadel/messages/entermsg
%config /usr/local/citadel/messages/dotopt
%config /usr/local/citadel/messages/mainmenu
%config /usr/local/citadel/messages/entopt
%config(noreplace) /usr/local/citadel/messages/goodbye
%config(noreplace) /usr/local/citadel/messages/hello
%config /usr/local/citadel/messages/help
%config(noreplace) /usr/local/citadel/messages/register
%config(noreplace) /usr/local/citadel/messages/newuser
%config /usr/local/citadel/messages/readopt
%config /usr/local/citadel/messages/roomaccess
%config(noreplace) /usr/local/citadel/messages/unlisted

%package client
Summary: Client for the Citadel/UX messaging system
Group: Applications/Communications
%description client
This is the text client software for the Citadel/UX messaging system.
Install this software if you need to connect to a Citadel/UX server.
%defattr(-,root,root)
%files client
/usr/local/citadel/citadel
/usr/local/citadel/citadel.rc
%doc docs/chat.txt
%post client
if [ -f /etc/services ]; then
	if ! grep '^citadel' /etc/services > /dev/null; then
		echo "citadel		504/tcp		# citadel" >> /etc/services
	fi
fi

%package smtp
Summary: SMTP server for the Citadel/UX messaging system
Group: System Environment/Daemons
%description smtp
This package provides the Citadel/UX SMTP service, which provides inbound
and outbound SMTP service for the Citadel/UX messaging system.  Install this
package if your Citadel/UX users should be able to send and receive Internet
e-mail.  If you also run another SMTP server you will need to read
docs/inetmailsetupmx.txt to configure SMTP service.
%defattr(-,root,root)
%files smtp
/usr/local/citadel/modules/libsmtp.so
/usr/local/citadel/modules/libsmtp.la
%doc docs/inetmailsetupmx.txt
%doc docs/inetmailsetup.txt

%package imap
Summary: IMAP server for the Citadel/UX messaging system
Group: System Environment/Daemons
%description imap
This package provides the Citadel/UX IMAP service, which provides IMAP
connectivity.  Install this package if you want to connect to the Citadel/UX
server with IMAP clients such as Outlook Express or Netscape.  Using this
access method, users can access both e-mail and all public rooms on the server.
%defattr(-,root,root)
%files imap
/usr/local/citadel/modules/libimap.so
/usr/local/citadel/modules/libimap.la

%package pop3
Summary: POP3 server for the Citadel/UX messaging system
Group: System Environment/Daemons
%description pop3
This package provides the Citadel/UX POP3 service, which provides POP3
connectivity.  Install this package if you want to connect to the Citadel/UX
server with POP3 clients such as Outlook Express or Netscape.  Note that the
POP3 client can only receive mail; install citadel-smtp as well if you want
users to be able to send mail.
%defattr(-,root,root)
%files pop3
/usr/local/citadel/modules/libpop3.so
/usr/local/citadel/modules/libpop3.la

