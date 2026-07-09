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

method_pattern = re.compile(
    r'''(    boolean canPlaceEntityOnDisplay\(\s*int displayId,\s*[^)]*\)\s*\{\n)'''
)
match = method_pattern.search(updated)
if not match:
    print(f"Did not find canPlaceEntityOnDisplay method in {path}", file=sys.stderr)
    sys.exit(1)

guard = '''        if (isFujisanDockedSecondaryDisplay(displayId)) {
            return true;
        }
'''

updated = updated[:match.end()] + guard + updated[match.end():]

helper = '''    private boolean isFujisanDockedSecondaryDisplay(int displayId) {
        return displayId == 1
                && SystemProperties.getBoolean("ro.feature.target_dual_display", false)
                && "4".equals(SystemProperties.get("persist.vendor.fujisan.display_mode", "1"));
    }'''

next_comment = "    /**\n     * Check if configuration of specified display matches current global config.\n"
if next_comment not in updated:
    print(f"Did not find displayConfigMatchesGlobal anchor in {path}", file=sys.stderr)
    sys.exit(1)
updated = updated.replace(next_comment, helper + "\n\n" + next_comment, 1)

path.write_text(updated)
print(f"Applied fujisan multidisplay ActivityStackSupervisor patch to {path}")
PY
