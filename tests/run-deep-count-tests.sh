#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TEST_DIR="$(mktemp -d "${TMPDIR:-/tmp}/macmatecaja-deep-count.XXXXXX")"
trap 'rmdir "$TEST_DIR" 2>/dev/null || true' EXIT
cc -O2 -Wall -Wextra $(pkg-config --cflags gio-2.0) \
  "$ROOT/tests/test-deep-count-tracker.c" $(pkg-config --libs gio-2.0) \
  -o "$TEST_DIR/test-deep-count"
"$TEST_DIR/test-deep-count" --verbose
unlink "$TEST_DIR/test-deep-count"
