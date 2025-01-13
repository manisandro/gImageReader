#!/bin/sh

echo all > /etc/rpm/macros.image-language-conf
dnf install -y \
  cmake \
  poppler-data \
  make \
  iso-codes \
  findutils \
  gettext \
  git \
  intltool \
  hunspell-en \
  tesseract \
  mingw32-nsis \
  zip \
  python3-gobject \
  \
  mingw32-gdb \
  mingw32-gcc-c++ \
  mingw32-dlfcn \
  mingw32-djvulibre \
  mingw32-libgomp \
  mingw32-libjpeg-turbo \
  mingw32-podofo \
  mingw32-poppler-qt6 \
  mingw32-qt6-qtbase \
  mingw32-qt6-qtimageformats \
  mingw32-qt6-qttools \
  mingw32-qt6-qttranslations \
  mingw32-qtspell-qt6 \
  mingw32-tesseract \
  mingw32-twaindsm \
  mingw32-quazip-qt6 \
  \
  mingw32-libzip \
  mingw32-gtk3 gtk3 gtk3-devel \
  mingw32-glib2-static glib2 glib2-devel \
  mingw32-gtkmm30 \
  mingw32-gtkspell3 \
  mingw32-gtkspellmm30 \
  mingw32-gtksourceviewmm3 \
  mingw32-cairomm \
  mingw32-poppler \
  mingw32-poppler-glib \
  mingw32-json-glib \
  mingw32-libxml++ \
  \
  mingw64-gdb \
  mingw64-gcc-c++ \
  mingw64-dlfcn \
  mingw64-djvulibre \
  mingw64-libgomp \
  mingw64-libjpeg-turbo \
  mingw64-podofo \
  mingw64-poppler-qt6 \
  mingw64-qt6-qtbase \
  mingw64-qt6-qtimageformats \
  mingw64-qt6-qttools \
  mingw64-qt6-qttranslations \
  mingw64-qtspell-qt6 \
  mingw64-tesseract \
  mingw64-twaindsm \
  mingw64-quazip-qt6 \
  \
  mingw64-libzip \
  mingw64-gtk3 gtk3 gtk3-devel \
  mingw64-glib2-static glib2 glib2-devel \
  mingw64-gtkmm30 \
  mingw64-gtkspell3 \
  mingw64-gtkspellmm30 \
  mingw64-gtksourceviewmm3 \
  mingw64-cairomm \
  mingw64-poppler \
  mingw64-poppler-glib \
  mingw64-json-glib \
  mingw64-libxml++
