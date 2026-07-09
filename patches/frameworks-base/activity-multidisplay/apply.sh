#!/bin/sh
set -eu

ROOT="${1:-$PWD}"
TARGET="$ROOT/frameworks/base/services/core/java/com/android/server/am/ActivityStackSupervisor.java"

if [ ! -f "$TARGET" ]; then
  echo "Missing target file: $TARGET" >&2
  exit 1
fi

python3 - "$TARGET" <<'PY'
from pathlib import Path
import re
import sys

path = Path(sys.argv[1])
text = path.read_text()

if "isFujisanDockedSecondaryDisplay" in text:
    print(f"ActivityStackSupervisor fujisan multidisplay patch already present in {path}")
    sys.exit(0)

updated = text

if "import android.os.SystemProperties;" not in updated:
    anchor = "import android.os.Process;\n"
    if anchor not in updated:
        print(f"Did not find import anchor in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(anchor, anchor + "import android.os.SystemProperties;\n", 1)

old = '''    boolean canPlaceEntityOnDisplay(int displayId, boolean resizeable) {
        return displayId == DEFAULT_DISPLAY || (mService.mSupportsMultiDisplay
                && (resizeable || displayConfigMatchesGlobal(displayId)));
    }'''

new = '''    boolean canPlaceEntityOnDisplay(int displayId, boolean resizeable) {
        return displayId == DEFAULT_DISPLAY
                || isFujisanDockedSecondaryDisplay(displayId)
                || (mService.mSupportsMultiDisplay
                        && (resizeable || displayConfigMatchesGlobal(displayId)));
    }

    private boolean isFujisanDockedSecondaryDisplay(int displayId) {
        return displayId == 1
                && SystemProperties.getBoolean("ro.feature.target_dual_display", false)
                && "4".equals(SystemProperties.get("persist.vendor.fujisan.display_mode", "1"));
    }'''

if old not in updated:
    pattern = re.compile(
        r'''    boolean canPlaceEntityOnDisplay\(int displayId, boolean resizeable\) \{\n'''
        r'''        return displayId == DEFAULT_DISPLAY \|\| \(mService\.mSupportsMultiDisplay\n'''
        r'''                && \(resizeable \|\| displayConfigMatchesGlobal\(displayId\)\)\);\n'''
        r'''    \}''')
    if not pattern.search(updated):
        print(f"Did not find canPlaceEntityOnDisplay block in {path}", file=sys.stderr)
        sys.exit(1)
    updated = pattern.sub(new, updated, count=1)
else:
    updated = updated.replace(old, new, 1)

path.write_text(updated)
print(f"Applied fujisan multidisplay ActivityStackSupervisor patch to {path}")
PY
