# Native build

Use macOS with Xcode command-line tools and Homebrew. The development runtime
uses GTK 3 Quartz; verify `pkg-config --variable=targets gdk-3.0` includes
`quartz`. Do not substitute an X11 GTK build.

Dependencies used by this port include:

```text
gtk+3 glib (>= 2.66) dbus gettext pkg-config meson ninja
autoconf automake libtool libxml2 libsoup samba libnfs libnotify
shared-mime-info libexif gobject-introspection python3
```

Clone/extract into a path without spaces (upstream autotools limitation), then:

```sh
bash scripts/build-native.sh
bash tests/run-config-tests.sh
python3 tests/test-launcher.py
bash tests/run-deep-count-tests.sh
./app/MacMateCaja.app/Contents/MacOS/MacMateCaja --version
open app/MacMateCaja.app
```

The script builds dconf and GVfs with Meson, the lightweight mate-desktop
subset, then Caja with `--enable-quartz-browser-only`. It installs only into
project-local `runtime/gvfs` and `prefix/quartz`. The script is derived from
the local build; dependency versions on another Mac can require adjustments.
Generated autotools inputs are included, so `mate-autogen` is not needed for
the first configure. Regenerate after changing Makefile.am/configure.ac with
the appropriate automake/autoconf tooling.

For incremental Caja-only work, first configure the development tree through
`bash scripts/build-native.sh`. Set `PKG_CONFIG_PATH` to the two project runtime
lib/pkgconfig directories plus Homebrew pkgconfig directories, then use
`make -C source/caja -j4 && make -C source/caja install`. Check
`source/caja/config.status --config` first: its prefix must be the repository's
private `prefix/quartz`, never the real system `/opt/MacMateCaja`.

To create the independent app and unsigned installer, run:

```sh
bash scripts/build-package.sh
python3 tests/test-packaging.py
```

Packaging uses a disposable clean Caja source copy under `build/` with neutral
prefix `/opt/MacMateCaja` and installs only through `DESTDIR`. It leaves the
development source/configuration untouched. The clean-copy step reuses the
source export filters to avoid stale generated files or embedded old build
paths. Its scratch source is removed on exit. Outputs go to ignored `dist/`.

PDF thumbnails use `gio` and macOS Quick Look. Remote file opening/editing also
needs rclone and macFUSE; macFUSE security approval is a macOS user action.
No temporary-download fallback is implemented for opening remote files.

The source launcher is path-relative, but dylib install names and configured
service files are bound to the runtime build prefix. Rebuild that development
runtime when moving the project. The separate packaged app embeds its libraries
and resources and does not require the project or Homebrew at runtime (optional
macFUSE/NetBird integrations retain their external requirements).

`VERSION` is the macOS port/bundle/installer version. The upstream Caja engine
base and its command-line version remain 1.28.0. Source publication does not
imply a public binary release; see [packaging](PACKAGING.md) for licensing gates.
