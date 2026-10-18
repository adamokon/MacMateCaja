# MacMateCaja Packaging Architecture and Relocation Guide

This document describes the design, build steps, and relocation mechanics for the standalone Apple Silicon macOS package of MacMateCaja.

---

## 1. Overview and Architecture

MacMateCaja is packaged as a self-contained macOS Application Bundle (`MacMateCaja.app`), an unsigned `.pkg` component package targeting `/Applications`, and a distribution `.zip` archive.

### Core Architecture
- **Target OS**: macOS 26.0+ (Apple Silicon `arm64`). Minimum OS honestly matches bundled Mach-O `LC_BUILD_VERSION` minos metadata (`minos 26.0`).
- **Display Backend**: Native GTK 3 Quartz (no XQuartz, no X11 dependencies).
- **VFS and Remote Backends**: In-bundle GVfs (`gvfsd`, `gvfsd-sftp`, `gvfsd-smb`, `gvfsd-nfs`, etc.) communicating over a private, session-isolated D-Bus bus.
- **Code Signing**: Local ad-hoc code signature (`codesign --force --sign - --timestamp=none`). No Developer ID payment or Apple notarization required.
- **No System Modifications**: All build and staging steps occur strictly inside this workspace; nothing is written to real `/opt` or system locations.

---

## 2. Directory Layout inside `MacMateCaja.app`

```text
MacMateCaja.app/
└── Contents/
    ├── Info.plist               # App metadata, bundle ID, LSMinimumSystemVersion 26.0
    ├── PkgInfo                  # APPL????
    ├── MacOS/
    │   ├── MacMateCaja          # Shell launcher setting relocatable environment
    │   ├── caja                 # Rebuilt neutral-prefix Caja binary
    │   ├── caja-*               # Properties, autorun, connect-server binaries
    │   ├── dconf                # dconf CLI utility
    │   ├── dbus-daemon          # Session D-Bus message bus daemon
    │   ├── dbus-run-session     # D-Bus session launcher
    │   ├── gio                  # Standalone GIO utility
    │   └── rclone               # Optional rclone binary (if available)
    ├── Frameworks/              # Transitive Mach-O dependencies (GTK, GLib, etc.)
    │   ├── libcaja-extension.1.dylib
    │   ├── libmate-desktop-2.17.dylib
    │   ├── libdconf.1.dylib
    │   ├── libgvfsdaemon.dylib
    │   ├── libgtk-3.0.dylib
    │   └── ... (all rewritten to @rpath)
    └── Resources/
        ├── MacMateCaja.icns     # Application icon
        ├── libexec/             # Background daemons (gvfsd, dconf-service, thumbnailer)
        ├── lib/
        │   ├── gio/modules/     # GIO plugins (libgvfsdbus, libdconfsettings)
        │   └── gdk-pixbuf-2.0/  # Image loaders and relative loaders.cache.template
        ├── share/
        │   ├── caja/            # UI XMLs, CSS stylesheets, patterns, browser.xml
        │   ├── pixmaps/caja/    # Stock pixmaps
        │   ├── icons/           # Hicolor and Adwaita icons
        │   ├── applications/    # caja.desktop
        │   ├── glib-2.0/schemas/# Compiled GSettings schemas (gschemas.compiled)
        │   └── dbus-1/services/ # Template D-Bus service files
        ├── etc/                 # Fontconfig configuration
        └── licenses/            # Upstream licenses (GPL/LGPL) and provenance notes
```

---

## 3. Relocation Mechanics

To guarantee that the application can be moved to arbitrary directories (including paths containing spaces) and run without developer or Homebrew environment variables:

1. **Mach-O Linkage**:
   - Every bundled dynamic library in `Frameworks/` has its identity set to `@rpath/<basename>`.
   - Every binary, daemon, and plugin has its dependency references changed from absolute Homebrew/developer paths to `@rpath/<basename>`.
   - Appropriate relative rpaths are embedded:
     - `MacOS/`: `@loader_path/../Frameworks`, `@executable_path/../Frameworks`
     - `Resources/libexec/`: `@loader_path/../../Frameworks`, `@executable_path/../../Frameworks`
     - `Frameworks/`: `@loader_path`
     - `Resources/lib/gio/modules/`: `@loader_path/../../../../Frameworks`
     - `Resources/lib/gdk-pixbuf-2.0/.../loaders/`: `@loader_path/../../../../../Frameworks`

2. **Resource Resolution in Caja**:
   - Recompiled Caja binaries support runtime overrides via `CAJA_DATADIR`, `CAJA_PIXMAPDIR`, `CAJA_UIDIR`, `MACMATECAJA_SHARED_DIR`, and `MATELOCALEDIR`.
   - The launcher detects its bundle location dynamically and exports these variables pointing inside `Contents/Resources/share`.
   - Desktop entry (`applications/caja.desktop`) resolves cleanly from the shared resource tree.

3. **Per-Session D-Bus and Cache Isolation**:
   - The launcher creates an ephemeral session directory (`/tmp/MacMateCaja-${UID}/session.XXXXXX`) cleaned up on exit.
   - D-Bus service files (`.service`) are generated on-the-fly with exact absolute `Exec=` paths pointing to the current bundle instance.
   - `loaders.cache` is generated from a relative template with safely escaped paths, preventing multi-instance race conditions and supporting spaces in file paths.

---

## 4. Packaging Commands

Run these commands from the repository root, not an external `project/` wrapper.
First build the private native dependencies with `bash scripts/build-native.sh`.
The staging script then configures/rebuilds Caja with the neutral
`/opt/MacMateCaja` prefix, but installs it only under repository `build/` through
`DESTDIR`; it does not install into the system `/opt` directory.

1. **Staged Caja Overlay** (preserves mate-desktop and schemas without touching `/opt`):
   ```bash
   bash scripts/stage-caja-rebuilt.sh
   ```

2. **Run Packager**:
   ```bash
   bash scripts/build-package.sh
   ```
   Outputs:
   - `dist/MacMateCaja.app`
   - `dist/MacMateCaja-1.28.0-arm64.pkg`
   - `dist/MacMateCaja-1.28.0-arm64.zip`

3. **Run Verification and Relocation Test Suite**:
   ```bash
   python3 tests/test-packaging.py
   ```

To inspect a different local output directory, set `MACMATECAJA_TEST_DIST_DIR`.
The tests use synthetic temporary directories only. No remote servers or real
user files are test fixtures. GUI startup is stopped through the test's own
process group, not a global process-name kill.

## 5. Publication status

The repository publishes packaging **source**, not a notarized binary release.
Local evaluation packages have been built, but public binary redistribution is
blocked pending a complete corresponding-source and license collection for all
bundled dependencies (including Homebrew libraries and rclone's Go dependencies).
Do not upload local `dist/` packages as a public release until that audit passes.
Developer ID enrollment and notarization are not prerequisites for creating the
unsigned installer/ad-hoc-signed application; the user must explicitly approve
first opening of a downloaded package using macOS Privacy & Security settings.
