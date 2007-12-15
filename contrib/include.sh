export VSERVER_ROOT=/var/lib/vservers/
export CIT_APACHE_DIR=/home/debiancitadel/
export APACHE_ROOT=${VSERVER_ROOT}/apache/${CIT_APACHE_DIR}
export TARGETS=/home/citbuild/targets
export CHECKOUT_DIR=/home/checkout/
export CONTRIB_DIR=/home/contrib/

# retrieve the debian sources.
# $1: the VSERVER_BASE
# $2: the inner Directory, aka CHECKOUT_DIR
# $3: the Program Component.
GetSource()
{
    cd ${VSERVER_ROOT}/$1/${2}; apt-get source $3
}

# compile the sources of one component.
# $1: the VSERVER_BASE
# $2: the inner Directory, aka CHECKOUT_DIR
# $3: the Program Component.
BuildSource ()
{
    chroot ${VSERVER_ROOT}/$1/ /bin/bash -c "cd ${2}/${3}*; fakeroot dpkg-buildpackage"
}

# Install a contrib library into the build system.
# $1: the VSERVER_BASE
# $2: the inner Directory, aka CHECKOUT_DIR
# $3: the Program Component.
InstallContrib ()
{
    chroot ${VSERVER_ROOT}/$1/ /bin/bash -c "cd ${2}; dpkg -i ${3}*.deb"
}


# $1: the VSERVER_BASE
# $2: the distro dir on the webserver
UpperResults()
{
    mv ${VSERVER_ROOT}/${1}/${CHECKOUT_DIR}/*.deb $APACHE_ROOT/public_html/$2/
    cp ${VSERVER_ROOT}/${1}/${CONTRIB_DIR}/*.deb $APACHE_ROOT/public_html/$2/
}

# $1: the VSERVER_BASE
# $2: the inner Directory, aka CHECKOUT_DIR
CleanBuild()
{
    rm -rf ${VSERVER_ROOT}/$1/${2}/*
}


# $1: the distro dir on the webserver
CleanApache()
{
    rm -f $APACHE_ROOT/public_html/$1/*.deb
}


# Install a contrib library into the build system.
# $1: the VSERVER_BASE
UpgradeBuildTarget ()
{
    chroot ${VSERVER_ROOT}/$1/ /bin/bash -c "apt-get update; apt-get upgrade"
}
