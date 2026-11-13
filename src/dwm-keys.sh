#!/bin/sh
# Generate dwm-keys(1) from the key and button definitions in config.h.
#
# The configuration is preprocessed so helper macros such as TAGKEYS and SHCMD
# are expanded, but X keysym names are left intact for readable output.  Action
# descriptions are taken from the @brief comments in the C sources.

set -eu

srcdir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
config=${1:-"$srcdir/config.h"}
cc=${CC:-cc}
version=${VERSION:-}

if [ ! -f "$config" ] && [ -f "$srcdir/config.def.h" ]; then
  config="$srcdir/config.def.h"
fi

if [ -z "$version" ]; then
  version=$(sed -n 's/^VERSION[[:space:]]*=[[:space:]]*//p' "$srcdir/config.mk" | sed -n '1p')
fi

tmp=$(mktemp "${TMPDIR:-/tmp}/dwm-keys.XXXXXX")
trap 'rm -f "$tmp"' EXIT HUP INT TERM

# Drop includes: the keysym constants must stay symbolic, and the arrays are
# still valid C after the project macros have been expanded.
sed '/^[[:space:]]*#[[:space:]]*include/d' "$config" \
  | $cc -E -C -nostdinc -x c - \
  | sed '/^#[[:space:]]*[0-9]/d' > "$tmp"

{
  printf '%s\n' '###CONFIG###'
  cat "$tmp"
  for f in "$srcdir"/*.c; do
    printf '###SOURCE %s###\n' "${f##*/}"
    cat "$f"
  done
} | awk -v version="$version" -f "$srcdir/dwm-keys.awk"
