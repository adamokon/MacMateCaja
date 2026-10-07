# Contributing

Read AGENTS.md before testing. Preserve native GTK Quartz, browser-only mode,
GIO/GVfs and asynchronous file metadata/thumbnail behavior. Test file operations
only with synthetic files in a unique temporary tree, never real user data.

Run configuration and launcher tests after changes; build Caja. GUI-affecting
changes additionally need manual verification of pane startup, shortcuts,
selection, thumbnails and remote access on disposable fixtures.

Do not store personal default locations in source. Use the configuration module.
Do not publish build outputs, private profiles or inherited developer history.
Use `scripts/export-public.py NEW_DIRECTORY --deny PRIVATE_IDENTIFIER` to make
an audited source snapshot. Export never overwrites/deletes a destination and
does not alter any source or binary asset bytes.

The clean export contains prepared distribution inputs (configure, Makefile.in
and generated marshaller sources) required for the first build. Upstream ignore
rules can hide those inputs: when initializing a new repository from an audited,
pristine export, stage **the manifest-listed files**, including ignored inputs.
Do not force-add a working/build directory or a user profile.
