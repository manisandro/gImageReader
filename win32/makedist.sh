#!/bin/sh

# Note: This script is written to be used with the Fedora mingw environment
MINGWROOT=/usr/i686-w64-mingw32/sys-root/mingw

# Halt on errors
set -e

if [ "$1" == "--debug" ]; then
	withdebug=1
fi

win32dir="$(dirname $(readlink -f $0))"
pushd "$win32dir" > /dev/null

# Build
pushd .. > /dev/null
mkdir -p $win32dir/_root
mingw32-configure --disable-versioncheck
mingw32-make -j4 DESTDIR="$win32dir/_root" install
rm -rf $win32dir/root
mv $win32dir/_root$MINGWROOT $win32dir/root
rm -rf $win32dir/_root
cp -R $win32dir/skel/* $win32dir/root
popd > /dev/null

# Collect dependencies
function linkdep {
	echo "Linking $1..."
	test -e "$MINGWROOT/$1" || return 1
	mkdir -p "root/$(dirname $1)" || return 1
	ln -sf "$MINGWROOT/$1" "root/$1" || return 1
	if [ $withdebug ]; then test -e "$MINGWROOT/$1.debug" && ln -sf "$MINGWROOT/$1.debug" "root/$1.debug"; fi
	return 0
}

function cpdep {
	echo "Copying $1..."
	mkdir -p "root/$(dirname $1)" || return 1
	cp -R "$MINGWROOT/$1" "root/$1" || return 1
	return 0
}

function finddeps {
	local deps
	read -a deps <<< $(mingw-objdump -p "$1" | grep "DLL Name" | awk '{print $3}')
    for dep in "${deps[@]}"; do
		case "${deplist[@]}" in *"$dep"*) continue;; esac
		if [ -e "$MINGWROOT/bin/$dep" ]; then
			deplist=("${deplist[@]}" "$dep")
			finddeps "$MINGWROOT/bin/$dep"
		fi
	done
}

function installdeps {
	local deplist
	finddeps "$1"
	for dep in "${deplist[@]}"; do
		linkdep bin/$dep
	done
}

installdeps root/bin/gimagereader.exe

if [ $withdebug ]; then
	linkdep bin/gdb.exe
	installdeps root/bin/gdb.exe
fi

linkdep bin/twaindsm.dll
installdeps bin/twaindsm.dll

linkdep lib/enchant/libenchant_myspell.dll
installdeps root/lib/enchant/libenchant_myspell.dll

linkdep lib/pango/1.8.0/modules/pango-arabic-lang.dll
installdeps root/lib/pango/1.8.0/modules/pango-arabic-lang.dll

linkdep lib/pango/1.8.0/modules/pango-indic-lang.dll
installdeps root/lib/pango/1.8.0/modules/pango-indic-lang.dll

linkdep lib/pango/1.8.0/modules/pango-basic-fc.dll
installdeps root/lib/pango/1.8.0/modules/pango-basic-fc.dll

cpdep share/glib-2.0/schemas/org.gtk.Settings.FileChooser.gschema.xml
cpdep share/locale

# Add english language data and spelling dictionaries
stat /usr/share/tesseract/tessdata/eng.traineddata > /dev/null
ln -sf /usr/share/tesseract/tessdata/eng.traineddata root/share/tessdata/eng.traineddata
stat /usr/share/myspell/en_US.dic > /dev/null
ln -sf /usr/share/myspell/en_US.dic root/share/myspell/dicts/en_US.dic
stat /usr/share/myspell/en_US.aff > /dev/null
ln -sf /usr/share/myspell/en_US.aff root/share/myspell/dicts/en_US.aff

# Remove unused files
find root/share/locale/ -type f -not -name "gtk*.mo" -not -name "glib*.mo" -not -name "gdk*.mo" -not -name "atk*.mo" -not -name "gimagereader.mo" -exec rm -f {} \;
rm -rf root/share/applications

# Compile schemas
glib-compile-schemas root/share/glib-2.0/schemas

# Build the installer
progName=$(cat ../config.h | grep PACKAGE_NAME | awk -F'"' '{print $2}')
progVersion=$(cat ../config.h | grep PACKAGE_VERSION | awk -F'"' '{print $2}')
makensis -DNAME=$progName -DPROGVERSION="$progVersion" installer.nsi;
