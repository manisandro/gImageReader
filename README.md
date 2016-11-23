gImageReader
============

gImageReader is a simple Gtk/Qt front-end to [tesseract-ocr](https://github.com/tesseract-ocr/tesseract).

![Logo](https://raw.githubusercontent.com/manisandro/gImageReader/gh-pages/gimagereader.jpg)

Features:
------------
 - Import PDF documents and images from disk, scanning devices, clipboard and screenshots
 - Process multiple images and documents in one go
 - Manual or automatic recognition area definition
 - Recognize to plain text or to hOCR documents
 - Recognized text displayed directly next to the image
 - Post-process the recognized text, including spellchecking
 - Generate PDF documents from hOCR documents

Installation:
---------------
- ![Source](https://raw.githubusercontent.com/manisandro/gImageReader/gh-pages/icons/source.png) **Source**: Download from the [releases page](https://github.com/manisandro/gImageReader/releases)
- ![Windows](https://raw.githubusercontent.com/manisandro/gImageReader/gh-pages/icons/windows.png) **Windows**: Download from the [releases page](https://github.com/manisandro/gImageReader/releases)
- ![Fedora](https://raw.githubusercontent.com/manisandro/gImageReader/gh-pages/icons/fedora.png) **Fedora**: Available from the [official repositories](https://apps.fedoraproject.org/packages/gimagereader)
- ![Debian](https://raw.githubusercontent.com/manisandro/gImageReader/gh-pages/icons/debian.png) **Debian**: Available from the [official repositories](https://packages.debian.org/unstable/main/gimagereader)
- ![Ubuntu](https://raw.githubusercontent.com/manisandro/gImageReader/gh-pages/icons/ubuntu.png) **Ubuntu**: Available from [ppa:sandromani/gimagereader](https://launchpad.net/~sandromani/+archive/ubuntu/gimagereader)
- ![OpenSUSE](https://raw.githubusercontent.com/manisandro/gImageReader/gh-pages/icons/opensuse.png) **OpenSUSE**: Available from [OpenSUSE Build Service](https://build.opensuse.org/project/show/home:sandromani)
- ![ArchLinux](https://raw.githubusercontent.com/manisandro/gImageReader/gh-pages/icons/arch.png) **ArchLinux**: Available from [AUR](https://aur.archlinux.org/packages/gimagereader/)

Compilation:
--------------
The steps for compiling gImageReader from source are documented in the [wiki](https://github.com/manisandro/gImageReader/wiki/Compiling-gImageReader).

Support:
----------
If you encounter issues, please file a ticket in the [issue tracker](https://github.com/manisandro/gImageReader/issues), or feel free to mail me directly at `manisandro(at)gmail(dot)com`. Be sure to also consult the [FAQ](https://github.com/manisandro/gImageReader/wiki/FAQ).

Contributing:
---------------
Contributions are always welcome, ideally in the form of pull-requests.

Especially welcome are translations. These can be created as follows:
  1. Copy `po/gimagereader.pot` file to `po/<language>.po` (i.e. `po/de.po`)
  2. Translate the strings in `po/<language>.po`
  3. Add the language to `po/LINGUAS`
