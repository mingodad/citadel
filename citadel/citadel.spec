# $Id$
Summary: Citadel/UX
Name: citadel
Version: 5.53
Release: 1
Copyright: GPL
Group: Applications/Communications
Source0: citadel-ux-%{PACKAGE_VERSION}.tar.gz
Buildroot: /var/tmp/citadel-%{PACKAGE_VERSION}-root
Autoprov: false

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
find $RPM_BUILD_ROOT/usr/local/citadel -type d | sed "s|$RPM_BUILD_ROOT|%dir |" > filelist
find $RPM_BUILD_ROOT -type f | egrep -v '(citadel\.rc|public_clients|/help/|/messages/|/network/)' | sed "s|$RPM_BUILD_ROOT||" >> filelist
find $RPM_BUILD_ROOT -type f | egrep '(citadel\.rc|public_clients|/help/|/messages/|/network/)' | sed "s|$RPM_BUILD_ROOT|%config |" >> filelist
chmod u+s $RPM_BUILD_ROOT/usr/local/citadel/chkpwd

%clean
rm -rf "$RPM_BUILD_ROOT"
rm -f filelist

%files -f filelist
%defattr(-,root,root)
