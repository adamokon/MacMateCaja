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
