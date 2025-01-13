#!/bin/sh

arch=${1:-x86_64}

if [ "$arch" == "i686" ]; then
    bits=32
elif [ "$arch" == "x86_64" ]; then
    bits=64
else
    echo "Error: unrecognized architecture $arch"
    exit 1
fi

iface=${2:-qt6}

# Note: This script is written to be used with the Fedora mingw environment
MINGWROOT=/usr/$arch-w64-mingw32/sys-root/mingw

optflags="-g -pipe -Wall -Wp,-D_FORTIFY_SOURCE=2 -fexceptions --param=ssp-buffer-size=4 -fno-omit-frame-pointer"

# Halt on errors
set -e

if [ "$3" == "debug" ]; then
    withdebug=1
    optflags+=" -O0"
else
    optflags+=" -O2"
fi

export MINGW32_CFLAGS="$optflags"
export MINGW32_CXXFLAGS="$optflags"
export MINGW64_CFLAGS="$optflags"
export MINGW64_CXXFLAGS="$optflags"

win32dir="$(dirname $(readlink -f $0))"
srcdir="$win32dir/../../"
builddir="$win32dir/../../build/mingw$bits-$iface"
installroot="$builddir/root"

# Build
rm -rf $builddir
mkdir -p $builddir
pushd $builddir > /dev/null
mingw$bits-cmake -DINTERFACE_TYPE=$iface ../../
mingw$bits-make -j4 DESTDIR="${installroot}_" install VERBOSE=1
mv ${installroot}_$MINGWROOT $installroot
rm -rf ${installroot}_
cp $win32dir/gimagereader-icon.rc $builddir
cp $win32dir/gimagereader.ico $builddir
cp $win32dir/installer.nsi $builddir

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
        [ -e "/usr/lib/debug${MINGWROOT}/$1.debug" ] && lnk "/usr/lib/debug${MINGWROOT}/$1.debug" "$destdir/$name.debug" || :
        [ -e "$MINGWROOT/$1.debug" ] && lnk "$MINGWROOT/$1.debug" "$destdir/$name.debug" || :
    fi
    return 0
}

function autoLinkDeps {
# Collects and links the dependencies of the specified binary
    for dep in $(mingw-objdump -p "$1" | grep "DLL Name" | awk '{print $3}'); do
        if [ -e $MINGWROOT/bin/$dep ]; then
            linkDep bin/$dep || return 1
        fi
    done
    return 0
}

autoLinkDeps $installroot/bin/gimagereader-$iface.exe
linkDep bin/gdb.exe

linkDep bin/twaindsm.dll
linkDep lib/enchant-2/enchant_hunspell.dll
linkDep lib/ossl-modules/legacy.dll

cp -R $win32dir/skel/* $installroot

if [ "$iface" == "gtk" ]; then

    linkDep bin/gspawn-win$bits-helper-console.exe
    linkDep bin/gspawn-win$bits-helper.exe
    for loader in $(ls -1 $MINGWROOT/lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-*.dll); do
      linkDep $(echo $loader | sed "s|^$MINGWROOT/||")
    done

    # Install locale files
    (
        cd $MINGWROOT
        for file in $(find share/locale -type f -name "gtk*.mo" -or -name "glib*.mo" -or -name "gdk*.mo" -or -name "atk*.mo"); do
            install -Dpm 0644 $file $installroot/$file
        done
    )

    # Copy skeleton
    cp -R $win32dir/gtk_skel/* $installroot

    # Install and compile schemas
    install -Dpm 0644 /usr/share/glib-2.0/schemas/org.gtk.Settings.FileChooser.gschema.xml $installroot/share/glib-2.0/schemas/org.gtk.Settings.FileChooser.gschema.xml
    glib-compile-schemas $installroot/share/glib-2.0/schemas

elif [ "$iface" == "qt5" ] || [ "$iface" == "qt6" ]; then

    linkDep $(ls $MINGWROOT/bin/libssl-*.dll | sed "s|^$MINGWROOT/||")
    linkDep $(ls $MINGWROOT/bin/libcrypto-*.dll | sed "s|^$MINGWROOT/||")
    linkDep lib/$iface/plugins/imageformats/qgif.dll  bin/imageformats
    linkDep lib/$iface/plugins/imageformats/qicns.dll bin/imageformats
    linkDep lib/$iface/plugins/imageformats/qico.dll  bin/imageformats
    linkDep lib/$iface/plugins/imageformats/qjp2.dll  bin/imageformats
    linkDep lib/$iface/plugins/imageformats/qjpeg.dll bin/imageformats
    linkDep lib/$iface/plugins/imageformats/qtga.dll  bin/imageformats
    linkDep lib/$iface/plugins/imageformats/qtiff.dll bin/imageformats
    linkDep lib/$iface/plugins/imageformats/qwbmp.dll bin/imageformats
    linkDep lib/$iface/plugins/imageformats/qwebp.dll bin/imageformats
    linkDep lib/$iface/plugins/platforms/qwindows.dll bin/platforms
    if [ "$iface" == "qt5" ]; then
        linkDep lib/$iface/plugins/styles/qwindowsvistastyle.dll bin/styles
    elif [ "$iface" == "qt6" ]; then
        linkDep lib/$iface/plugins/styles/qmodernwindowsstyle.dll bin/styles
    fi

    # Install locale files
    mkdir -p $installroot/share/$iface/translations/
    cp -a $MINGWROOT/share/$iface/translations/{qt_*.qm,qtbase_*.qm,QtSpell_*.qm}  $installroot/share/$iface/translations
    rm -f $installroot/share/$iface/translations/qt_help_*.qm

fi

# Add english language data, poppler-data and spelling dictionaries
install -Dpm 0644 /usr/share/tesseract/tessdata/eng.traineddata $installroot/share/tessdata/eng.traineddata
install -Dpm 0644 /usr/share/hunspell/en_US.dic $installroot/share/hunspell/en_US.dic
install -Dpm 0644 /usr/share/hunspell/en_US.aff $installroot/share/hunspell/en_US.aff
cp -r "/usr/share/poppler/" "$installroot/share/"

# Copy isocodes
install -Dpm 0644 /usr/share/xml/iso-codes/iso_639.xml $installroot/share/xml/iso-codes/iso_639.xml
install -Dpm 0644 /usr/share/xml/iso-codes/iso_3166.xml $installroot/share/xml/iso-codes/iso_3166.xml
(
    cd /usr/
    for file in $(find share/locale -type f -name "iso_*.mo"); do
        install -Dpm 0644 $file $installroot/$file
    done
)

# Remove unused files
rm -rf $installroot/share/applications
rm -rf $installroot/share/appdata

# List installed files
(
    cd $installroot
    find -type f -or -type l | sed 's|/|\\|g' | sed -E 's|^\.(.*)$|Delete "\$INSTDIR\1"|g' > $builddir/unfiles.nsi

    # Ensure custom tessdata and spelling files are deleted
    echo 'Delete "$INSTDIR\share\hunspell\*"' >> $builddir/unfiles.nsi
    echo 'Delete "$INSTDIR\share\tessdata\*"' >> $builddir/unfiles.nsi

    # Ensure legacy spelling dictionaries location is cleaned up
    echo 'Delete "$INSTDIR\share\myspell\dicts\*"' >> $builddir/unfiles.nsi
    echo 'RMDir "$INSTDIR\share\myspell\dicts"' >> $builddir/unfiles.nsi
    echo 'Delete "$INSTDIR\share\myspell\*"' >> $builddir/unfiles.nsi
    echo 'RMDir "$INSTDIR\share\myspell"' >> $builddir/unfiles.nsi

    # Ensure potential log files are deleted
    echo 'Delete "$INSTDIR\gimagereader.log"' >> $builddir/unfiles.nsi
    echo 'Delete "$INSTDIR\twain.log"' >> $builddir/unfiles.nsi

    find -type d -depth | sed 's|/|\\|g' | sed -E 's|^\.(.*)$|RMDir "\$INSTDIR\1"|g' >> $builddir/unfiles.nsi
)

progName=$(grep -oP 'SET\(PACKAGE_NAME \K(\w+)(?=\))' $srcdir/CMakeLists.txt)
progVersion=$(grep -oP 'SET\(PACKAGE_VERSION \K([\d\.]+)(?=\))' $srcdir/CMakeLists.txt)
if [ $withdebug ]; then
    variant="_debug"
fi

if [ ! -z $4 ]; then
    progVersion="$4"
fi

# Build portable zip
pushd $builddir
ln -s root ${progName}_${progVersion}_${iface}
zip -r "${progName}_${progVersion}_${iface}_${arch}_portable.zip" ${progName}_${progVersion}_${iface}
rm ${progName}_${progVersion}_${iface}
popd

# Build the installer
makensis -DNAME=$progName -DARCH=$arch -DVARIANT="$variant" -DPROGVERSION="$progVersion" -DIFACE="$iface" installer.nsi;

# Cleanup
rm -rf $installroot

echo "Installer written to $PWD/${progName}_${progVersion}_${iface}_${arch}.exe"
