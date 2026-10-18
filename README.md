# MacMateCaja

A macOS port and enhanced software fork of [MATE Caja](https://github.com/mate-desktop/caja),
the file manager of the [MATE Desktop environment](https://mate-desktop.org/)
on Linux. This port is based on Caja 1.28.0 and runs natively through GTK 3 Quartz.
It is an independent project, not an official MATE release or endorsement.
This is a
source distribution with scripts for a self-contained, ad-hoc-signed macOS installer.
No Developer ID or Apple notarization is included.
No XQuartz, Docker or Linux desktop session is required.

Features include dark styling, multiple panes with tmux-inspired shortcuts,
AI path-copy buttons, background image/PDF thumbnails, asynchronous directory
counts/sizes and folder badges, GIO/GVfs remote browsing, and NetBird integration.
Remote files opened in external macOS applications use rclone/macFUSE mounts;
there is deliberately no download fallback for opening/editing files.
Thumbnail generation may use short-lived files which are removed afterward.

## Personal configuration

Copy `config/config.example.ini` to `~/.config/macmatecaja/config.ini` and replace
the example entries. See [configuration](docs/CONFIGURATION.md). No private
servers or favorite locations are shipped in the default configuration.

## Build and test

See [building](docs/BUILDING.md). Run `bash tests/run-config-tests.sh` to test
configuration independently of the GUI. Run `python3 tests/test-launcher.py`
to test the launcher with a synthetic runtime (no user file operations).
Run `python3 tests/test-export.py` for export/privacy regressions.

## Free macOS installer

See [packaging](docs/PACKAGING.md) and the [Polish installation guide](docs/INSTALL-PL.md).
After building native dependencies, run `bash scripts/build-package.sh` from this
repository. Local `.app`, unsigned `.pkg`, and `.zip` artifacts go into `dist/`.
The tested binary target is Apple Silicon on macOS 26+. Run
`python3 tests/test-packaging.py` to verify a local package using synthetic data.
Public binary releases remain blocked until complete corresponding sources and
license material for all bundled dependencies have been collected. The packaging
source is published now; no binary release is implied.

## Origins, authors and license

The foundation of this program is the work of the MATE Caja developers and,
before them, the Nautilus developers and contributors. Their copyright notices,
author lists, translations and license files are preserved. The macOS port and
enhancements do not replace or claim authorship of that original work.
See [credits](AUTHORS.md) and the original [Caja author list](source/caja/AUTHORS).

MacMateCaja and its original port additions are released under the **GNU General
Public License, version 2 or any later version (GPL-2.0-or-later)**, following
Caja's licensing. Libraries and other bundled source components retain their
own licenses; see [license notes](LICENSE-NOTES.md) and [LICENSE](LICENSE).

This repository is a clean source snapshot of the modified port, rather than
an import of the developer's old local Git history. It includes the modified
sources and build inputs, but no personal profiles or installed binary runtime.
For macOS-port issues, use this repository's issue tracker; Linux Caja upstream
is maintained at [mate-desktop/caja](https://github.com/mate-desktop/caja).
