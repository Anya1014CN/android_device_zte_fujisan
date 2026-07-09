#!/bin/sh
set -eu

ROOT="${1:-$PWD}"
TARGET="$ROOT/frameworks/base/services/core/java/com/android/server/display/LogicalDisplay.java"

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
    print(f"LogicalDisplay fujisan docked-secondary patch already present in {path}")
    sys.exit(0)

updated = text

if "import android.os.SystemProperties;" not in updated:
    anchor = "import android.hardware.display.DisplayManagerInternal;\n"
    if anchor not in updated:
        print(f"Did not find import anchor in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(anchor, anchor + "import android.os.SystemProperties;\n", 1)

method_pattern = re.compile(
    r'''(    public void setHasContentLocked\(boolean hasContent\) \{\n)'''
    r'''(        mHasContent = hasContent;\n)'''
    r'''(    \}\n)'''
)
replacement = '''    public void setHasContentLocked(boolean hasContent) {
        if (isFujisanDockedSecondaryDisplay()) {
            hasContent = true;
        }
        mHasContent = hasContent;
    }
'''

if not method_pattern.search(updated):
    print(f"Did not find setHasContentLocked method in {path}", file=sys.stderr)
    sys.exit(1)
updated = method_pattern.sub(replacement, updated, count=1)

helper = '''    private boolean isFujisanDockedSecondaryDisplay() {
        return mDisplayId == 1
                && SystemProperties.getBoolean("ro.feature.target_dual_display", false)
                && "4".equals(SystemProperties.get("persist.vendor.fujisan.display_mode", "1"))
                && "3".equals(SystemProperties.get("persist.sys.zte.hallStatus", "3"));
    }

'''
anchor = "    /**\n     * Requests the given mode.\n"
if anchor not in updated:
    print(f"Did not find helper anchor in {path}", file=sys.stderr)
    sys.exit(1)
updated = updated.replace(anchor, helper + anchor, 1)

path.write_text(updated)
print(f"Applied fujisan docked-secondary LogicalDisplay patch to {path}")
PY
