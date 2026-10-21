# Port-specific behavior

- GTK 3 Quartz browser-only application; no MATE desktop session or X11.
- Dark theme and larger base UI font, multiple independently navigable panes.
- Tmux-inspired prefix `Ctrl-b`, then `%` (vertical split), `"` (horizontal
  split), `o` (next pane), `x` (close pane). F1 displays shortcut help and origin.
- AI buttons copy the directory URI/path or selected file paths to the macOS
  clipboard. Successful copy adds a check; selection/navigation resets the
  respective button, and using either button resets the other.
- Local/GVfs file browsing, move-biased dragging, Ctrl/Command additive
  selection and operation-progress reporting.
- List columns: Name, Item, Size, Type, Date Modified. Directory item totals
  and allocated size are counted asynchronously and progressively displayed.
  Remote backends without allocated size expose a logical-size fallback.
- Amber directory silhouettes differ from file icons. Recursive content hints
  (PDF, IMG, VID, MUS, TXT, DOC, ZIP, FILE, MIX/PDF+) and item totals reuse the
  existing deep-count traversal. A slash means no descendant directories, not
  necessarily no files. Loading/unreadable states stay distinct.
- Image/PDF thumbnails run through Caja's background thumbnail queue. PDF
  thumbnails use Quick Look and delete temporary inputs. Cache uses the short
  disposable GVfs cache path, outside the source tree.
- Remote files opened/edited externally use rclone/macFUSE mapped paths. No
  temporary-download opening fallback. Mounting requires working macFUSE.
- NetBird peer/connection dialog; saved user connections remain private.
- Startup configuration is data-only INI; personal defaults never belong in
  public source. See CONFIGURATION.md for profiles and precedence.

## Responsiveness and permissions (1.28.1)

Recursive statistics use 64-bit inode hash keys and low-priority batches of 100
entries. Intermediate results are published at most every 100ms; final results
are always published. The file-list loading state requires basic/link metadata,
not completed thumbnails or recursive counts. Preview icons update later.

Copy and Move have different permission requirements. Move requires the ability
to remove each selected item from its parent, even when the item itself is owned
by the current user. A read-only parent can therefore permit Copy but disable
Move/F6. The menu labels that state as not permitted; the application must not
bypass OS permission checks or silently change disk ownership/ACLs.

The macOS port version is stored in VERSION; its bundle uses numeric version
metadata. The underlying engine is based on upstream MATE Caja 1.28.0.

The existing features above were preserved during source cleanup; this release
preparation validates the configuration/launcher and native build, not a full
new GUI regression of every historical feature.
