#!/bin/sh
dir=$(dirname "$(readlink -f "$0")")
which astyle &> /dev/null || echo "Please install astyle." && exit 1
find gtk qt \( -name '*.cc' -or -name '*.hh' \) -exec astyle --options=$dir/astyle.options {} \;
