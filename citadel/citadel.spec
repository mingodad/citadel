# $Id$
Summary: Citadel/UX 5.50beta1
Name: citadel
Version: 5.50
Release: 0.3
Copyright: GPL
Group: Applications/Communications
Source0: citadel.tar.gz
Buildroot: /var/tmp/citadel-%{PACKAGE_VERSION}-root
Autoprov: false

%description
An advanced messaging system which can be used for BBS, groupware, and
online community applications.  It is multithreaded, client/server, database
driven, and accessible via a growing selection of front ends.
%prep
%setup -n citadel

%build
CFLAGS="$RPM_OPT_FLAGS" ./configure
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/local/citadel
make prefix=$RPM_BUILD_ROOT/usr/local/citadel install
find $RPM_BUILD_ROOT/usr/local/citadel -type d | sed "s|$RPM_BUILD_ROOT|%dir |" > filelist
find $RPM_BUILD_ROOT -type f | egrep -v '(citadel\.rc|public_clients|/help/|/messages/|/network/)' | sed "s|$RPM_BUILD_ROOT||" >> filelist
find $RPM_BUILD_ROOT -type f | egrep '(citadel\.rc|public_clients|/help/|/messages/|/network/)' | sed "s|$RPM_BUILD_ROOT|%config |" >> filelist

%clean
rm -rf "$RPM_BUILD_ROOT"
rm -f filelist

%files -f filelist
