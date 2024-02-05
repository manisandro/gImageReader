#!/bin/sh
dir=$(dirname "$(readlink -f "$0")")
which astyle &> /dev/null || ( echo "Please install astyle." && exit 1 )

out=$(astyle --formatted --options=$dir/astyle.options $(find gtk qt common \( -name '*.cc' -or -name '*.hh' \)))
echo "$out"
[ -z "$out" ] && exit 0 || exit 1
