# Validation — 2026-10-07

Executed on the developer's Apple Silicon Mac, without remote user-file writes:

- Four GLib configuration tests passed: neutral defaults, Unicode/space paths
  and independent ownership, invalid values/reinitialization, XDG config lookup.
- Four synthetic launcher tests passed: default arguments, explicit CLI/Finder
  arguments, existing bus, failure cleanup. XML paths with spaces, ampersands
  and Unicode were tested. No real GUI file operations were generated.
- Five synthetic export tests passed: byte-preserving asset export, build-file
  filtering, private-path rejection, no overwrite of existing destinations,
  rejection of external symlinks.
- Native Caja build/install succeeded for the active local runtime.
- A full build from the clean source snapshot in an independent temporary
  directory succeeded: dconf, GVfs, mate-desktop-lite, Caja, schema installation.
  The rebuilt source launcher returned `MATE caja 1.28.0` with `--version`.
- Local Caja linkage check found no X11/xcb/libSM/libICE dependency.
- Public export audited developer-specific identifiers and native home paths;
  upstream authors/notices were retained, artwork bytes were copied unchanged.

Known build noise: upstream gettext emits a `gitignore` target warning and
skips the po sub-build; GTK deprecated API warnings remain. Full GUI regression
and validation on another computer were not performed in this preparation.
This is a source release, not a signed/notarized/relocatable binary package.
On 2026-10-08 the owner authorized GPL-2.0-or-later for original port files and
artwork, following Caja. Publication/license metadata changes do not expand
the GUI regression coverage reported above.

Publication audit also checked credential-token/private-key signatures. The
retained `source/gvfs/test/files/ssh_host_rsa_key` is a public upstream test
fixture, not a developer credential. Its SHA-256 is
`283408c73ae8e3cb3d262585bdd12d09d5eef667a6e3d6604a906d73b16b9b80`,
identical to the [GNOME GVfs upstream fixture](https://github.com/GNOME/gvfs/blob/master/test/files/ssh_host_rsa_key)
verified on 2026-10-08. Never use this public test key for real authentication.

## Installer integration — 2026-10-19

The latest installer sources and Caja runtime-resource overrides are integrated
into the repository root. Packaging no longer needs an external `project/`
workspace or a developer-specific home path.

- Configuration, synthetic launcher and export regression tests: 13/13 passed.
- Existing local installer after relocating the project: 7/7 package tests passed.
- A fresh package built from the consolidated runtime inputs: 202 Mach-O files;
  deep/strict ad-hoc signature verification, unsigned PKG and ZIP creation passed.
- Fresh output: 7/7 package tests passed, including Mach-O path checks, signatures,
  plist, PKG expansion, relocation into two temporary directories (one with spaces),
  synthetic GIO reads and isolated GUI startup/cleanup.
- Shell syntax checks and the source-only export/privacy audit passed.

The previously compiled source changes are included; this integration check did
not repeat a full native dependency rebuild. No production remote locations,
real user-file operations or system installation were part of these tests.
The current tested binary target is Apple Silicon and macOS 26+; no Intel,
older macOS or second physical machine validation is claimed.

Installer source publication does not authorize a public binary release:
complete corresponding sources and license material for bundled transitive
dependencies must still be collected first. The local package uses an ad-hoc
signature, not Apple Developer ID or notarization.

## Recursive sizing responsiveness — 2026-10-21

A live hang sample identified the main thread spending most of its time in
`deep_count_more_files_callback` / `deep_count_one`. Inode deduplication used
a linear array search for every entry, resulting in quadratic work on large
directory trees despite asynchronous GIO enumeration.

The inode set now uses owned 64-bit hash keys, preserving hardlink accounting
and missing/zero-inode behavior. Enumeration remains in batches of 100 at low
priority. Progress notifications from batches and directory boundaries are
throttled to 100ms; final results are always published.

- Three production-helper regressions passed, including 500,000 distinct entries
  in 0.019 seconds with 13 timer heartbeats delivered during the work.
- Thirteen existing configuration/launcher/export tests passed.
- Full Caja rebuild and staged installation succeeded; fresh standalone bundle
  packaging, signature verification and all seven package tests passed.
- Manual GUI test used only two directories and one newly created empty file
  under a unique temporary root: F6 moved the file to the other pane, verified
  on disk. The Move to submenu opened and displayed the other-pane F6 action.
  No real user data was copied, moved, renamed or deleted by these tests.

The updated application was installed locally. Its previous bundle was preserved
for rollback. A stale local Dock entry was retargeted from an archived development
bundle to the installed application; user profiles were not changed.

### 1.28.1 follow-up

A later report distinguished Copy working from Move being disabled. Read-only
metadata queries confirmed that the source parent lacked write permission and
its children reported `access::can-delete=FALSE`, even though the children were
writable. No disk permissions were changed. A synthetic parent-permission test
now reproduces that distinction using only a fresh temporary directory.

File-list readiness no longer waits for thumbnail completion; monitors still
request and publish thumbnails independently. Disabled Move to actions display
an explicit not-permitted label and explain the source-removal requirement.

- Five focused tracker/readiness/permission tests, four configuration tests,
  four synthetic launcher tests and five export tests passed (18 total).
- The full native dependency/Caja build passed.
- Packaging from a disposable, clean Caja source copy passed. The developer
  `config.status` SHA-256 was identical before and after packaging; its prefix
  remained the private development runtime, not the neutral `/opt` prefix.
- All seven tests of the freshly built 1.28.1 bundle/installer passed (25 tests
  across the five suites). No tests changed real user-file permissions.
- Scratch packaging sources were removed; both bundle version fields are the
  numeric port version 1.28.1. The engine base remains upstream Caja 1.28.0.
- Installed 1.28.1 passed six bundle tests under a sandbox denying the project,
  legacy paths and Homebrew; the PKG test was correctly skipped because only
  the application bundle is installed in Applications. The neutral-prefix
  dependency resolver was also checked to prefer this run's staged library.
