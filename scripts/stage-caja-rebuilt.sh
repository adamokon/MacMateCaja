#!/bin/bash
# stage-caja-rebuilt.sh
# Staged installation of rebuilt Caja with neutral prefix /opt/MacMateCaja into copied prefix.
# Never touches real /opt. Preserves mate-desktop-lite runtime and schemas.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
WORKSPACE_ROOT="$PROJECT_ROOT"

CAJA_SRC="$PROJECT_ROOT/source/caja"
STAGED_DIR="$WORKSPACE_ROOT/build/staged_caja"
TARGET_PREFIX="$PROJECT_ROOT/prefix/quartz"

echo "=== Staging Rebuilt Caja ==="
if [ ! -f "$TARGET_PREFIX/lib/libmate-desktop-2.17.dylib" ] || [ ! -x "$PROJECT_ROOT/runtime/gvfs/libexec/gvfsd" ]; then
  echo "Build the native dependencies first: bash scripts/build-native.sh" >&2
  exit 1
fi

mkdir -p "$STAGED_DIR"
BREW_PREFIX="${MACMATECAJA_HOMEBREW_PREFIX:-$(brew --prefix)}"
export PATH="$BREW_PREFIX/bin:/usr/bin:/bin:/usr/sbin:/sbin"
export PKG_CONFIG_PATH="$PROJECT_ROOT/runtime/gvfs/lib/pkgconfig:$TARGET_PREFIX/lib/pkgconfig:$BREW_PREFIX/lib/pkgconfig:$BREW_PREFIX/share/pkgconfig"
(
  cd "$CAJA_SRC"
  ./configure --prefix=/opt/MacMateCaja --disable-schemas-compile --disable-update-mimedb \
    --disable-icon-update --disable-gtk-doc --disable-introspection --disable-packagekit \
    --disable-xmp --disable-self-check --enable-quartz-browser-only
  make -j"${MACMATECAJA_BUILD_JOBS:-4}"
)

echo "Running make install with DESTDIR=$STAGED_DIR..."
make -C "$CAJA_SRC" install DESTDIR="$STAGED_DIR"

STAGED_PREFIX="$STAGED_DIR/opt/MacMateCaja"
if [ ! -d "$STAGED_PREFIX" ]; then
  echo "Error: expected staged prefix at $STAGED_PREFIX not found." >&2
  exit 1
fi

echo "Overlaying staged files into $TARGET_PREFIX..."
# 1. Binaries
mkdir -p "$TARGET_PREFIX/bin"
for b in "$STAGED_PREFIX/bin"/*; do
  if [ -f "$b" ]; then
    cp -fp "$b" "$TARGET_PREFIX/bin/"
  fi
done

# 2. Caja libraries (preserving libmate-desktop)
mkdir -p "$TARGET_PREFIX/lib"
for l in "$STAGED_PREFIX/lib"/libcaja-extension*; do
  if [ -e "$l" ]; then
    cp -RPfp "$l" "$TARGET_PREFIX/lib/"
  fi
done

# 3. Share directories
mkdir -p "$TARGET_PREFIX/share"
if [ -d "$STAGED_PREFIX/share/caja" ]; then
  cp -RPfp "$STAGED_PREFIX/share/caja" "$TARGET_PREFIX/share/"
fi
if [ -d "$STAGED_PREFIX/share/pixmaps/caja" ]; then
  mkdir -p "$TARGET_PREFIX/share/pixmaps"
  cp -RPfp "$STAGED_PREFIX/share/pixmaps/caja" "$TARGET_PREFIX/share/pixmaps/"
fi
if [ -d "$STAGED_PREFIX/share/applications" ]; then
  mkdir -p "$TARGET_PREFIX/share/applications"
  cp -RPfp "$STAGED_PREFIX/share/applications"/* "$TARGET_PREFIX/share/applications/"
fi

echo "Staged overlay complete. Preserved mate-desktop runtime in $TARGET_PREFIX/lib."
