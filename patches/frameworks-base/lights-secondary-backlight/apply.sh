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

if "syncFujisanSecondaryBacklight" in text:
    print(f"LightsService fujisan secondary-backlight patch already present in {path}")
    sys.exit(0)

updated = text

imports = {
    "import android.os.SystemProperties;\n": "import android.os.Message;\n",
    "import java.io.FileOutputStream;\n": "import android.util.Slog;\n",
    "import java.io.IOException;\n": "import java.io.FileOutputStream;\n",
}
for new_import, anchor in imports.items():
    if new_import not in updated:
        if anchor not in updated:
            print(f"Did not find import anchor {anchor.strip()} in {path}", file=sys.stderr)
            sys.exit(1)
        updated = updated.replace(anchor, anchor + new_import, 1)

native_call = '''                    setLight_native(mId, color, mode, onMS, offMS, brightnessMode);
'''
replacement = native_call + '''                    if (mId == LightsManager.LIGHT_ID_BACKLIGHT) {
                        syncFujisanSecondaryBacklight(color & 0x000000ff);
                    }
'''
if native_call not in updated:
    print(f"Did not find setLight_native call in {path}", file=sys.stderr)
    sys.exit(1)
updated = updated.replace(native_call, replacement, 1)

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

        try (FileOutputStream out = new FileOutputStream(FUJISAN_SECONDARY_BACKLIGHT)) {
            out.write(Integer.toString(brightness).getBytes());
            out.write('\\n');
        } catch (IOException e) {
            Slog.w(TAG, "Unable to sync fujisan secondary backlight", e);
        }
    }

'''
if helper_anchor not in updated:
    print(f"Did not find helper anchor in {path}", file=sys.stderr)
    sys.exit(1)
updated = updated.replace(helper_anchor, helper + helper_anchor, 1)

path.write_text(updated)
print(f"Applied fujisan secondary-backlight LightsService patch to {path}")
PY
