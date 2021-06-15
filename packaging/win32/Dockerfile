FROM fedora:rawhide

MAINTAINER Sandro Mani <manisandro@gmail.com>
########## SYSTEM SETUP ##########
RUN dnf -y update; dnf clean all
# fundamental packages 
RUN dnf -y install file gcc make man sudo tar; dnf clean all


########## MINGW & BUILD DEPENDENCIES ##########
USER root
RUN dnf -y install mingw64-gcc
RUN dnf -y install mingw64-freetype freetype freetype-devel
RUN dnf -y install mingw64-cairo mingw64-cairo-static cairo cairo-devel
RUN dnf -y install mingw64-harfbuzz harfbuzz harfbuzz-devel
RUN dnf -y install mingw64-pango pango pango-devel
RUN dnf -y install mingw64-poppler poppler poppler-devel
RUN dnf -y install mingw64-gtk3 gtk3 gtk3-devel
RUN dnf -y install mingw64-glib2-static glib2 glib2-devel
RUN dnf -y install atk atk-devel 
RUN dnf -y install mingw64-winpthreads mingw64-winpthreads-static


# gImageReader
RUN \
echo all > /etc/rpm/macros.image-language-conf && \
dnf install -y \
  cmake \
  poppler-data \
  make \
  iso-codes \
  findutils \
  gettext \
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
  mingw32-poppler-qt5 \
  mingw32-qt5-qtbase \
  mingw32-qt5-qtimageformats \
  mingw32-qt5-qttools \
  mingw32-qt5-qttranslations \
  mingw32-qtspell-qt5 \
  mingw32-tesseract \
  mingw32-twaindsm \
  mingw32-quazip-qt5 \
  \
  mingw64-gdb \
  mingw64-gcc-c++ \
  mingw64-dlfcn \
  mingw64-djvulibre \
  mingw64-libgomp \
  mingw64-libjpeg-turbo \
  mingw64-podofo \
  mingw64-poppler-qt5 \
  mingw64-qt5-qtbase \
  mingw64-qt5-qtimageformats \
  mingw64-qt5-qttools \
  mingw64-qt5-qttranslations \
  mingw64-qtspell-qt5 \
  mingw64-tesseract \
  mingw64-twaindsm \
  mingw64-quazip-qt5 \
  \
  mingw64-libzip \
  \
  mingw64-gtkmm30 \
  mingw64-gtkspell3 \
  mingw64-gtkspellmm30 \
  mingw64-gtksourceviewmm3 \
  mingw64-cairomm \
  mingw64-poppler \
  mingw64-poppler-glib \
  mingw64-json-glib \
  mingw64-libxml++


WORKDIR /workspace
VOLUME ["/workspace"]