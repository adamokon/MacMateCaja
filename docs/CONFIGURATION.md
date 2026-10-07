# Configuration and private profiles

The configuration is read at startup, not continuously watched. Fully quit the
app before testing another profile; an already running Caja instance keeps its
startup configuration.

Search order:

1. `MACMATECAJA_CONFIG_FILE`, when explicitly set to an absolute filename.
2. `$XDG_CONFIG_HOME/macmatecaja/config.ini`, with an absolute XDG directory.
3. `$HOME/.config/macmatecaja/config.ini`.

Missing/malformed files use neutral defaults: Desktop if it exists, otherwise
home, in the left pane; home in the right pane; no added permanent places.
Invalid entries are skipped. An invalid explicit override fails closed.
Explicit command-line locations override configured default panes.

```ini
[ui]
left_path=/Volumes/Archive
right_path=sftp://user@example.invalid/documents

[places]
Archive=/Volumes/Archive
Documents=sftp://user@example.invalid/documents
```

Values are absolute native paths or file/sftp/smb/ftp/afp/dav/davs URIs. Spaces
and Unicode in native paths are supported; encode URI path spaces as `%20`.
Labels in `[places]` are displayed literally, even while a volume is offline.
No shell commands, environment-variable or tilde expansion is performed.
Do not place passwords/tokens in URIs; use normal backend authentication.

Keep private profiles outside the repository, with permissions `0600`. Select
one for a single launch:

```sh
MACMATECAJA_CONFIG_FILE="$HOME/.config/macmatecaja/personal.ini" \
  ./app/MacMateCaja.app/Contents/MacOS/MacMateCaja
```

Existing GTK/Caja bookmarks, saved connection profiles, SSH configuration and
credentials remain in the user's normal configuration directories. They are
not migrated, deleted, or exported to the public source snapshot.

The source launcher resolves the project/runtime relative to its app bundle.
`MACMATECAJA_QUARTZ_PREFIX`, `MACMATECAJA_GVFS_PREFIX` and
`MACMATECAJA_HOMEBREW_PREFIX` override dependency locations. This does not make
compiled dylibs relocatable: rebuild when moving an installed runtime.
