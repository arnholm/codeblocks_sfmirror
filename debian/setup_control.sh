#!/bin/sh

# Run this in the root of the project. The current working dir should have
# the "debian" folder in it.

# Generate the debian/control file using debian/control.in as template.
# You can generate different control files for different Debian versions.
# The debian version is passed as argument or it is detected to match the
# running system. Pass "detect" as first argument if you want to force an
# explicit detection.

if [ "$#" -ne 1 ] || [ -z "${1##*detect*}" ]; then
    echo "Doing version detection"
    test -e /etc/os-release && os_release='/etc/os-release' || os_release='/usr/lib/os-release'
    . "$os_release"
    if [ $ID = "debian" ] || [ $ID = "ubuntu" ]; then
        version=$VERSION_CODENAME
    else
        version=`cat /etc/debian_version`
    fi
else
    version=$1
fi

echo "Found version: $version"

if [ -z "${version##*stretch*}" ] || \
   [ -z "${version##*9.*}" ] || \
   [ -z "${version##*buster*}" ] || \
   [ -z "${version##*10.*}" ];
then
    echo "Distro is matching wxGTK 3.0 + GTK2"
    CB_WXGTK_DEPS=libwxgtk3.0-dev
    CB_GTK_DEPS=libgtk2.0-dev
elif [ $version = "bullseye" ] || \
     [ $version = "focal" ] || \
     [ $version = "jammy" ] || \
     [ -z "${version##*11.*}" ];
then
    echo "Distro is matching wxGTK 3.0 + GTK3"
    CB_WXGTK_DEPS=libwxgtk3.0-gtk3-dev
    CB_GTK_DEPS=libgtk-3-dev
else
    echo "Distro is matching wxGTK 3.2 + GTK3"
    CB_WXGTK_DEPS=libwxgtk3.2-dev
    CB_GTK_DEPS=libgtk-3-dev
fi

echo "Setting dependencies to '$CB_WXGTK_DEPS $CB_GTK_DEPS' "

echo "Creating the debian/control file"
sed -e "s/@CB_WXGTK_DEPS@/$CB_WXGTK_DEPS/g" \
    -e "s/@CB_GTK_DEPS@/$CB_GTK_DEPS/g" \
    debian/control.in > debian/control
