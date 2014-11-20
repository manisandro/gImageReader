#!/bin/sh

arch=${1:-i686}

if [ "$arch" == "i686" ]; then
    bits=32
elif [ "$arch" == "x86_64" ]; then
    bits=64
else
    echo "Error: unrecognized architecture $arch"
    exit 1
fi

iface=${2:-qt4}

# Note: This script is written to be used with the Fedora mingw environment
MINGWROOT=/usr/$arch-w64-mingw32/sys-root/mingw

# Halt on errors
set -e

if [ "$2" == "--debug" ]; then
    withdebug=1
fi

win32dir="$(dirname $(readlink -f $0))"
srcdir="$win32dir/../../"
builddir="$win32dir/../../build/mingw$bits"
installroot="$builddir/root"

# Build
rm -rf $builddir
mkdir -p $builddir
pushd $builddir > /dev/null
mingw$bits-cmake -DINTERFACE_TYPE=$iface ../../
mingw$bits-make -j4 DESTDIR="${installroot}_" install
mv ${installroot}_$MINGWROOT $installroot
rm -rf ${installroot}_
cp -R $win32dir/skel/* $installroot
cp $win32dir/gimagereader-icon.rc $builddir
cp $win32dir/gimagereader.ico $builddir
cp $win32dir/installer.nsi $builddir

# Collect dependencies
function isnativedll {
    # If the import library exists but not the dynamic library, the dll ist most likely a native one
    local lower=${1,,}
    [ ! -e $MINGWROOT/bin/$1 ] && [ -f $MINGWROOT/lib/lib${lower/%.*/.a} ] && return 0;
    return 1;
}

function linkDep {
# Link the specified binary dependency and it's dependencies
    local destdir="$installroot/${2:-$(dirname $1)}"
    local name="$(basename $1)"
    test -e "$destdir/$name" && return 0
    echo "Linking $1..."
    [ ! -e "$MINGWROOT/$1" ] && (echo "Error: missing $MINGWROOT/$1"; return 1)
    mkdir -p "$destdir" || return 1
    ln -sf "$MINGWROOT/$1" "$destdir/$name" || return 1
    autoLinkDeps $destdir/$name || return 1
    if [ $withdebug ]; then
        [ -e "$MINGWROOT/$1.debug" ] && ln -sf "$MINGWROOT/$1.debug" "$destdir/$name.debug" || echo "Warning: missing $name.debug"
    fi
    return 0
}

function autoLinkDeps {
# Collects and links the dependencies of the specified binary
    for dep in $(mingw-objdump -p "$1" | grep "DLL Name" | awk '{print $3}'); do
        if ! isnativedll "$dep"; then
            linkDep bin/$dep || return 1
        fi
    done
    return 0
}

autoLinkDeps root/bin/gimagereader.exe
linkDep bin/gdb.exe
linkDep bin/gspawn-win$bits-helper-console.exe
linkDep bin/gspawn-win$bits-helper.exe

linkDep bin/twaindsm.dll
linkDep lib/enchant/libenchant_myspell.dll
linkDep lib/pango/1.8.0/modules/pango-arabic-lang.dll
linkDep lib/pango/1.8.0/modules/pango-indic-lang.dll
linkDep lib/pango/1.8.0/modules/pango-basic-fc.dll
linkDep lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-pcx.dll
linkDep lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-xbm.dll
linkDep lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-ani.dll
linkDep lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-wbmp.dll
linkDep lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-pnm.dll
linkDep lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-icns.dll
linkDep lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-tga.dll
linkDep lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-jasper.dll
linkDep lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-qtif.dll
linkDep lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-xpm.dll
linkDep lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-ras.dll


install -Dpm 0644 /usr/share/glib-2.0/schemas/org.gtk.Settings.FileChooser.gschema.xml $installroot/share/glib-2.0/schemas/org.gtk.Settings.FileChooser.gschema.xml
# Install locale files
(
    cd $MINGWROOT
    for file in $(find share/locale -type f -name "gtk*.mo" -or -name "glib*.mo" -or -name "gdk*.mo" -or -name "atk*.mo"); do
        install -Dpm 0644 $file $installroot/$file
    done
)

# Add english language data and spelling dictionaries
install -Dpm 0644 /usr/share/tesseract/tessdata/eng.traineddata $installroot/share/tessdata/eng.traineddata
install -Dpm 0644 /usr/share/myspell/en_US.dic $installroot/share/myspell/dicts/en_US.dic
install -Dpm 0644 /usr/share/myspell/en_US.aff $installroot/share/myspell/dicts/en_US.aff

# Copy isocodes
install -Dpm 0644 /usr/share/xml/iso-codes/iso_639.xml $installroot/share/xml/iso-codes/iso_639.xml
install -Dpm 0644 /usr/share/xml/iso-codes/iso_3166.xml $installroot/share/xml/iso-codes/iso_3166.xml

# Remove unused files
rm -rf root/share/applications

# Compile schemas
glib-compile-schemas root/share/glib-2.0/schemas

# Build the installer
progName=$(grep -oP 'SET\(PACKAGE_NAME \K(\w+)(?=\))' $srcdir/CMakeLists.txt)
progVersion=$(grep -oP 'SET\(PACKAGE_VERSION \K([\d\.]+)(?=\))' $srcdir/CMakeLists.txt)
makensis -DNAME=$progName -DARCH=$arch -DPROGVERSION="$progVersion" installer.nsi;

# Cleanup
rm -rf $builddir/root

echo "Installer written to $PWD/${progName}_${progVersion}_${arch}.exe"
