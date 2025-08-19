#!/bin/bash
# gImageReader v3.4.3 tesseract OCR front-end GUI
# makePkg.sh: Copyright 2005-2025 Valerio Messina efa@iol.it
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
# 7z Win   packages: $AppName/ (bin+assets) src/
# tar.gz   packages: $AppName/ (bin+assets) src/
# AppImage packages: AppDir/   usr/bin
# macOS    packages: DiskImage/$AppName.app/Contents/MacOS/
#
# Syntax: $ makePkg.sh [-y] Linux|WinMxe|WinMgw|Osx [32|64] [trail]

makePkgVer=2025-08-19

APP="gImageReader"     # app name
BINRAD="gimagereader" # binary radix
BINPATH=`pwd` # where find the just build binary
SRCPATH="../gtk/src" # source package path
RESPATH="../packaging/AppImage" # other resources (readme, icons, desktop files) to copy in packages
TMPPATH="../packaging" # temp path where create uncompressed dir,AppDir,DiskImage
DSTPATH="../packaging" # path where write the final Linux|Mingw/MXE|OSX package

DEPSPATHMGW64="/mingw64/bin" # path of DLLs needed to generate the Mingw64 package
DEPSPATHMGW32="/mingw32/bin" # path of DLLs needed to generate the Mingw32 package
DEPSLISTMGW="" # list of dll for MinGW cross-build dynamic link

echo "makePkg.sh: create a Linux|MinGW|MXE|OSX package for $APP ..."

# check for external dependency compliance
flag=0
for extCmd in 7z chmod cp cut date getconf grep mkdir mv pwd realpath rm tar uname wget ; do
   exist=`which $extCmd 2> /dev/null`
   if (test "" = "$exist") then
      echo "ERROR: makePkg.sh Required external dependency: "\"$extCmd\"" unsatisfied!"
      flag=1
   fi
done
if [[ "$flag" = 1 ]]; then
   echo "ERROR: makePkg.sh Install the required packages and retry. Exit"
   exit
fi

if (test "$1" = "-y") then
   batch=1
   shift
fi
if [[ "$1" = "" || "$1" != "Linux" && "$1" != "WinMxe" && "$1" != "WinMgw" && "$1" != "Osx" ]]; then
   echo "ERROR: makePkg.sh unsupported/miss target:'$1' platform to create package"
   echo "Syntax: $ makePkg.sh [-y] Linux|WinMxe|WinMgw|Osx [32|64] [trail]"
   echo "          -y for batch execution without confirmations"
   echo "          trail is the GUI binary trailer. eg. Gui, -gtk, -qt5, -qt6"
   exit
fi

#exist=`which gtk-mac-bundler 2> /dev/null`
#if (test "$1" = "Osx" && test "" = "$exist") then
#   echo "ERROR: makePkg.sh depend on 'gtk-mac-bundler' to generate for macOS. Exit"
#   exit
#fi
exist=`which osxcross-dmg 2> /dev/null`
if (test "$1" = "OSX64" && test "" = "$exist") then
   echo "ERROR: makePkg.sh depend on 'osxcross-dmg' to generate for macOS. Exit"
   exit
fi

PKG="$1"
CPU=`uname -m` # i686 or x86_64
if (test "" = "$2") then
   BIT=$(getconf LONG_BIT)
else # at least 2
   if [[ "$2" = "32" || "$2" = "64" ]]; then
      BIT="$2"
      if [[ "" != "$3" ]]; then
         TRAIL="$3"
      fi
   else
      TRAIL="$2"
   fi
fi
if [[ "$TRAIL" = "" ]]; then
   TRAIL="Gui"
fi

BINCLI=""
BINGUI="${BINRAD}${TRAIL}"
BINPATH=`realpath "$BINPATH"`
SRCPATH=`realpath "$SRCPATH"`
RESPATH=`realpath "$RESPATH"`
DSTPATH=`realpath "$DSTPATH"`
if [[ ! -d "$BINPATH" ]]; then
   echo "ERROR: makePkg.sh cannot find binary path '$BINPATH'. Exit"
   exit
fi
if [[ ! -d "$SRCPATH" ]]; then
   echo "ERROR: makePkg.sh cannot find source path '$SRCPATH'. Exit"
   exit
fi
if [[ ! -d "$RESPATH" ]]; then
   echo "ERROR: makePkg.sh cannot find resource path '$RESPATH'. Exit"
   exit
fi
if (test "$BINCLI" = "" && test "$BINGUI" = "") then
   echo "ERROR: makePkg.sh cannot be both NULL 'BINCLI' and 'BINGUI'. Exit"
   exit
fi
if (test "$BINCLI" != "") then
   if [[ ! -f "$BINPATH/$BINCLI" ]]; then
      echo "ERROR: makePkg.sh cannot find just built binary '$BINPATH/$BINCLI'. Exit"
      exit
   fi
fi
if (test "$BINGUI" != "") then
   if [[ ! -f "$BINPATH/$BINGUI" ]]; then
      echo "ERROR: makePkg.sh cannot find just built binary '$BINPATH/$BINGUI'. Exit"
      exit
   fi
fi

OS=`uname`
if (test "$OS" != "Darwin") then
   OS=`uname -o`  # Msys or GNU/Linux, illegal on macOS
fi
if [[ "$OS" != "Msys" && "$OS" != "GNU/Linux" ]]; then
   echo "ERROR makePkg.sh: work in Linux|MXE(Linux)|MinGW only"
   exit
fi
if [[ "$OS" = "Msys" && "$PKG" != "MGW32" && "$PKG" != "MGW64" ]]; then
   echo "ERROR makePkg.sh: Unsupported target package:$PKG on MinGW/MSYS2"
   exit
fi

if [[ "$OS" = "GNU/Linux" && "$PKG" != "Linux" && "$PKG" != "WinMxe" && "$PKG" != "Osx" ]]; then
   echo "ERROR makePkg.sh: Unsupported target package:$PKG on Linux"
   exit
fi

DATE=`date -I`
VER="3.4.3" # VER=`grep VER version.h | cut -d'"' -f2` # M.mm.bb
TGT=$PKG
if [[ "$OS" = "Msys" ]]; then
   if [[ "$BIT" = "64" ]]; then
      DEPPATH=$DEPSPATHMGW64
   fi
   if [[ "$BIT" = "32" ]]; then
      DEPPATH=$DEPSPATHMGW32
   fi
fi
TMPBINP="" # temp bin Package Path
if [[ "$PKG" = "Osx" ]]; then
   TMPBINP="DiskImage/$APP.app/Contents/MacOS"
fi
if [[ "$PKG" = "Linux" ]]; then
   TMPBINP="usr/bin"
fi
if (test "$CPU" = "x86_64" && test "$BIT" = "32") then
   CPU=i686
fi
if (test "$PKG" = "WinMxe" || test "$PKG" = "WinMgw") then
   EXT=".exe"
fi
PKGNAME="${APP}${TRAIL}_${VER}_${DATE}_${TGT}_${CPU}_${BIT}bit"
echo "DATE   : $DATE"    # today
echo "HOSTOS : $OS"      # current OS
echo "APP    : $APP"     # app name
echo "VER    : $VER"     # app src ver
echo "PKG    : $PKG"     # input parameter
echo "TGTOS  : $TGT"     # target OS
echo "TGTCPU : $CPU"     # target CPU
echo "TGTBIT : $BIT"     # target BIT
echo "BINRAD : $BINRAD"  # binary radix
if (test "$BINCLI" != "") then
echo "BINCLI : $BINCLI"  # binary file for CLI
fi
if (test "$BINGUI" != "") then
echo "BINGUI : $BINGUI"  # binary file for GUI
fi
if (test "$EXT" != "") then
echo "EXT    : $EXT"     # binary extension (Mxe|MinGw)
fi
echo "BINPATH: $BINPATH" # binary path
echo "SRCPATH: $SRCPATH"  # packaging file source path
echo "RESPATH: $RESPATH" # resources path
if (test "$DEPPATH" != "") then
echo "DEPPATH: $DEPPATH"  # dependancy path (MinGw)
fi
echo "TMPPATH: $TMPPATH" # temp path where create uncompressed dir,AppDir,DiskImage
echo "TMPBINP: $TMPBINP" # tmp bin path. eg. usr/bin | DiskImage/$APP.app/Contents/MacOS
echo "DSTPATH: $DSTPATH" # package creation path
echo "PKGNAME: $PKGNAME" # package name
if (test "$batch" != "1") then
   read -p "Proceed? A key to continue"
fi
echo ""

echo "makePkg: Creating $APP $VER package for $TGT$BIT.$CPU ..."
#cp -a $RESPATH/README.txt $RESPATH/README.md

echo "Creating temp and destination path if miss ..."
if [[ ! -d "$TMPPATH" ]]; then
   mkdir -p "$TMPPATH"
fi
if [[ ! -d "$DSTPATH" ]]; then
   mkdir -p "$DSTPATH"
fi

echo "Removing old temp files if present ..."
if (test "$TGT" = "Linux" || test "$TGT" = "WinMxe" || test "$TGT" = "WinMgw") then
   if [[ -d "$TMPPATH/$APP" ]]; then
      echo "Removing old $APP ..."
      rm -rf "$TMPPATH/$APP"
   fi
fi
if (test "$TGT" = "Linux") then
   if [[ -d "$TMPPATH/AppDir" ]]; then
      echo "Removing old AppDir ..."
      rm -rf "$TMPPATH/AppDir"
   fi
fi

echo "Removing old temp packages if present ..."
if (test "$TGT" = "Linux") then
   if [[ -f "$TMPPATH/$PKGNAME.tgz" ]]; then
      echo "Removing old tgz Package ..."
      rm -rf "$TMPPATH/$PKGNAME.tgz"
   fi
   if [[ -f "$TMPPATH/$PKGNAME.AppImage" ]]; then
      echo "Removing old AppImage ..."
      rm -f "$TMPPATH/$PKGNAME.AppImage"
   fi
fi
if (test "$TGT" = "WinMxe" || test "$TGT" = "WinMgw") then
   if [[ -f "$TMPPATH/$PKGNAME.7z" ]]; then
      echo "Removing old 7z Package ..."
      rm -rf "$TMPPATH/$PKGNAME.7z"
   fi
fi
if (test "$TGT" = "Osx") then
   if [[ -f "$TMPPATH/$APP.dmg" ]]; then
      echo "Removing old dmg Package ..."
      rm -rf "$TMPPATH/$APP.dmg"
   fi
fi

if [[ "$TGT" = "Osx" ]]; then
   if (test "$BINCLI" != "") then
      cp -a $BINPATH/$BINCLI $TMPPATH # bin
   fi
   if (test "$BINGUI" != "") then
      cp -a $BINPATH/$BINGUI $TMPPATH # bin
   fi
   cp -a $RESPATH/$APP.icns $TMPPATH # need to be near bin
   cd $TMPPATH
   #gtk-mac-bundler $APP.bundle
   osxcross-dmg -rw $BIN $APP $VER # create compressed MacOS package
   rm uncompressed.dmg $APP.icns # remove copied
   rm $BINCLI $BINGUI 2>/dev/null # remove copied
   mv $APP$VER.dmg $DSTPATH
   exit
fi

echo "Creating package directory structure ..."
mkdir -p $TMPPATH/$APP/src
if (test "$TGT" = "Linux") then
   mkdir -p "$TMPPATH/AppDir/$TMPBINP"
fi

echo "Copying sources ..."
cp -a $SRCPATH/*.h*       $TMPPATH/$APP/src/
cp -a $SRCPATH/*.c*       $TMPPATH/$APP/src/
#cp -a $SRCPATH/Makefile*  $TMPPATH/$APP/src/
#cp -a $SRCPATH/*.ini      $TMPPATH/$APP/src/   # if needed
#cp -aL $SRCPATH/$APP.png   $TMPPATH/$APP/src/   # if needed

echo "Copying assets ..."
#cp -a $RESPATH/*.ini      $TMPPATH/$APP/   # if needed
if (test "$TGT" = "Linux") then
#cp -a $RESPATH/*.ini   $TMPPATH/AppDir/$TMPBINP   # if needed
cp -a $RESPATH/$APP$TRAIL.desktop $TMPPATH/$APP/
cp -a $RESPATH/$APP$TRAIL.desktop $TMPPATH/AppDir/
cp -a $RESPATH/$APP.png     $TMPPATH/$APP/
cp -a $RESPATH/$APP.png     $TMPPATH/AppDir/
cp -a $RESPATH/makePkg.sh   $TMPPATH/$APP/
fi
if (test "$TGT" = "WinMxe" || test "$TGT" = "WinMgw") then
cp -a $RESPATH/$APP.ico     $TMPPATH/$APP/
fi

echo "Copying docs ..."
cp -a ../COPYING             $TMPPATH/$APP/
#cp -a $RESPATH/README.txt    $TMPPATH/$APP/   # if needed
#cp -a $RESPATH/Changelog.txt $TMPPATH/$APP/   # if needed
#cp -a $RESPATH/${APP}GUI.png $TMPPATH/$APP/   # if needed
if (test "$TGT" = "Linux") then
cp -a ../COPYING             $TMPPATH/AppDir/
#cp -a $RESPATH/README.txt    $TMPPATH/AppDir/  # if needed
#cp -a $RESPATH/Changelog.txt $TMPPATH/AppDir/  # if needed
fi

echo "Copying binary ..."
if [[ "$TGT" = "Linux" && "$BIT" = "64" ]]; then
   if (test "$BINCLI" != "") then
      cp -a $BINPATH/$BINCLI $TMPPATH/$APP/
      cp -a $BINPATH/$BINCLI $TMPPATH/AppDir/$TMPBINP/
   fi
   if (test "$BINGUI" != "") then
      cp -a $BINPATH/$BINGUI $TMPPATH/$APP/
      cp -a $BINPATH/$BINGUI $TMPPATH/AppDir/$TMPBINP/
   fi
fi
if [[ "$TGT" = "Linux" && "$BIT" = "32" ]]; then
   if (test "$BINCLI" != "") then
      cp -a $BINPATH/$BINCLI $TMPPATH/$APP/
      cp -a $BINPATH/$BINCLI $TMPPATH/AppDir/$TMPBINP/
   fi
   if (test "$BINGUI" != "") then
      cp -a $BINPATH/$BINGUI $TMPPATH/$APP/
      cp -a $BINPATH/$BINGUI $TMPPATH/AppDir/$TMPBINP/
   fi
fi

if [[ "$TGT" = "WinMgw" && "$BIT" = "64" ]]; then
   if (test "$BINCLI" != "") then
      cp -a $BINPATH/$BINCLI$EXT $TMPPATH/$APP/
   fi
   if (test "$BINGUI" != "") then
      cp -a $BINPATH/$BINGUI$EXT $TMPPATH/$APP/
   fi
   echo "Copying deps ..."
   for DLL in $DEPSLISTMGW ; do
      cp -a $DEPPATH/$DLL $TMPPATH/$APP/
   done
fi
if [[ "$TGT" = "WinMgw" && "$BIT" = "32" ]]; then
   if (test "$BINCLI" != "") then
      cp -a $BINPATH/$BINCLI$EXT $TMPPATH/$APP/
   fi
   if (test "$BINGUI" != "") then
      cp -a $BINPATH/$BINGUI$EXT $TMPPATH/$APP/
   fi
   echo "Copying deps ..."
   for DLL in $DEPSLISTMGW ; do
      if [[ "$DLL" = "libgcc_s_seh-1.dll" ]]; then continue ; fi
      cp -a $DEPPATH/$DLL $TMPPATH/$APP/
   done
   cp -a $DEPPATH/libgcc_s_dw2-1.dll $TMPPATH/$APP/
fi

if [[ "$TGT" = "WinMxe" && "$BIT" = "64" ]]; then
   if (test "$BINCLI" != "") then
      cp -a $BINPATH/$BINCLI$EXT $TMPPATH/$APP/
   fi
   if (test "$BINGUI" != "") then
      cp -a $BINPATH/$BINGUI$EXT $TMPPATH/$APP/
   fi
fi
if [[ "$TGT" = "WinMxe" && "$BIT" = "32" ]]; then
   if (test "$BINCLI" != "") then
      cp -a $BINPATH/$BINCLI$EXT $TMPPATH/$APP/
   fi
   if (test "$BINGUI" != "") then
      cp -a $BINPATH/$BINGUI$EXT $TMPPATH/$APP/
   fi
fi

# remove unwanted files if any

echo "Compressing package ..."
cwd=`pwd`
cd $TMPPATH
if (test "$TGT" = "Linux") then
   file="$PKGNAME.tgz"
   echo "Creating package file:'$file' ..."
   tar -cvaf $file $APP > /dev/null
   echo "Package file:'$file' done"
fi
if (test "$TGT" = "WinMxe" || test "$TGT" = "WinMgw") then
   file="$PKGNAME.7z"
   echo "Creating package file:'$file' ..."
   7z a -m0=lzma -mx=9 -r $file $APP > /dev/null
   echo "Package file:'$file' done"
fi
#rm -r AppDir
#rm -r $APP
mv $file $DSTPATH/
echo "Release file: '$DSTPATH/$file'"

# make AppImage
if [[ ("$TGT" = "Linux") && ("$CPU" = "x86_64" || "$CPU" = "i686")]]; then # skip on ARM & RISC-V
   if (test -f logWget$DATE.txt) then { rm logWget$DATE.txt ; } fi
   if (test "$BIT" = "64") then
      echo "Generating the AppDir (about 3'50\") ..."
      if (! test -x linuxdeploy-x86_64.AppImage) then
         echo "Downloading linuxdeploy ..."
         wget -nv "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" 2>> logWget$DATE.txt
         chmod +x linuxdeploy-x86_64.AppImage
      fi
      if (test "$TRAIL" = "-gtk") then
         if (! test -x linuxdeploy-plugin-gtk.sh) then
            echo "Downloading linuxdeploy GTK plugin ..."
            wget -nv "https://raw.githubusercontent.com/linuxdeploy/linuxdeploy-plugin-gtk/master/linuxdeploy-plugin-gtk.sh" 2>> logWget$DATE.txt
            chmod +x linuxdeploy-plugin-gtk.sh
         fi
        #./linuxdeploy-x86_64.AppImage -e $TMPPATH/AppDir/$TMPBINP/$BINGUI --appdir AppDir -p gtk -i $TMPPATH/AppDir/$APP.png -d $TMPPATH/AppDir/$APP$TRAIL.desktop --output appimage"
         ./linuxdeploy-x86_64.AppImage -e $TMPPATH/AppDir/$TMPBINP/$BINGUI --appdir AppDir -p gtk -i $TMPPATH/AppDir/$APP.png -d $TMPPATH/AppDir/$APP$TRAIL.desktop > logLinuxdeploy$DATE.txt
         cp -a $BINPATH/glib-2.0/schemas/org.gnome.gimagereader.gschema.xml $TMPPATH/AppDir/usr/share/glib-2.0/schemas/
         mv $TMPPATH/AppDir/usr/share/glib-2.0/schemas/gschemas.compiled    $TMPPATH/AppDir/usr/share/glib-2.0/schemas/gschemas.compiled.orig
         cp -a $BINPATH/glib-2.0/schemas/gschemas.compiled                  $TMPPATH/AppDir/usr/share/glib-2.0/schemas/
      fi
      if [[ "$TRAIL" = "-qt5" || "$TRAIL" = "-qt6" ]]; then
         if (! test -x linuxdeploy-plugin-qt-x86_64.AppImage) then
            echo "Downloading linuxdeploy QT plugin ..."
            wget -nv "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage" 2>> logWget$DATE.txt
            chmod +x linuxdeploy-plugin-qt-x86_64.AppImage
         fi
        #./linuxdeploy-x86_64.AppImage -e $TMPPATH/AppDir/$TMPBINP/$BINGUI --appdir AppDir -p qt -i $TMPPATH/AppDir/$APP.png -d $TMPPATH/AppDir/$APP$TRAIL.desktop --output appimage"
         ./linuxdeploy-x86_64.AppImage -e $TMPPATH/AppDir/$TMPBINP/$BINGUI --appdir AppDir -p qt -i $TMPPATH/AppDir/$APP.png -d $TMPPATH/AppDir/$APP$TRAIL.desktop > logLinuxdeploy$DATE.txt
         #cp -a $BINPATH/glib-2.0/schemas/org.gnome.gimagereader.gschema.xml $TMPPATH/AppDir/usr/share/glib-2.0/schemas/
         #mv $TMPPATH/AppDir/usr/share/glib-2.0/schemas/gschemas.compiled    $TMPPATH/AppDir/usr/share/glib-2.0/schemas/gschemas.compiled.orig
         #cp -a $BINPATH/glib-2.0/schemas/gschemas.compiled                  $TMPPATH/AppDir/usr/share/glib-2.0/schemas/
      fi
      echo "Generating the AppImage (about 3'10\") ..."
      ./linuxdeploy-x86_64.AppImage --appdir AppDir --output appimage >> logLinuxdeploy$DATE.txt
      ret=$?
      file="$PKGNAME.AppImage"
      if (test "$ret" = "0") then
         echo "AppImage created: $DSTPATH/$file"
         mv ${APP}${TRAIL}-x86_64.AppImage $DSTPATH/$file
      else
         echo "AppImage failed: $file"
      fi
   fi
   if (test "$BIT" = "32") then
      echo "As now skip AppImage at 32 bit"
   fi
   #rm -rf $TMPPATH/AppDir
   cd $cwd
fi
echo Done
