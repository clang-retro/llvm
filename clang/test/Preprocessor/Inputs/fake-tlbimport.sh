#!/bin/sh
# Stands in for tlbimport in microsoft-import-typelib.c: writes NAME.tlh,
# holding the arguments it was given, into the -o directory and prints its
# path. "fail" in the arguments makes it fail; "warn" makes it warn.
out=.
args="$*"
for last; do :; done
case " $args " in
*" fail "*) echo "tlbimport: cannot read the type library" >&2; exit 1 ;;
*" warn "*) echo "tlbimport: warning: unknown #import attribute warn ignored" >&2 ;;
esac
while [ $# -gt 0 ]; do
  case $1 in -o) out=$2; shift ;; esac
  shift
done
base=$(basename "$last")
base=${base%%.*}
printf 'IMPORTED(%s)\n' "$args" > "$out/$base.tlh"
echo "$out/$base.tlh"
