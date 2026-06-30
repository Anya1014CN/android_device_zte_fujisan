#!/bin/sh
set -eu

ROOT="${1:-$PWD}"
TARGET="$ROOT/hardware/qcom/display-caf/msm8996/libqdutils/qdMetaData.cpp"

if [ ! -f "$TARGET" ]; then
  echo "Missing target file: $TARGET" >&2
  exit 1
fi

python3 - "$TARGET" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()

replacements = [
    (
        '''        ALOGE("%s: Private handle is invalid - handle:%p id: %" PRIu64,\n                __func__, handle, handle->id);''',
        '''        ALOGE("%s: Private handle is invalid - handle:%p",\n                __func__, handle);''',
    ),
    (
        '''        ALOGE("%s: Invalid metadata fd - handle:%p id: %" PRIu64 "fd: %d",\n                __func__, handle, handle->id, handle->fd_metadata);''',
        '''        ALOGE("%s: Invalid metadata fd - handle:%p fd: %d",\n                __func__, handle, handle->fd_metadata);''',
    ),
    (
        '''            ALOGE("%s: metadata mmap failed - handle:%p id: %" PRIu64  "fd: %d err: %s",\n                __func__, handle, handle->id, handle->fd_metadata, strerror(errno));''',
        '''            ALOGE("%s: metadata mmap failed - handle:%p fd: %d err: %s",\n                __func__, handle, handle->fd_metadata, strerror(errno));''',
    ),
]

updated = text
applied = 0
for old, new in replacements:
    if old in updated:
        updated = updated.replace(old, new)
        applied += 1

if applied == 0:
    print(f"No matching qdMetaData.cpp hunks found in {path}", file=sys.stderr)
    sys.exit(1)

path.write_text(updated)
print(f"Applied {applied} qdMetaData.cpp replacements to {path}")
PY
