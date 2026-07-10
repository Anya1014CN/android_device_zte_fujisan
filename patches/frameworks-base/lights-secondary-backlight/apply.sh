#!/bin/sh
set -eu

ROOT="${1:-$PWD}"
TARGET="$ROOT/frameworks/base/services/core/java/com/android/server/lights/LightsService.java"

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

updated = text

imports = {
    "import android.os.SystemProperties;\n": "import android.os.Message;\n",
    "import android.os.StrictMode;\n": "import android.os.SystemProperties;\n",
    "import java.io.FileOutputStream;\n": "import android.util.Slog;\n",
    "import java.io.IOException;\n": "import java.io.FileOutputStream;\n",
}
for new_import, anchor in imports.items():
    if new_import not in updated:
        if anchor not in updated:
            print(f"Did not find import anchor {anchor.strip()} in {path}", file=sys.stderr)
            sys.exit(1)
        updated = updated.replace(anchor, anchor + new_import, 1)

native_call_pattern = re.compile(
    r'''(                    setLight_native\(mId, color, mode, onMS, offMS,\s*'''
    r'''(?:brightnessMode|brightnessMode,\s*mBrightnessLevel)\);)\n''',
    re.S,
)
replacement = r'''\1
                    if (mId == LightsManager.LIGHT_ID_BACKLIGHT) {
                        syncFujisanSecondaryBacklight(color & 0x000000ff);
                    }
'''
if "syncFujisanSecondaryBacklight(color & 0x000000ff)" not in updated:
    if not native_call_pattern.search(updated):
        print(f"Did not find setLight_native call in {path}", file=sys.stderr)
        sys.exit(1)
    updated = native_call_pattern.sub(replacement, updated, count=1)

helper_anchor = '''    private int getVrDisplayMode() {
'''
helper = '''    private static final String FUJISAN_SECONDARY_BACKLIGHT =
            "/sys/class/leds/lcd-backlight-2/brightness";

    private boolean shouldSyncFujisanSecondaryBacklight() {
        return SystemProperties.getBoolean("ro.feature.target_dual_display", false)
                && ("2".equals(SystemProperties.get("persist.vendor.fujisan.display_mode", "1"))
                        || "4".equals(SystemProperties.get("persist.vendor.fujisan.display_mode", "1"))
                        || "8".equals(SystemProperties.get("persist.vendor.fujisan.display_mode", "1")))
                && ("3".equals(SystemProperties.get("persist.sys.zte.hallStatus", "1"))
                        || SystemProperties.getBoolean(
                                "persist.vendor.fujisan.force_dual_screen", false));
    }

    private void syncFujisanSecondaryBacklight(int brightness) {
        if (!shouldSyncFujisanSecondaryBacklight()) {
            return;
        }

        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
        try {
            try (FileOutputStream out = new FileOutputStream(FUJISAN_SECONDARY_BACKLIGHT)) {
                out.write(Integer.toString(brightness).getBytes());
                out.write(10);
            } catch (IOException e) {
                Slog.w(TAG, "Unable to sync fujisan secondary backlight", e);
            }
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

'''
helper_pattern = re.compile(
    r'''    private static final String FUJISAN_SECONDARY_BACKLIGHT =\n'''
    r'''            "/sys/class/leds/lcd-backlight-2/brightness";\n\n'''
    r'''    private boolean shouldSyncFujisanSecondaryBacklight\(\) \{\n'''
    r'''        return SystemProperties\.getBoolean\("ro\.feature\.target_dual_display", false\)\n'''
    r'''                && \("2"\.equals\(SystemProperties\.get\("persist\.vendor\.fujisan\.display_mode", "1"\)\)\n'''
    r'''                        \|\| "4"\.equals\(SystemProperties\.get\("persist\.vendor\.fujisan\.display_mode", "1"\)\)\n'''
    r'''                        \|\| "8"\.equals\(SystemProperties\.get\("persist\.vendor\.fujisan\.display_mode", "1"\)\)\)\n'''
    r'''                && \("3"\.equals\(SystemProperties\.get\("persist\.sys\.zte\.hallStatus", "1"\)\)\n'''
    r'''                        \|\| SystemProperties\.getBoolean\(\n'''
    r'''                                "persist\.vendor\.fujisan\.force_dual_screen", false\)\);\n'''
    r'''    \}\n\n'''
    r'''    private void syncFujisanSecondaryBacklight\(int brightness\) \{\n'''
    r'''        .*?'''
    r'''    \}\n\n''',
    re.S,
)
if helper_pattern.search(updated):
    updated = helper_pattern.sub(lambda _match: helper, updated, count=1)
else:
    if helper_anchor not in updated:
        print(f"Did not find helper anchor in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(helper_anchor, helper + helper_anchor, 1)

if updated == text:
    print(f"LightsService fujisan secondary-backlight patch already present in {path}")
else:
    path.write_text(updated)
    print(f"Applied fujisan secondary-backlight LightsService patch to {path}")
PY
