#!/bin/sh
which astyle &> /dev/null || echo "Please install astyle." && exit 1
find gtk qt \( -name '*.cc' -or -name '*.hh' \) -exec astyle --options=$dir/astyle.options {} \;
