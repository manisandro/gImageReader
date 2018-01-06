FROM fedora:rawhide

MAINTAINER Sandro Mani <manisandro@gmail.com>

RUN \
echo all > /etc/rpm/macros.image-language-conf && \
dnf install -y 'dnf-command(config-manager)' && \
dnf config-manager --add-repo https://copr.fedorainfracloud.org/coprs/smani/mingw-extras/repo/fedora-rawhide/smani-mingw-extras-fedora-rawhide.repo && \
dnf install -y \
  cmake \
  iso-codes \
  gettext \
  hunspell-en \
  tesseract \
  mingw32-nsis \
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
  mingw32-libzip \
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
  mingw64-libzip

WORKDIR /workspace
VOLUME ["/workspace"]
