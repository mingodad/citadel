# $Id$
Summary: Citadel/UX 5.50alpha3
Name: citadel
Version: 5.50
Release: 0.2
Copyright: GPL
Group: Applications/Communications
Source0: citadel.tar.gz
Buildroot: /var/tmp/citadel-%{PACKAGE_VERSION}-root
Autoprov: false

%description
An implementation of the Citadel BBS program for Unix systems. This is the de
facto standard Unix version of Citadel, and is now an advanced client/server
application.

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
