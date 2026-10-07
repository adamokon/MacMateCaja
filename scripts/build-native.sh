#!/bin/bash
# Build from source; installs ONLY into this project's private runtime prefixes.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
case "$root" in *' '*) echo 'Use a build directory without spaces (autotools constraint).' >&2; exit 1;; esac
brew_prefix="${MACMATECAJA_HOMEBREW_PREFIX:-$(brew --prefix)}"
quartz="$root/prefix/quartz"
gvfs="$root/runtime/gvfs"
export PATH="$brew_prefix/bin:/usr/bin:/bin:/usr/sbin:/sbin"
export PKG_CONFIG_PATH="$gvfs/lib/pkgconfig:$quartz/lib/pkgconfig:$brew_prefix/lib/pkgconfig:$brew_prefix/share/pkgconfig"
jobs="${MACMATECAJA_BUILD_JOBS:-4}"

setup_meson() {
  local dir="$1" source="$2"
  shift 2
  if [ -f "$dir/build.ninja" ]; then
    meson setup --reconfigure "$dir" "$source" "$@"
  else
    meson setup "$dir" "$source" "$@"
  fi
  ninja -C "$dir" -j "$jobs"
  ninja -C "$dir" install
}

setup_meson "$root/build/dconf" "$root/source/dconf" --prefix="$gvfs" \
  -Dman=false -Dgtk_doc=false -Dvapi=false -Dbash_completion=false
setup_meson "$root/build/gvfs" "$root/source/gvfs" --prefix="$gvfs" --buildtype=debugoptimized \
  -Dadmin=false -Dafc=false -Dafp=false -Darchive=false -Dbluray=false \
  -Dcdda=false -Ddevel_utils=false -Ddnssd=false -Dfuse=false -Dgcr=false \
  -Dgcrypt=false -Dgoa=false -Dgoogle=false -Dgphoto2=false -Dgudev=false \
  -Dhttp=true -Dinstalled_tests=false -Dkeyring=false -Dlibusb=false \
  -Dlogind=false -Dman=false -Dmtp=false -Dnfs=true -Donedrive=false \
  -Dsftp=true -Dsmb=true -Dsystemduserunitdir=no -Dtmpfilesdir=no \
  -Dudisks2=false -Dunit_tests=false -Dwsdd=false
make -C "$root/source/mate-desktop-lite" -j "$jobs" install PREFIX="$quartz"
(
  cd "$root/source/caja"
  ./configure --prefix="$quartz" --disable-schemas-compile --disable-update-mimedb \
    --disable-icon-update --disable-gtk-doc --disable-introspection \
    --disable-packagekit --disable-xmp --disable-self-check --enable-quartz-browser-only
  make -j "$jobs"
  make install
)
glib-compile-schemas "$quartz/share/glib-2.0/schemas"
echo 'Built native runtime. Launch app/MacMateCaja.app; this is not a relocatable binary bundle.'
