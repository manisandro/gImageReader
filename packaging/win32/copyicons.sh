#!/bin/sh

cd "$(dirname "$(readlink -f "$0")")"

if [ $# -lt 1 ]; then
	echo "Usage: $0 sourcethemedir"
	exit 1
fi

topdir="$PWD"
srcdir="$1"

icons="\
$(grep -o "property name=\"icon_name\">[^<]*<" ../../gtk/data/*.ui | awk -F'[<>]' '{print $2}')
$(grep -o "from_icon_name\s*(\s*\"[^\*]*\"" ../../gtk/src/{*.cc,*.hh} | awk -F'"' '{print $2}')
$(grep -o "<iconset theme=\"[^\"]*\"" ../../qt/data/*.ui | awk -F'"' '{print $2}')
$(grep -o "QIcon::fromTheme\s*(\s*\"[^\"]*\"" ../../qt/src/{*.cc,*.hh} | awk -F'"' '{print $2}')"

(
cd $srcdir
for icon in $(echo "$icons" | sort | uniq); do
	found=0
	for size in 16x16 22x22; do
		for srcicon in $(find $size -type f -name $icon.png -print); do
			found=1
			install -Dpm0644 "$srcicon" "$topdir/skel/share/icons/hicolor/$srcicon"
		done
	done
	if [ $found -eq 0 ]; then
		echo "Warning: missing icon $icon"
	fi
done
)