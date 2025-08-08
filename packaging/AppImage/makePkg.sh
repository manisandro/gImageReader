#!/bin/bash
# makePkg.sh: Copyright 2025 Valerio Messina efa@iol.it
# makePkg.sh is part of gImageReader
# gImageReader is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# gImageReader is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with gImageReader. If not, see <http://www.gnu.org/licenses/>.
#
# Script to generate a Linux|Mingw|MXE|OSX package of 'gImageReader'
# used on: Linux=>bin64, Linux=>bin32, Linux=>macOS64
#          MinGw64=>bin64, MinGw32=>bin32, MXE64=>bin64, MXE32=>bin32
#
# Syntax: $ makePkg.sh LIN32|LIN64|MGW32|MGW64|MXE32|MXE64|OSX64

makePkgVer=2025-03-07

DEPSPATHMGW64="/mingw64/bin" # path of DLLs needed to generate the Mingw64 package
DEPSPATHMGW32="/mingw32/bin" # path of DLLs needed to generate the Mingw32 package
DEPSLISTMGW=""

APP="gImageReader"     # app name
SRCPKG="../packaging/AppImage" # source package path
BIN="gimagereader-gtk" # binary
DSTPATH="packaging" # path where create the Linux|Mingw/MXE|OSX package directory

echo "makePkg.sh: create a Linux|MinGW|MXE|OSX package for $APP ..."

# check for external dependency compliance
flag=0
for extCmd in 7z cp cut date grep mkdir mv pwd rm tar uname ; do
   exist=`which $extCmd 2> /dev/null`
   if (test "" = "$exist") then
      echo "Required external dependency: "\"$extCmd\"" unsatisfied!"
      flag=1
   fi
done
if [[ "$flag" = 1 ]]; then
   echo "ERROR: Install the required packages and retry. Exit"
   exit
fi

if [[ "$1" = "" || "$1" != "LIN32" && "$1" != "LIN64" && "$1" != "MGW32" && "$1" != "MGW64" && "$1" != "MXE32" && "$1" != "MXE64" && "$1" != "OSX64" ]]; then
   echo "ERROR: miss/unsupported package type"
   echo "Syntax: $ makePkg.sh LIN32|LIN64|MGW32|MGW64|MXE32|MXE64|OSX64"
   exit
fi

exist=`which osxcross-dmg 2> /dev/null`
if (test "$1" = "OSX64" && test "" = "$exist") then
   echo "ERROR: makePkg.sh depend on 'osxcross-dmg' to generate for macOS. Exit"
   exit
fi

if [[ -f $BIN ]]; then
   mkdir -p $DSTPATH
else
   echo "ERROR: makePkg.sh cannot find just built binary './$BIN'. Exit"
   exit
fi

PKG="$1"
CPU=`uname -m` # i686 or x86_64
BIT=64
if [[ "$PKG" = "LIN32" || "$PKG" = "MGW32" || "$PKG" = "MXE32" ]]; then
   BIT=32
fi
if [[ "$BIT" = "32" && "$CPU" = "x86_64" ]]; then
   CPU=i686
fi

OS=`uname -o`  # Msys or GNU/Linux
#VER=`grep VER version.h | cut -d'"' -f2` # M.mm
VER="3.4.2"
DATE=`date -I`
BINPATH=`pwd`
TGT=WinMxe
if [[ "$OS" = "Msys" ]]; then
   TGT=WinMgw
   if [[ "$BIT" = "64" ]]; then
      DEPSRC=$DEPSPATHMGW64
   fi
   if [[ "$BIT" = "32" ]]; then
      DEPSRC=$DEPSPATHMGW32
   fi
fi
UPKGPAT="" # Uncompressed Package Path
if [[ "$PKG" = "LIN32" || "$PKG" = "LIN64" ]]; then
   TGT=Linux
   UPKGPAT="usr/bin"
   DSTSP="usr/src"
fi
if [[ "$PKG" = "OSX64" ]]; then
   TGT=Osx
   UPKGPAT="DiskImage/$APP.app/Contents/MacOS"
   DSTSP="$UPKGPAT"
fi
DSTPKG="${APP}_${VER}_${DATE}_${TGT}_${CPU}_${BIT}bit"

if [[ "$OS" != "Msys" && "$OS" != "GNU/Linux" ]]; then
   echo "ERROR: work in MinGW|MXE(Linux) only"
   exit
fi
if [[ "$OS" = "Msys" && "$PKG" != "MGW32" && "$PKG" != "MGW64" ]]; then
   echo "ERROR makePkg.sh: Unsupported target package:$PKG on MinGW/MSYS2"
   exit
fi

if [[ "$OS" = "GNU/Linux" && "$PKG" != "LIN32" && "$PKG" != "LIN64" && "$PKG" != "MXE32" && "$PKG" != "MXE64" && "$PKG" != "OSX64" ]]; then
   echo "ERROR makePkg.sh: Unsupported target package:$PKG on Linux"
   exit
fi

echo "PKG    : $PKG"     # input par
echo "TGT    : $TGT"     # req target OS
echo "TGTBIT : $BIT"     # req target BIT
echo "TGTCPU : $CPU"     # target CPU
echo "VER    : $VER"
echo "DATE   : $DATE"
echo "HOSTOS : $OS"      # current OS
echo "APP    : $APP"     # app name
echo "SRCPKG : $SRCPKG"  # packaging file source path
echo "BINPATH: $BINPATH" # source binary path
echo "BIN    : $BIN"     # binary file
echo "SRCDEP : $DEPSRC"  # dependancy path (MinGw)
echo "DSTPATH: $DSTPATH" # package creation path
echo "DSTSP  : $DSTSP"   # app sources destination path
echo "UPKGPAT: $UPKGPAT" # package uncompressed creation path
echo "DSTPKG : $DSTPKG"  # package name
read -p "Proceed? A key to continue"
echo ""

echo "Creating $APP $VER package for $TGT$BIT.$CPU ..."
if [[ -d "$DSTPATH/AppDir" ]]; then
   echo "Removing old AppDir ..."
   rm -rf "$DSTPATH/AppDir"
fi
if [[ -f "$DSTPATH/AppImage/$DSTPKG.AppImage" ]]; then
   echo "Removing old AppImage ..."
   rm -f "$DSTPATH/AppImage/$DSTPKG.AppImage"
fi
if [[ -d "$DSTPATH/AppImage/AppDir" ]]; then
   echo "Removing old AppImageDir ..."
   rm -rf "$DSTPATH/AppImage/AppDir"
fi
if [[ -d $DSTPATH/$DSTPKG ]]; then
   echo "Removing old DSTPKG ..."
   rm -rf "$DSTPATH/$DSTPKG"
fi
mkdir -p "$DSTPATH/AppDir/$UPKGPAT"

echo "Copying sources ..."
#cp -a $SRCPATH/*.h $SRCPATH/*.c $DSTPATH/AppDir/$UPKGPAT
#cp -a $SRCPATH/Makefile* $DSTPATH/AppDir/$UPKGPAT
echo "Copying assets ..."
cp -a $SRCPKG/gImageReader.png $DSTPATH/AppDir/$UPKGPAT/gImageReader.png
cp -a $SRCPKG/gImageReader.desktop $DSTPATH/AppDir/$UPKGPAT
echo "Copying docs ..."
#cp -a $SRCPKG/Changelog.txt $DSTPATH/AppDir/$UPKGPAT
#cp -a $SRCPKG/COPYING.txt $DSTPATH/AppDir/$UPKGPAT
#cp -a $SRCPKG/README.txt $DSTPATH/AppDir/$UPKGPAT
#cp -a $SRCPKG/gImageReader_package.txt $DSTPATH/AppDir/$UPKGPAT
#cp -a $SRCPKG/gImageReader_config.txt  $DSTPATH/AppDir/$UPKGPAT
echo "Copying package ..."
cp -a $SRCPKG/makePkg.sh $DSTPATH/AppDir/$UPKGPAT
echo "Copying binary ..."
if [[ "$PKG" = "LIN64" ]]; then
   cp -a $BINPATH/gimagereader-gtk $DSTPATH/AppDir/$UPKGPAT
fi
if [[ "$PKG" = "LIN32" ]]; then
   cp -a $BINPATH/gimagereader-gtk_32 $DSTPATH/AppDir/$UPKGPAT
fi
if [[ "$PKG" = "MGW64" ]]; then
   cp -a $BINPATH/gimagereader-gtk_mgw.exe $DSTPATH/AppDir/$UPKGPAT
   echo "Copying deps ..."
   for DLL in $DEPSLISTMGW ; do
      cp -a $DEPSPATHMGW64/$DLL $DSTPATH/AppDir/$UPKGPAT
   done
fi
if [[ "$PKG" = "MGW32" ]]; then
   cp -a $BINPATH/gimagereader-gtk_mgw32.exe $DSTPATH/AppDir/$UPKGPAT
   echo "Copying deps ..."
   for DLL in $DEPSLISTMGW ; do
      if [[ "$DLL" = "libgcc_s_seh-1.dll" ]]; then continue ; fi
      cp -a $DEPSPATHMGW32/$DLL $DSTPATH/AppDir/$UPKGPAT
   done
   cp -a $DEPSPATHMGW32/libgcc_s_dw2-1.dll $DSTPATH/AppDir/$UPKGPAT
fi
if [[ "$PKG" = "MXE64" ]]; then
   cp -a $BINPATH/gimagereader-gtk_mxe.exe $DSTPATH/AppDir/$UPKGPAT
fi
if [[ "$PKG" = "MXE32" ]]; then
   cp -a $BINPATH/gimagereader-gtk_mxe32.exe $DSTPATH/AppDir/$UPKGPAT
fi
if [[ "$PKG" = "OSX64" ]]; then
   cp -a $BINPATH/gimagereader-gtk_osx $DSTPATH/AppDir/$UPKGPAT
   cp -a $SRCPKG/gImageReader.icns $DSTPATH/AppDir/$UPKGPAT
fi

# remove unwanted files if any
#rm "$DSTPATH/AppDir/$UPKGPAT/*.tzx" 2> /dev/null

# make AppImage
if [[ ("$PKG" = "LIN64" || "$PKG" = "LIN32") && ("$CPU" = "x86_64" || "$CPU" = "i686")]]; then
   echo "Generating the AppImage (about 40\") ..."
   mkdir -p "$DSTPATH/AppImage/AppDir/$UPKGPAT"
   cd $DSTPATH/AppImage
   if (test -f logWget$date.txt) then { rm logWget$date.txt ; } fi
   if (test "$BIT" = "64") then
      if (! test -x linuxdeploy-x86_64.AppImage) then
         echo "Downloading linuxdeploy ..."
         #wget -nv "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" 2>> logWget$date.txt
         wget -nv "https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20231026-1/linuxdeploy-x86_64.AppImage" 2>> logWget$date.txt
         chmod +x linuxdeploy-x86_64.AppImage
      fi
      if (! test -x linuxdeploy-plugin-gtk.sh) then
         wget -nv "https://raw.githubusercontent.com/linuxdeploy/linuxdeploy-plugin-gtk/master/linuxdeploy-plugin-gtk.sh" 2>> logWget$date.txt
         chmod +x linuxdeploy-plugin-gtk.sh
      fi
      #echo "linuxdeploy-x86_64.AppImage -e $BINPATH/gimagereader-gtk --appdir AppDir -p gtk -i $BINPATH/$SRCPKG/gImageReader.png -d $BINPATH/$SRCPKG/gImageReader.desktop --output appimage"
      linuxdeploy-x86_64.AppImage -e $BINPATH/gimagereader-gtk --appdir AppDir -p gtk -i $BINPATH/$SRCPKG/gImageReader.png -d $BINPATH/$SRCPKG/gImageReader.desktop > logLinuxdeploy$date.txt
      cp -a $BINPATH/glib-2.0/schemas/org.gnome.gimagereader.gschema.xml $BINPATH/$DSTPATH/AppImage/AppDir/usr/share/glib-2.0/schemas/
      mv $BINPATH/$DSTPATH/AppImage/AppDir/usr/share/glib-2.0/schemas/gschemas.compiled $BINPATH/$DSTPATH/AppImage/AppDir/usr/share/glib-2.0/schemas/gschemas.compiled.orig
      cp -a $BINPATH/glib-2.0/schemas/gschemas.compiled $BINPATH/$DSTPATH/AppImage/AppDir/usr/share/glib-2.0/schemas/
      #linuxdeploy-x86_64.AppImage -e $BINPATH/gimagereader-gtk --appdir AppDir -p gtk -i $BINPATH/$SRCPKG/gImageReader.png -d $BINPATH/$SRCPKG/gImageReader.desktop --output appimage >> logLinuxdeploy$date.txt
      echo "Generating the AppImage (about 20\") ..."
      linuxdeploy-x86_64.AppImage --appdir AppDir --output appimage >> logLinuxdeploy$date.txt
      file=$DSTPKG.AppImage
      mv ${APP}-x86_64.AppImage $BINPATH/$DSTPATH/$file
      echo "AppImage created: $DSTPATH/$file"
   fi
   if (test "$BIT" = "32") then
      echo "As now skip AppImage at 32 bit"
   fi
   rm -rf AppDir
   cd $BINPATH
   #rm -$DSTPATH/AppImage
   # echo "Note: maybe need:"
   # echo "export XDG_DATA_DIRS=/usr/local/share:/usr/share:./gImageReader/build"
fi

echo "Compressing package ..."
if [[ "$PKG" = "OSX64" ]]; then
   cp -a $BINPATH/gimagereader-gtk_osx $BINPATH/gImageReader.icns $BINPATH/$DSTPATH/AppDir
   cd $DSTPATH/AppDir
   osxcross-dmg -rw $BINPATH/gimagereader-gtk_osx $APP $VER
   rm uncompressed.dmg $BINPATH/gimagereader-gtk_osx gImageReader.icns
   #mv ${APP}$VER.dmg $BINPATH/$DSTPATH/${APP}$VER.dmg
   echo "Release file: '$BINPATH/$DSTPATH/${APP}$VER.dmg'"
   echo Done
   exit
fi
cd $BINPATH/$DSTPATH
mv AppDir $DSTPKG
if [[ -f $DSTPKG.tgz ]]; then
   rm $DSTPKG.tgz
fi
if [[ -f $DSTPKG.7z ]]; then
   rm $DSTPKG.7z
fi
if [[ "$PKG" = "LIN64" || "$PKG" = "LIN32" ]]; then
   #echo "tar cvaf $DSTPKG.tgz $DSTPKG"
   tar cvaf $DSTPKG.tgz $DSTPKG > /dev/null
   file=$DSTPKG.tgz
fi
if [[ "$OS" = "Msys" ]]; then
   #echo "7z a -mx=9 -r $DSTPKG.7z $DSTPKG"
   7z a -mx=9 -r $DSTPKG.7z $DSTPKG > /dev/null
   file=$DSTPKG.7z
fi
if [[ "$PKG" = "MXE64" || "$PKG" = "MXE32" ]]; then
   #echo "7z a -m0=lzma -mx=9 -r $DSTPKG.7z $DSTPKG"
   7z a -m0=lzma -mx=9 -r $DSTPKG.7z $DSTPKG > /dev/null
   file=$DSTPKG.7z
fi
rm -rf $DSTPKG
if [[ ! -f $BINPATH/$DSTPATH/$file ]]; then
   mv $file $BINPATH/$DSTPATH/
fi
echo "Release file: '$BINPATH/$DSTPATH/$file'"
echo Done
