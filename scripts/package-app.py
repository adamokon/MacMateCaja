#!/usr/bin/env python3
"""
package-app.py - Self-contained Apple Silicon macOS application packager for MacMateCaja.

Creates a fully relocatable, ad-hoc codesigned MacMateCaja.app, an unsigned .pkg
component targeting /Applications, and a distribution ZIP in dist/.
Preserves GTK Quartz, GVfs/D-Bus, and ad-hoc signature integrity without external network.
"""

import argparse
import glob
import hashlib
import os
import plistlib
import re
import shutil
import stat
import subprocess
import sys
from pathlib import Path


def run_cmd(cmd, check=True, text=True):
    res = subprocess.run(cmd, capture_output=True, text=text)
    if check and res.returncode != 0:
        cmd_str = ' '.join(str(c) for c in cmd) if isinstance(cmd, list) else cmd
        raise RuntimeError(f"Command failed (exit {res.returncode}): {cmd_str}\nStdout:\n{res.stdout}\nStderr:\n{res.stderr}")
    return res


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()


def is_macho(path):
    if not os.path.isfile(path) or os.path.islink(path):
        return False
    try:
        with open(path, 'rb') as f:
            magic = f.read(4)
            return magic in (
                b'\xfe\xed\xfa\xce', b'\xce\xfa\xed\xfe',
                b'\xfe\xed\xfa\xcf', b'\xcf\xfa\xed\xfe',
                b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca'
            )
    except Exception:
        return False


def get_dependencies(path):
    res = run_cmd(['otool', '-L', str(path)], check=True)
    lines = res.stdout.strip().splitlines()
    deps = []
    # Skip first line (path itself)
    for line in lines[1:]:
        line = line.strip()
        if not line:
            continue
        dep_path = line.split(' (compatibility version')[0].strip()
        deps.append(dep_path)
    return deps


def get_rpaths(path):
    res = run_cmd(['otool', '-l', str(path)], check=True)
    rpaths = []
    lines = res.stdout.splitlines()
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        if line == 'cmd LC_RPATH':
            while i < len(lines) and not lines[i].strip().startswith('path '):
                i += 1
            if i < len(lines):
                p = lines[i].strip()[5:].split(' (offset')[0].strip()
                rpaths.append(p)
        i += 1
    return rpaths


def is_system_lib(dep):
    return (
        dep.startswith('/System/Library/') or
        dep.startswith('/usr/lib/')
    )


LAUNCHER_SCRIPT = """#!/bin/bash
set -euo pipefail

CONTENTS_DIR="$(cd "$(dirname "$0")/.." && pwd)"
MACMATECAJA_TEMP_ROOT="${TMPDIR:-/tmp}/MacMateCaja-${UID:-$(id -u)}"
MACMATECAJA_GVFS_CACHE="/tmp/MacMateCaja-gvfs-${UID:-$(id -u)}"
RESOURCE_SHARE="$CONTENTS_DIR/Resources/share"

umask 077
mkdir -p "$MACMATECAJA_TEMP_ROOT" "$MACMATECAJA_GVFS_CACHE"
for private_dir in "$MACMATECAJA_TEMP_ROOT" "$MACMATECAJA_GVFS_CACHE"; do
  if [ -L "$private_dir" ] || [ ! -O "$private_dir" ]; then
    echo "MacMateCaja: unsafe cache directory: $private_dir" >&2
    exit 1
  fi
  chmod 700 "$private_dir"
done

# Session-isolated directory for D-Bus and dynamic caches to prevent multi-instance race conditions
SESSION_DIR="$(mktemp -d "$MACMATECAJA_TEMP_ROOT/session.XXXXXX")"
cleanup_session() {
  /bin/rm -rf "$SESSION_DIR"
}
trap cleanup_session EXIT

# Safe escaping helper for sed replacement
sed_escape() {
  printf '%s' "$1" | /usr/bin/sed -e 's/[\\\\&|]/\\\\&/g'
}

xml_escape() {
  printf '%s' "$1" | /usr/bin/sed -e 's/&/\\&amp;/g' -e 's/</\\&lt;/g' -e 's/>/\\&gt;/g'
}

# Relocatable runtime environment
export PATH="$CONTENTS_DIR/MacOS:$CONTENTS_DIR/Resources/libexec:/usr/bin:/bin:/usr/sbin:/sbin"
export DYLD_FALLBACK_LIBRARY_PATH="$CONTENTS_DIR/Frameworks:${DYLD_FALLBACK_LIBRARY_PATH:-}"
export XDG_DATA_DIRS="$RESOURCE_SHARE:/usr/share"
export XDG_CONFIG_DIRS="$CONTENTS_DIR/Resources/etc/xdg:/etc/xdg"
export GSETTINGS_SCHEMA_DIR="$RESOURCE_SHARE/glib-2.0/schemas"
export GIO_EXTRA_MODULES="$CONTENTS_DIR/Resources/lib/gio/modules"
export GIO_USE_VFS="${GIO_USE_VFS:-gvfs}"
export GTK_THEME="${GTK_THEME:-Adwaita:dark}"
export GDK_BACKEND="quartz"
export GTK_USE_PORTAL="0"
export NO_AT_BRIDGE="1"

# GTK modules and fontconfig
export GTK_PATH="$CONTENTS_DIR/Resources/lib/gtk-3.0"
if [ -d "$CONTENTS_DIR/Resources/etc/fonts" ]; then
  export FONTCONFIG_PATH="$CONTENTS_DIR/Resources/etc/fonts"
  export FONTCONFIG_FILE="$CONTENTS_DIR/Resources/etc/fonts/fonts.conf"
fi

# Short disposable GVfs cache home
export XDG_CACHE_HOME="$MACMATECAJA_GVFS_CACHE"

# Caja resource paths relocation
export CAJA_DATADIR="$RESOURCE_SHARE/caja"
export CAJA_PIXMAPDIR="$RESOURCE_SHARE/pixmaps/caja"
export CAJA_UIDIR="$RESOURCE_SHARE/caja/ui"
export MACMATECAJA_SHARED_DIR="$RESOURCE_SHARE"
export MATELOCALEDIR="$RESOURCE_SHARE/locale"

args=()
for arg in "$@"; do
  case "$arg" in
    -psn_*) ;;
    *) args+=("$arg") ;;
  esac
done

# 1. Session-local gdk-pixbuf loaders cache
LOADER_DIR="$CONTENTS_DIR/Resources/lib/gdk-pixbuf-2.0/2.10.0/loaders"
LOADERS_CACHE="$SESSION_DIR/loaders.cache"
TEMPLATE_CACHE="$CONTENTS_DIR/Resources/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache.template"

if [ -f "$TEMPLATE_CACHE" ]; then
  ESC_LOADER_DIR="$(sed_escape "$LOADER_DIR")"
  /usr/bin/sed "s|@LOADER_DIR@|$ESC_LOADER_DIR|g" "$TEMPLATE_CACHE" > "$LOADERS_CACHE"
  export GDK_PIXBUF_MODULE_FILE="$LOADERS_CACHE"
fi

# 2. Session-local GTK immodules cache
IMMODULES_DIR="$CONTENTS_DIR/Resources/lib/gtk-3.0/3.0.0/immodules"
IMMODULES_CACHE="$SESSION_DIR/immodules.cache"
IMMODULES_TEMPLATE="$CONTENTS_DIR/Resources/lib/gtk-3.0/3.0.0/immodules.cache.template"

if [ -f "$IMMODULES_TEMPLATE" ]; then
  ESC_IM_DIR="$(sed_escape "$IMMODULES_DIR")"
  /usr/bin/sed "s|@IMMODULES_DIR@|$ESC_IM_DIR|g" "$IMMODULES_TEMPLATE" > "$IMMODULES_CACHE"
  export GTK_IM_MODULE_FILE="$IMMODULES_CACHE"
fi

# 3. D-Bus session bus setup
if [ -z "${DBUS_SESSION_BUS_ADDRESS:-}" ]; then
  DBUS_RUN_SESSION="$CONTENTS_DIR/MacOS/dbus-run-session"
  if [ ! -x "$DBUS_RUN_SESSION" ]; then
    DBUS_RUN_SESSION="$(command -v dbus-run-session || true)"
  fi

  if [ -n "$DBUS_RUN_SESSION" ] && [ -x "$DBUS_RUN_SESSION" ]; then
    DBUS_CONFIG="$SESSION_DIR/bus.conf"
    DBUS_SERVICE_DIR="$SESSION_DIR/services"
    mkdir -p "$DBUS_SERVICE_DIR"

    ESC_LIBEXEC="$(sed_escape "$CONTENTS_DIR/Resources/libexec")"
    ESC_BIN="$(sed_escape "$CONTENTS_DIR/MacOS")"

    # Populate relocatable D-Bus service files with quoted Exec paths
    for svc in "$RESOURCE_SHARE/dbus-1/services"/*.service; do
      if [ -f "$svc" ]; then
        sname="$(basename "$svc")"
        /usr/bin/sed \\
          -e "s|@APP_LIBEXEC@|$ESC_LIBEXEC|g" \\
          -e "s|@APP_BIN@|$ESC_BIN|g" \\
          "$svc" > "$DBUS_SERVICE_DIR/$sname"
      fi
    done

    {
      printf '<busconfig><type>session</type><keep_umask/><listen>unix:tmpdir=/tmp</listen><auth>EXTERNAL</auth>\\n'
      printf '<servicedir>%s</servicedir>\\n' "$(xml_escape "$DBUS_SERVICE_DIR")"
      printf '<standard_session_servicedirs/><policy context="default"><allow send_destination="*" eavesdrop="true"/><allow eavesdrop="true"/><allow own="*"/></policy>\\n'
      printf '<limit name="service_start_timeout">120000</limit></busconfig>\\n'
    } > "$DBUS_CONFIG"

    # Directly launch caja under dbus-run-session without recursive shell relaunch!
    if [ "${#args[@]}" -eq 0 ]; then
      "$DBUS_RUN_SESSION" --config-file="$DBUS_CONFIG" -- "$CONTENTS_DIR/MacOS/caja" --browser --no-desktop
    else
      "$DBUS_RUN_SESSION" --config-file="$DBUS_CONFIG" -- "$CONTENTS_DIR/MacOS/caja" --browser --no-desktop "${args[@]}"
    fi
    status=$?
    exit $status
  fi
  echo "MacMateCaja: D-Bus session bus required but dbus-run-session not found." >&2
  exit 1
fi

# If DBUS_SESSION_BUS_ADDRESS was already provided from outside:
if [ "${#args[@]}" -eq 0 ]; then
  "$CONTENTS_DIR/MacOS/caja" --browser --no-desktop
else
  "$CONTENTS_DIR/MacOS/caja" --browser --no-desktop "${args[@]}"
fi
status=$?
exit $status
"""

INFO_PLIST_CONTENT = """<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
 "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleDevelopmentRegion</key>
  <string>en</string>
  <key>CFBundleDisplayName</key>
  <string>MacMateCaja</string>
  <key>CFBundleExecutable</key>
  <string>MacMateCaja</string>
  <key>CFBundleIconFile</key>
  <string>MacMateCaja</string>
  <key>CFBundleIdentifier</key>
  <string>pl.okon.macmatecaja</string>
  <key>CFBundleName</key>
  <string>MacMateCaja</string>
  <key>CFBundlePackageType</key>
  <string>APPL</string>
  <key>CFBundleShortVersionString</key>
  <string>1.28.0</string>
  <key>CFBundleVersion</key>
  <string>1.28.0-macos-arm64-1</string>
  <key>LSMinimumSystemVersion</key>
  <string>26.0</string>
  <key>NSHighResolutionCapable</key>
  <true/>
</dict>
</plist>
"""


def locate_dependency(dep, macho_path, frameworks_dir, quartz_prefix, gvfs_prefix, brew_prefix, staged_prefix=None):
    """
    Map dependencies by mapping copied prefixes BEFORE original absolute paths.
    Resolves @rpath, Homebrew paths, and copied prefix paths with destination existence check.
    """
    dep_name = os.path.basename(dep)

    # Rebase legacy build inputs by their runtime suffix, never a developer's home.
    for marker, prefix in (("/prefix/quartz/", quartz_prefix), ("/runtime/gvfs/", gvfs_prefix)):
        if marker in dep:
            mapped = prefix / dep.split(marker, 1)[1]
            if mapped.exists():
                return mapped.resolve()

    # 1. Direct path check
    dep_p = Path(dep)
    if dep_p.is_absolute() and dep_p.exists():
        return dep_p.resolve()

    # 2. Check staged prefix (rebuilt Caja)
    if staged_prefix and dep.startswith("/opt/MacMateCaja"):
        sub = dep[len("/opt/MacMateCaja"):].lstrip('/')
        cand = staged_prefix / sub
        if cand.exists():
            return cand.resolve()

    # 4. Check Homebrew path mapping
    if dep.startswith("/opt/homebrew/"):
        sub = dep[len("/opt/homebrew/"):].lstrip('/')
        cand = brew_prefix / sub
        if cand.exists():
            return cand.resolve()

    # 5. Check @rpath / basename lookup in known library locations
    candidate_dirs = [
        frameworks_dir,
        quartz_prefix / "lib",
        gvfs_prefix / "lib",
        gvfs_prefix / "lib/gvfs",
        brew_prefix / "lib",
    ]
    if staged_prefix:
        candidate_dirs.insert(1, staged_prefix / "lib")

    for cdir in candidate_dirs:
        cand = cdir / dep_name
        if cand.exists():
            return cand.resolve()

    # Search Homebrew opt subdirectories
    opt_matches = glob.glob(str(brew_prefix / "opt/*/lib" / dep_name))
    if opt_matches:
        return Path(opt_matches[0]).resolve()

    # Search Homebrew Cellar
    cellar_matches = glob.glob(str(brew_prefix / "Cellar/*/*/*/lib" / dep_name)) + \
                     glob.glob(str(brew_prefix / "Cellar/*/*/lib" / dep_name)) + \
                     glob.glob(str(brew_prefix / "Cellar/*/*/*/lib/private" / dep_name)) + \
                     glob.glob(str(brew_prefix / "Cellar/*/*/lib/private" / dep_name))
    if cellar_matches:
        return Path(cellar_matches[0]).resolve()

    return None


def generate_loaders_cache_template(brew_cache_file, template_cache_file):
    """
    Parse gdk-pixbuf loaders.cache and replace all quoted loader paths with @LOADER_DIR@/<basename>.
    Handles Cellar paths and brew lib paths uniformly.
    """
    if not brew_cache_file.exists():
        print(f"Warning: {brew_cache_file} not found; cannot create loaders template", file=sys.stderr)
        return

    out_lines = []
    with open(brew_cache_file, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            stripped = line.strip()
            if stripped.startswith('"') and stripped.endswith('.so"'):
                loader_path = stripped.strip('"')
                loader_base = os.path.basename(loader_path)
                out_lines.append(f'"@LOADER_DIR@/{loader_base}"\n')
            elif stripped.startswith('# LoaderDir ='):
                out_lines.append('# LoaderDir = @LOADER_DIR@\n')
            else:
                out_lines.append(line)

    template_cache_file.parent.mkdir(parents=True, exist_ok=True)
    with open(template_cache_file, 'w', encoding='utf-8') as f:
        f.writelines(out_lines)


def generate_immodules_cache_template(brew_im_cache, template_im_cache):
    """
    Parse gtk-3.0 immodules.cache and replace module paths with @IMMODULES_DIR@/<basename>.
    """
    if not brew_im_cache.exists():
        return

    out_lines = []
    with open(brew_im_cache, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            stripped = line.strip()
            if stripped.startswith('"') and '.so"' in stripped:
                parts = stripped.split('"')
                if len(parts) >= 2:
                    mod_path = parts[1]
                    mod_base = os.path.basename(mod_path)
                    new_line = line.replace(f'"{mod_path}"', f'"@IMMODULES_DIR@/{mod_base}"')
                    out_lines.append(new_line)
                    continue
            out_lines.append(line)

    template_im_cache.parent.mkdir(parents=True, exist_ok=True)
    with open(template_im_cache, 'w', encoding='utf-8') as f:
        f.writelines(out_lines)


def rebase_fontconfig(brew_fonts_dir, target_fonts_dir):
    """
    Inspect and copy fontconfig configurations, flattening and resolving any symlinks
    so that no links escape back into /opt/homebrew.
    Rebases fonts.conf to eliminate Homebrew cache directories.
    """
    target_fonts_dir.mkdir(parents=True, exist_ok=True)
    conf_d_target = target_fonts_dir / "conf.d"
    conf_d_target.mkdir(parents=True, exist_ok=True)

    # Copy and rebase fonts.conf
    src_fonts_conf = brew_fonts_dir / "fonts.conf"
    if src_fonts_conf.exists():
        content = src_fonts_conf.read_text(encoding="utf-8", errors="ignore")
        # Remove /opt/homebrew/var/cache/fontconfig
        content = re.sub(r'<cachedir>/opt/homebrew/.*?</cachedir>', '', content)
        (target_fonts_dir / "fonts.conf").write_text(content, encoding="utf-8")

    # Copy conf.d files as real independent files
    src_conf_d = brew_fonts_dir / "conf.d"
    if src_conf_d.exists():
        for item in src_conf_d.iterdir():
            real_file = item.resolve()
            if real_file.is_file():
                shutil.copyfile(real_file, conf_d_target / item.name)


def build_app(quartz_prefix, gvfs_prefix, brew_prefix, dist_dir, workspace_root, staged_prefix=None):
    app_dir = dist_dir / "MacMateCaja.app"
    contents = app_dir / "Contents"
    macos_dir = contents / "MacOS"
    frameworks_dir = contents / "Frameworks"
    resources_dir = contents / "Resources"
    libexec_dir = resources_dir / "libexec"
    lib_dir = resources_dir / "lib"
    share_dir = resources_dir / "share"
    etc_dir = resources_dir / "etc"
    gio_modules_dir = lib_dir / "gio/modules"
    pixbuf_loaders_dir = lib_dir / "gdk-pixbuf-2.0/2.10.0/loaders"
    immodules_dir = lib_dir / "gtk-3.0/3.0.0/immodules"
    printbackends_dir = lib_dir / "gtk-3.0/3.0.0/printbackends"
    licenses_dir = resources_dir / "licenses"

    print(f"Cleaning previous app bundle at {app_dir}...")
    if app_dir.exists():
        shutil.rmtree(app_dir)

    for d in [macos_dir, frameworks_dir, libexec_dir, gio_modules_dir, pixbuf_loaders_dir, immodules_dir, printbackends_dir, share_dir, etc_dir, licenses_dir]:
        d.mkdir(parents=True, exist_ok=True)

    # 1. Info.plist & PkgInfo
    print("Writing Info.plist and PkgInfo...")
    (contents / "Info.plist").write_text(INFO_PLIST_CONTENT, encoding="utf-8")
    (contents / "PkgInfo").write_text("APPL????", encoding="utf-8")

    # 2. Icon & Launcher
    print("Copying icon and creating launcher...")
    icon_src = workspace_root / "assets/MacMateCaja.icns"
    if icon_src.exists():
        shutil.copy2(icon_src, resources_dir / "MacMateCaja.icns")
    elif (workspace_root / "app/MacMateCaja.app/Contents/Resources/MacMateCaja.icns").exists():
        shutil.copy2(workspace_root / "app/MacMateCaja.app/Contents/Resources/MacMateCaja.icns", resources_dir / "MacMateCaja.icns")

    launcher_path = macos_dir / "MacMateCaja"
    launcher_path.write_text(LAUNCHER_SCRIPT, encoding="utf-8")
    launcher_path.chmod(0o755)

    # 3. Binaries from Quartz / Staged
    print("Copying core Caja binaries...")
    bin_source_dir = staged_prefix / "bin" if (staged_prefix and (staged_prefix / "bin/caja").exists()) else quartz_prefix / "bin"
    for bin_name in ["caja", "caja-file-management-properties", "caja-autorun-software", "caja-connect-server"]:
        src = bin_source_dir / bin_name
        if src.exists():
            shutil.copy2(src, macos_dir / bin_name)
            (macos_dir / bin_name).chmod(0o755)
        else:
            raise RuntimeError(f"Required binary {bin_name} missing from {bin_source_dir}")

    # 4. Binaries from GVfs & Homebrew
    print("Copying dconf, dbus, and gio runtime binaries...")
    if (gvfs_prefix / "bin/dconf").exists():
        shutil.copy2(gvfs_prefix / "bin/dconf", macos_dir / "dconf")
        (macos_dir / "dconf").chmod(0o755)
    else:
        raise RuntimeError(f"dconf binary missing from {gvfs_prefix}/bin")

    for dbus_bin in ["dbus-daemon", "dbus-run-session"]:
        src = brew_prefix / "bin" / dbus_bin
        if src.exists():
            shutil.copy2(src, macos_dir / dbus_bin)
            (macos_dir / dbus_bin).chmod(0o755)
        else:
            print(f"Warning: {dbus_bin} not found in Homebrew", file=sys.stderr)

    # Bundle gio executable
    gio_bin = brew_prefix / "bin/gio"
    if gio_bin.exists():
        shutil.copy2(gio_bin, macos_dir / "gio")
        (macos_dir / "gio").chmod(0o755)

    # Check and bundle rclone
    rclone_bin = None
    for cand in [brew_prefix / "bin/rclone", Path("/usr/local/bin/rclone")]:
        if cand.exists():
            rclone_bin = cand
            break
    if rclone_bin:
        print(f"Bundling rclone executable from {rclone_bin}...")
        shutil.copy2(rclone_bin, macos_dir / "rclone")
        (macos_dir / "rclone").chmod(0o755)
        # Copy actual rclone COPYING / LICENSE file
        rclone_copying_matches = glob.glob(str(brew_prefix / "Cellar/rclone/*/COPYING*"))
        if rclone_copying_matches and os.path.exists(rclone_copying_matches[0]):
            shutil.copy2(rclone_copying_matches[0], licenses_dir / "rclone-LICENSE.txt")
        else:
            (licenses_dir / "rclone-LICENSE.txt").write_text("""Copyright (C) 2012 by Nick Craig-Wood http://www.craig-wood.com/nick/

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
""", encoding="utf-8")

    # 5. GVfs libexec daemons
    print("Copying GVfs daemons to Resources/libexec...")
    gvfs_libexec = gvfs_prefix / "libexec"
    if gvfs_libexec.exists():
        for item in gvfs_libexec.iterdir():
            if item.is_file() and not item.is_symlink():
                shutil.copy2(item, libexec_dir / item.name)
                (libexec_dir / item.name).chmod(0o755)

    # PDF thumbnailer
    thumb_src = workspace_root / "app/MacMateCaja.app/Contents/Resources/libexec/macmatecaja-pdf-thumbnailer"
    if thumb_src.exists():
        shutil.copy2(thumb_src, libexec_dir / "macmatecaja-pdf-thumbnailer")
        (libexec_dir / "macmatecaja-pdf-thumbnailer").chmod(0o755)

    # 6. GIO modules
    print("Copying GIO modules...")
    gvfs_gio = gvfs_prefix / "lib/gio/modules"
    if gvfs_gio.exists():
        for item in gvfs_gio.iterdir():
            if item.is_file() and not item.is_symlink():
                shutil.copy2(item, gio_modules_dir / item.name)
                (gio_modules_dir / item.name).chmod(0o755)
    # GIO TLS support is a separate plugin, needed by HTTPS/WebDAV backends.
    for item in (brew_prefix / "lib/gio/modules").glob("*.so"):
        shutil.copy2(item, gio_modules_dir / item.name)
    run_cmd([str(brew_prefix / "bin/gio-querymodules"), str(gio_modules_dir)])

    # 7. gdk-pixbuf loaders & template cache
    print("Copying gdk-pixbuf loaders and generating relative template...")
    brew_loaders = brew_prefix / "lib/gdk-pixbuf-2.0/2.10.0/loaders"
    if not brew_loaders.exists():
        cand = glob.glob(str(brew_prefix / "Cellar/gdk-pixbuf/*/lib/gdk-pixbuf-2.0/2.10.0/loaders"))
        if cand:
            brew_loaders = Path(cand[0])

    if brew_loaders.exists():
        for item in brew_loaders.iterdir():
            if item.name.endswith(".so"):
                shutil.copy2(item, pixbuf_loaders_dir / item.name)
                (pixbuf_loaders_dir / item.name).chmod(0o755)

    brew_cache = brew_prefix / "lib/gdk-pixbuf-2.0/2.10.0/loaders.cache"
    template_cache = lib_dir / "gdk-pixbuf-2.0/2.10.0/loaders.cache.template"
    generate_loaders_cache_template(brew_cache, template_cache)

    # 8. GTK immodules & printbackends
    print("Copying GTK immodules and printbackends...")
    brew_immodules = brew_prefix / "lib/gtk-3.0/3.0.0/immodules"
    if not brew_immodules.exists():
        cand = glob.glob(str(brew_prefix / "Cellar/gtk+3/*/lib/gtk-3.0/3.0.0/immodules"))
        if cand:
            brew_immodules = Path(cand[0])
    if brew_immodules.exists():
        for item in brew_immodules.iterdir():
            if item.name.endswith(".so"):
                shutil.copy2(item, immodules_dir / item.name)
                (immodules_dir / item.name).chmod(0o755)

    brew_printbackends = brew_prefix / "lib/gtk-3.0/3.0.0/printbackends"
    if not brew_printbackends.exists():
        cand = glob.glob(str(brew_prefix / "Cellar/gtk+3/*/lib/gtk-3.0/3.0.0/printbackends"))
        if cand:
            brew_printbackends = Path(cand[0])
    if brew_printbackends.exists():
        for item in brew_printbackends.iterdir():
            if item.name.endswith(".so"):
                shutil.copy2(item, printbackends_dir / item.name)
                (printbackends_dir / item.name).chmod(0o755)

    brew_im_cache = brew_prefix / "lib/gtk-3.0/3.0.0/immodules.cache"
    template_im_cache = lib_dir / "gtk-3.0/3.0.0/immodules.cache.template"
    generate_immodules_cache_template(brew_im_cache, template_im_cache)

    # 9. Fontconfig inspection and rebasing
    print("Rebasing fontconfig configuration...")
    brew_fonts = brew_prefix / "etc/fonts"
    if brew_fonts.exists():
        rebase_fontconfig(brew_fonts, etc_dir / "fonts")

    # 10. Shared resources (Caja data, pixmaps, icons, schemas, dbus services, mime)
    print("Copying share data, themes, icons, and schemas...")
    caja_share_src = staged_prefix / "share/caja" if (staged_prefix and (staged_prefix / "share/caja").exists()) else quartz_prefix / "share/caja"
    if caja_share_src.exists():
        shutil.copytree(caja_share_src, share_dir / "caja", dirs_exist_ok=True)

    pixmaps_src = staged_prefix / "share/pixmaps/caja" if (staged_prefix and (staged_prefix / "share/pixmaps/caja").exists()) else quartz_prefix / "share/pixmaps/caja"
    if pixmaps_src.exists():
        (share_dir / "pixmaps").mkdir(parents=True, exist_ok=True)
        shutil.copytree(pixmaps_src, share_dir / "pixmaps/caja", dirs_exist_ok=True)

    icons_src = quartz_prefix / "share/icons"
    if icons_src.exists():
        shutil.copytree(icons_src, share_dir / "icons", dirs_exist_ok=True)

    adwaita_src = brew_prefix / "share/icons/Adwaita"
    if adwaita_src.exists():
        shutil.copytree(adwaita_src, share_dir / "icons/Adwaita", dirs_exist_ok=True)

    desktop_src = staged_prefix / "share/applications" if (staged_prefix and (staged_prefix / "share/applications").exists()) else quartz_prefix / "share/applications"
    if desktop_src.exists():
        shutil.copytree(desktop_src, share_dir / "applications", dirs_exist_ok=True)

    mime_src = quartz_prefix / "share/mime"
    if mime_src.exists():
        shutil.copytree(mime_src, share_dir / "mime", dirs_exist_ok=True)
    brew_mime = brew_prefix / "share/mime"
    if brew_mime.exists():
        shutil.copytree(brew_mime, share_dir / "mime", dirs_exist_ok=True)

    thumb_data_src = workspace_root / "app/MacMateCaja.app/Contents/Resources/share/thumbnailers"
    if thumb_data_src.exists():
        shutil.copytree(thumb_data_src, share_dir / "thumbnailers", dirs_exist_ok=True)

    locales_src = quartz_prefix / "share/locale"
    if locales_src.exists():
        shutil.copytree(locales_src, share_dir / "locale", dirs_exist_ok=True)

    # Compile GSettings schemas
    schemas_dst = share_dir / "glib-2.0/schemas"
    schemas_dst.mkdir(parents=True, exist_ok=True)
    for sdir in [
        quartz_prefix / "share/glib-2.0/schemas",
        gvfs_prefix / "share/glib-2.0/schemas",
        workspace_root / "source/mate-desktop/schemas",
        brew_prefix / "share/glib-2.0/schemas"
    ]:
        if sdir.exists():
            for f in sdir.glob("*.xml"):
                shutil.copy2(f, schemas_dst / f.name)
            for f in sdir.glob("*.xml.in"):
                content = f.read_text(encoding="utf-8", errors="ignore")
                content = content.replace("@datadir@", "/Applications/MacMateCaja.app/Contents/Resources/share")
                out_name = f.name[:-3]
                (schemas_dst / out_name).write_text(content, encoding="utf-8")

    print("Compiling glib-2.0 schemas...")
    run_cmd(["glib-compile-schemas", str(schemas_dst)], check=True)

    # DBus services (with quoted Exec paths)
    dbus_svc_dst = share_dir / "dbus-1/services"
    dbus_svc_dst.mkdir(parents=True, exist_ok=True)
    for sdir in [quartz_prefix / "share/dbus-1/services", gvfs_prefix / "share/dbus-1/services"]:
        if sdir.exists():
            for f in sdir.glob("*.service"):
                content = f.read_text(encoding="utf-8", errors="ignore")
                # Normalize Exec paths to use quoted executable paths
                content = re.sub(r'Exec=.*/(gvfsd\S*)', r'Exec="@APP_LIBEXEC@/\1"', content)
                content = re.sub(r'Exec=.*/(dconf-service\S*)', r'Exec="@APP_LIBEXEC@/\1"', content)
                content = re.sub(r'Exec=.*/(caja\S*)(.*)', r'Exec="@APP_BIN@/\1"\2', content)
                (dbus_svc_dst / f.name).write_text(content, encoding="utf-8")

    # Upstream licenses & provenance
    print("Bundling upstream licenses, AUTHORS, and provenance notes...")
    for lic_file in ["LICENSE", "AUTHORS.md", "LICENSE-NOTES.md"]:
        p = workspace_root / lic_file
        if p.exists():
            shutil.copy2(p, licenses_dir / lic_file)
    caja_copying = workspace_root / "source/caja/COPYING"
    if caja_copying.exists():
        shutil.copy2(caja_copying, licenses_dir / "COPYING")
    caja_copying_lib = workspace_root / "source/caja/COPYING.LIB"
    if caja_copying_lib.exists():
        shutil.copy2(caja_copying_lib, licenses_dir / "COPYING.LIB")
    caja_authors = workspace_root / "source/caja/AUTHORS"
    if caja_authors.exists():
        shutil.copy2(caja_authors, licenses_dir / "AUTHORS-caja.txt")
    mate_authors = workspace_root / "source/mate-desktop/AUTHORS"
    if mate_authors.exists():
        shutil.copy2(mate_authors, licenses_dir / "AUTHORS-mate-desktop.txt")

    (licenses_dir / "SOURCE-PROVENANCE.md").write_text("""# MacMateCaja Source Provenance and Redistribution Notice

> [!CAUTION]
> **PUBLIC REDISTRIBUTION STATUS: BLOCKED**
> Complete upstream corresponding-source collection has not yet been mirrored into an offline
> distribution archive. Redistribution of these binaries is strictly limited to local evaluation
> until all corresponding source archives and build scripts are mirrored.

## 1. Upstream Components and License Provenance
- **MATE Caja**: GPL-2.0-or-later (Upstream: https://github.com/mate-desktop/caja, tag 1.28.0)
- **mate-desktop-lite**: LGPL-2.0-or-later / GPL-2.0-or-later (Upstream: https://github.com/mate-desktop/mate-desktop, tag 1.28.2)
- **GVfs**: LGPL-2.0-or-later (Upstream: https://gitlab.gnome.org/GNOME/gvfs, tag 1.50.4)
- **dconf**: LGPL-2.1-or-later (Upstream: https://gitlab.gnome.org/GNOME/dconf)
- **GTK+ 3**: LGPL-2.1-or-later (Homebrew formula gtk+3 3.24.52)
- **GLib**: LGPL-2.1-or-later (Homebrew formula glib 2.88.2)
- **Samba**: GPL-3.0-or-later (Homebrew formula samba 4.24.4)
- **rclone**: MIT License (Homebrew formula rclone 1.75.1)

## 2. Upstream Authors and Credits
Please refer to AUTHORS-caja.txt, AUTHORS-mate-desktop.txt, AUTHORS.md, and LICENSE-NOTES.md in this directory.
""", encoding="utf-8")

    # 11. Initial Dylibs from Quartz, Staged, and GVfs
    print("Copying runtime dylibs into Frameworks...")
    initial_libs = [
        gvfs_prefix / "lib/libdconf.1.dylib",
        gvfs_prefix / "lib/gvfs/libgvfsdaemon.dylib",
        gvfs_prefix / "lib/gvfs/libgvfscommon.dylib",
        quartz_prefix / "lib/libmate-desktop-2.17.dylib",
    ]
    if staged_prefix and (staged_prefix / "lib/libcaja-extension.1.dylib").exists():
        initial_libs.append(staged_prefix / "lib/libcaja-extension.1.dylib")
    else:
        initial_libs.append(quartz_prefix / "lib/libcaja-extension.1.dylib")

    for dlib in initial_libs:
        if dlib.exists():
            shutil.copy2(dlib, frameworks_dir / dlib.name)
        else:
            raise RuntimeError(f"Initial runtime library {dlib} does not exist!")

    # Compatibility symlinks in Frameworks
    for sym_name, target in [
        ("libcaja-extension.dylib", "libcaja-extension.1.dylib"),
        ("libmate-desktop-2.dylib", "libmate-desktop-2.17.dylib"),
        ("libdconf.dylib", "libdconf.1.dylib")
    ]:
        sym_p = frameworks_dir / sym_name
        sym_p.unlink(missing_ok=True)
        os.symlink(target, sym_p)

    # 12. Transitive Mach-O closure
    print("Resolving transitive Mach-O dependencies...")
    bundled_macho = set()
    queue = []
    dep_alias_map = {}  # Maps dep_name -> actual file in Frameworks
    basename_source_map = {}

    def scan_bundle():
        for root, dirs, files in os.walk(contents):
            for f in files:
                p = Path(root) / f
                if is_macho(p) and not p.is_symlink():
                    if p not in bundled_macho:
                        bundled_macho.add(p)
                        queue.append(p)
                        if p.parent == frameworks_dir:
                            basename_source_map[p.name] = p

    scan_bundle()

    while queue:
        macho = queue.pop(0)
        deps = get_dependencies(macho)
        for dep in deps:
            if is_system_lib(dep):
                continue

            real_path = locate_dependency(dep, macho, frameworks_dir, quartz_prefix, gvfs_prefix, brew_prefix, staged_prefix)
            if not real_path:
                raise RuntimeError(f"FATAL: Unresolved non-system dependency '{dep}' referenced by Mach-O binary '{macho}'")

            dep_basename = os.path.basename(dep)
            target_name = real_path.name
            target_path = frameworks_dir / target_name

            # Collision check
            if target_name in basename_source_map:
                existing = basename_source_map[target_name]
                if existing != target_path and real_path != existing:
                    if sha256_file(real_path) != sha256_file(existing):
                        raise RuntimeError(f"Basename collision detected: '{target_name}' from '{real_path}' and '{existing}' have differing content!")

            if not target_path.exists():
                shutil.copy2(real_path, target_path)
                target_path.chmod(0o755)
                bundled_macho.add(target_path)
                basename_source_map[target_name] = real_path
                queue.append(target_path)

            # Ensure dep_basename exists in Frameworks (create symlink if real_path.name differs from dep_basename)
            if dep_basename != target_name:
                alias_path = frameworks_dir / dep_basename
                if not alias_path.exists():
                    os.symlink(target_name, alias_path)
                dep_alias_map[dep_basename] = target_name

            # Also replicate any companion symlinks from source directory
            src_dir = real_path.parent
            for sibling in src_dir.iterdir():
                if sibling.is_symlink():
                    try:
                        resolved = sibling.resolve()
                        if resolved == real_path:
                            sib_alias = frameworks_dir / sibling.name
                            if not sib_alias.exists():
                                os.symlink(target_name, sib_alias)
                    except Exception:
                        pass

    print(f"Total bundled Mach-O files: {len(bundled_macho)}")

    # 13. Rewriting install names and dependencies with install_name_tool
    print("Rewriting Mach-O install names, rpaths, and dependency links...")

    # Set ID on every dylib in Frameworks
    for f in frameworks_dir.iterdir():
        if is_macho(f) and not f.is_symlink():
            f.chmod(0o755)
            run_cmd(["install_name_tool", "-id", f"@rpath/{f.name}", str(f)], check=True)

    # Process all Mach-O files
    for macho in sorted(bundled_macho):
        macho.chmod(0o755)
        # Plugins can carry LC_ID_DYLIB too; -change does not rewrite that ID.
        identity = run_cmd(["otool", "-D", str(macho)]).stdout.splitlines()[1:]
        if identity:
            run_cmd(["install_name_tool", "-id", "@rpath/" + Path(identity[0].strip()).name, str(macho)])
        old_rpaths = get_rpaths(macho)
        for rp in old_rpaths:
            run_cmd(["install_name_tool", "-delete_rpath", rp, str(macho)], check=False)

        # Precise relative rpath calculation:
        # Contents/MacOS: up 1 to Contents -> @loader_path/../Frameworks, @executable_path/../Frameworks
        # Contents/Resources/libexec: up 2 to Contents -> @loader_path/../../Frameworks, @executable_path/../../Frameworks
        # Contents/Frameworks: @loader_path
        # Contents/Resources/lib/gio/modules: up 4 to Contents -> @loader_path/../../../../Frameworks, @executable_path/../../../../Frameworks
        # Contents/Resources/lib/gdk-pixbuf-2.0/2.10.0/loaders: up 5 to Contents -> @loader_path/../../../../../Frameworks, @executable_path/../../../../../Frameworks
        # Contents/Resources/lib/gtk-3.0/3.0.0/immodules: up 5 to Contents -> @loader_path/../../../../../Frameworks, @executable_path/../../../../../Frameworks
        # Contents/Resources/lib/gtk-3.0/3.0.0/printbackends: up 5 to Contents -> @loader_path/../../../../../Frameworks, @executable_path/../../../../../Frameworks
        parent = macho.parent
        if parent == macos_dir:
            run_cmd(["install_name_tool", "-add_rpath", "@loader_path/../Frameworks", str(macho)], check=True)
            run_cmd(["install_name_tool", "-add_rpath", "@executable_path/../Frameworks", str(macho)], check=True)
        elif parent == libexec_dir:
            run_cmd(["install_name_tool", "-add_rpath", "@loader_path/../../Frameworks", str(macho)], check=True)
            run_cmd(["install_name_tool", "-add_rpath", "@executable_path/../../Frameworks", str(macho)], check=True)
        elif parent == frameworks_dir:
            run_cmd(["install_name_tool", "-add_rpath", "@loader_path", str(macho)], check=True)
        elif parent == gio_modules_dir:
            run_cmd(["install_name_tool", "-add_rpath", "@loader_path/../../../../Frameworks", str(macho)], check=True)
            run_cmd(["install_name_tool", "-add_rpath", "@executable_path/../../../../Frameworks", str(macho)], check=True)
        elif parent in (pixbuf_loaders_dir, immodules_dir, printbackends_dir):
            run_cmd(["install_name_tool", "-add_rpath", "@loader_path/../../../../../Frameworks", str(macho)], check=True)
            run_cmd(["install_name_tool", "-add_rpath", "@executable_path/../../../../../Frameworks", str(macho)], check=True)

        # Update all non-system dependencies to @rpath/<dep_name>
        deps = get_dependencies(macho)
        for dep in deps:
            if is_system_lib(dep):
                continue
            dep_basename = os.path.basename(dep)

            # Ensure the targeted library or alias exists in Frameworks
            if not (frameworks_dir / dep_basename).exists():
                raise RuntimeError(f"FATAL: Dependency {dep_basename} for {macho} not found in Frameworks!")

            target_dep = f"@rpath/{dep_basename}"
            run_cmd(["install_name_tool", "-change", dep, target_dep, str(macho)], check=True)

    # 14. Ad-hoc codesigning
    print("Ad-hoc codesigning all Mach-O files and the app bundle...")
    for f in sorted(bundled_macho, key=lambda p: len(str(p)), reverse=True):
        run_cmd(["codesign", "--force", "--sign", "-", "--timestamp=none", str(f)], check=True)

    run_cmd(["codesign", "--force", "--sign", "-", "--timestamp=none", str(app_dir)], check=True)

    print("Verifying code signature...")
    run_cmd(["codesign", "--verify", "--deep", "--strict", str(app_dir)], check=True)
    print("Bundle signed and verified successfully.")
    return app_dir


def build_pkg_and_zip(app_dir, dist_dir, version="1.28.0"):
    pkg_path = dist_dir / f"MacMateCaja-{version}-arm64.pkg"
    zip_path = dist_dir / f"MacMateCaja-{version}-arm64.zip"

    print(f"Building unsigned component package at {pkg_path}...")
    if pkg_path.exists():
        pkg_path.unlink()

    run_cmd([
        "pkgbuild",
        "--component", str(app_dir),
        "--install-location", "/Applications",
        str(pkg_path)
    ], check=True)

    print(f"Building ZIP archive at {zip_path}...")
    if zip_path.exists():
        zip_path.unlink()

    run_cmd([
        "ditto",
        "-c", "-k",
        "--keepParent",
        str(app_dir),
        str(zip_path)
    ], check=True)

    return pkg_path, zip_path


def main():
    parser = argparse.ArgumentParser(description="Package MacMateCaja into a standalone macOS bundle.")
    parser.add_argument("--quartz-prefix", type=Path, default=Path("prefix/quartz"))
    parser.add_argument("--gvfs-prefix", type=Path, default=Path("runtime/gvfs"))
    parser.add_argument("--brew-prefix", type=Path, default=Path("/opt/homebrew"))
    parser.add_argument("--dist-dir", type=Path, default=Path("dist"))
    parser.add_argument("--workspace-root", type=Path, default=Path("."))
    parser.add_argument("--staged-prefix", type=Path, default=None)
    args = parser.parse_args()

    workspace_root = args.workspace_root.resolve()
    quartz_prefix = (workspace_root / args.quartz_prefix).resolve()
    gvfs_prefix = (workspace_root / args.gvfs_prefix).resolve()
    brew_prefix = args.brew_prefix.resolve()
    dist_dir = (workspace_root / args.dist_dir).resolve()
    dist_dir.mkdir(parents=True, exist_ok=True)

    staged_prefix = None
    if args.staged_prefix:
        staged_prefix = (workspace_root / args.staged_prefix).resolve()
    else:
        cand_staged = workspace_root / "build/staged_caja/opt/MacMateCaja"
        if cand_staged.exists():
            staged_prefix = cand_staged

    print("=== MacMateCaja Packaging ===")
    print(f"Workspace: {workspace_root}")
    print(f"Quartz Prefix: {quartz_prefix}")
    print(f"GVfs Prefix: {gvfs_prefix}")
    print(f"Homebrew Prefix: {brew_prefix}")
    print(f"Staged Rebuilt Prefix: {staged_prefix}")
    print(f"Dist Dir: {dist_dir}")

    app_dir = build_app(quartz_prefix, gvfs_prefix, brew_prefix, dist_dir, workspace_root, staged_prefix)
    pkg_path, zip_path = build_pkg_and_zip(app_dir, dist_dir)

    print("\n=== Deliverables Produced ===")
    for path in [app_dir, pkg_path, zip_path]:
        if path.is_file():
            size_mb = path.stat().st_size / (1024 * 1024)
            sha = sha256_file(path)
            print(f"  {path.name} ({size_mb:.2f} MB) - SHA256: {sha}")
        else:
            total_size = sum(f.stat().st_size for f in path.rglob('*') if f.is_file())
            print(f"  {path.name}/ ({total_size / (1024 * 1024):.2f} MB)")


if __name__ == "__main__":
    main()
