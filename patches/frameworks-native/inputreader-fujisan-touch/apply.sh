#!/bin/sh
set -eu

ROOT="${1:-$PWD}"
TARGET="$ROOT/frameworks/native/services/inputflinger/InputReader.cpp"

if [ ! -f "$TARGET" ]; then
    echo "fujisan: ERROR: $TARGET not found" >&2
    exit 1
fi

python3 - "$TARGET" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()
updated = text

for include, anchor in (
    ("#include <string.h>\n", "#include <stdlib.h>\n"),
    ("#include <cutils/properties.h>\n", "#include <log/log.h>\n"),
):
    if include not in updated:
        if anchor not in updated:
            print(f"fujisan: ERROR: missing include anchor {anchor.strip()} in {path}",
                  file=sys.stderr)
            sys.exit(1)
        updated = updated.replace(anchor, anchor + include, 1)

old = '''    getAbsoluteAxisInfo(ABS_MT_TRACKING_ID, &mRawPointerAxes.trackingId);
    getAbsoluteAxisInfo(ABS_MT_SLOT, &mRawPointerAxes.slot);

    if (mRawPointerAxes.trackingId.valid
'''

new = '''    getAbsoluteAxisInfo(ABS_MT_TRACKING_ID, &mRawPointerAxes.trackingId);
    getAbsoluteAxisInfo(ABS_MT_SLOT, &mRawPointerAxes.slot);

    if (property_get_bool("ro.feature.target_dual_display", false)
            && strcmp(getDeviceName().string(), "zte-touchscreen-2nd") == 0
            && mRawPointerAxes.x.valid
            && (mRawPointerAxes.x.minValue != 0 || mRawPointerAxes.x.maxValue != 1079)) {
        ALOGI("fujisan: normalizing secondary touch X axis from %d..%d to 0..1079",
                mRawPointerAxes.x.minValue, mRawPointerAxes.x.maxValue);
        mRawPointerAxes.x.minValue = 0;
        mRawPointerAxes.x.maxValue = 1079;
    }

    if (mRawPointerAxes.trackingId.valid
'''

if new not in updated:
    if old not in updated:
        print(f"fujisan: ERROR: could not find MultiTouchInputMapper axis block in {path}",
              file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(old, new, 1)

if updated == text:
    print(f"fujisan: InputReader secondary touch patch already present in {path}")
else:
    path.write_text(updated)
    print(f"fujisan: InputReader secondary touch patch applied to {path}")
PY
