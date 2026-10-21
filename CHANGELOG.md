# Changes

## 1.28.1 — 2026-10-21

- Fix quadratic inode bookkeeping during recursive folder sizing: use full
  64-bit hash keys and low-priority batches of 100 entries.
- Throttle incremental statistics updates to 100ms while always publishing final
  results. Preserve hardlink accounting, cancellation and remote GIO support.
- Do not keep the file-list loading state waiting for thumbnails. Display
  placeholder icons immediately and update previews asynchronously.
- Explain disabled Move to actions when source removal is not permitted.
  Copying can be permitted even when moving is forbidden by source permissions;
  no permission checks are bypassed.
- Package from an isolated clean Caja source copy: a neutral packaging prefix
  no longer replaces the developer tree's configured installation prefix.
- Share macOS bundle/installer version through `VERSION`, with numeric bundle
  build metadata. The upstream engine base stays MATE Caja 1.28.0.

This is a source-only release. Local ad-hoc-signed Apple Silicon/macOS 26+
packages exist, but public binary distribution remains blocked pending the
complete source/license collection for all bundled dependencies.
