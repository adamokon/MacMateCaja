#!/bin/bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
build="$(mktemp -d "${TMPDIR:-/tmp}/macmatecaja-config-build.XXXXXX")"
trap '/bin/rm -f "$build/test-config"; /bin/rmdir "$build"' EXIT
${CC:-cc} -std=c99 -Wall -Wextra -Werror \
  -I"$root/source/caja/src" $(pkg-config --cflags gio-2.0) \
  "$root/tests/test-config.c" "$root/source/caja/src/macmatecaja-config.c" \
  $(pkg-config --libs gio-2.0) -o "$build/test-config"
"$build/test-config"
