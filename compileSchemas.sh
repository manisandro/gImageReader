#!/bin/bash

cd $(dirname $(readlink -f $0))

if [ $# -lt 1 ]; then
    echo "Usage: $0 builddir"
    exit 1
fi

builddir=$1

mkdir -p "$builddir/glib-2.0/schemas"
cp "gtk/data/org.gnome.gimagereader.gschema.xml" "$builddir/glib-2.0/schemas/"
glib-compile-schemas "$builddir/glib-2.0/schemas/"
