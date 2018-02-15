#!/bin/sh

cd "$(dirname "$(readlink -f "$0")")"

# Generate POTFILES.in
echo "Updating POTFILES.in..."
echo "# List of source files containing translatable strings." > po/POTFILES.in
echo "" >> po/POTFILES.in
echo "data/gimagereader.desktop.in" >> po/POTFILES.in
find gtk/data/ -type f -name *.ui -printf '[type: gettext/glade]%p\n' | sort >> po/POTFILES.in
find gtk/src/ -type f \( -name *.hh -or -name *.cc \) -print | sort >> po/POTFILES.in
find qt/data/ -type f -name *.ui -printf '[type: gettext/qtdesigner]%p\n' | sort >> po/POTFILES.in
find qt/src/ -type f \( -name *.hh -or -name *.cc \) -print | sort >> po/POTFILES.in

# Update POT and PO files
( 
cd po
echo "Updating gimagereader.pot..."
intltool-update --gettext-package=gimagereader --pot

while read lang; do
    echo "Updating $lang.po..."
    intltool-update --gettext-package=gimagereader $lang
done < LINGUAS

)
