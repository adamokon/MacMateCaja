#!/usr/bin/env python3
"""Copy clean Caja build inputs, reusing the audited source-export filters."""
from pathlib import Path
import runpy
import shutil
import sys

root = Path(__file__).resolve().parents[1]
destination = Path(sys.argv[1]).resolve()
if (root / "build").resolve() not in destination.parents:
    raise SystemExit("Packaging sources must stay below the repository build directory")
destination.mkdir(parents=True, exist_ok=False)
selected = runpy.run_path(str(root / "scripts/export-public.py"))["selected"]
source = root / "source/caja"
for item in source.rglob("*"):
    relative = item.relative_to(root)
    if item.is_file() and selected(item, relative):
        if item.is_symlink() and root not in item.resolve().parents:
            raise SystemExit("External source symlink is not a build input")
        target = destination / item.relative_to(source)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(item, target)
