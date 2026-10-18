#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
WORKSPACE_ROOT="$PROJECT_ROOT"

echo "=== Step 1: Staging Rebuilt Neutral Caja ==="
bash "$SCRIPT_DIR/stage-caja-rebuilt.sh"

echo "=== Step 2: Running Application Packager ==="
python3 "$SCRIPT_DIR/package-app.py" \
  --workspace-root "$WORKSPACE_ROOT" \
  --quartz-prefix "$PROJECT_ROOT/prefix/quartz" \
  --gvfs-prefix "$PROJECT_ROOT/runtime/gvfs" \
  --brew-prefix "${MACMATECAJA_HOMEBREW_PREFIX:-/opt/homebrew}" \
  --dist-dir "$WORKSPACE_ROOT/dist"

echo "=== Packaging Complete ==="
