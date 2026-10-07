# Agent safety

Read this before modifying or testing MacMateCaja.

Never automate dragging, copying, moving, renaming or deleting real user data,
default panes, mounted volumes or existing remote directories. A past automated
drag test moved a real folder into another real directory. The change was
recovered, but diagnostic tests must never repeat that mistake.

For any file-operation test, create a unique directory with `mktemp -d` under
`$TMPDIR`. Put both test panes and newly created test items under it. Confirm
the displayed paths before mouse-down. Verify contents before and after each
operation. Do not use existing local or remote data as fixtures.

Preserve GTK Quartz (no X11), GIO/GVfs and browser-only startup. Keep GVfs cache
in `/tmp/MacMateCaja-gvfs-<uid>`: macOS SSH control sockets have a short path
limit. Never replace it with the much longer macOS per-user TMPDIR.

Do not publish user profiles, bookmarks, credentials, technical caches, build
outputs or old Git history. Preserve upstream notices and check license notes.
Never run destructive reset/cleanup commands on a dirty worktree.
