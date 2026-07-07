#!/bin/sh
set -eu

ROOT="${1:-$PWD}"
TARGET="$ROOT/frameworks/base/core/java/com/android/internal/policy/PhoneFallbackEventHandler.java"

if [ ! -f "$TARGET" ]; then
  echo "Missing target file: $TARGET" >&2
  exit 1
fi

python3 - "$TARGET" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()

import_old = "import android.provider.Settings;\n"
import_new = "import android.provider.MediaStore;\nimport android.provider.Settings;\n"

camera_up_old = """            case KeyEvent.KEYCODE_CAMERA: {
                if (getKeyguardManager().inKeyguardRestrictedInputMode()) {
                    break;
                }
                if (event.isTracking() && !event.isCanceled()) {
                    // Add short press behavior here if desired
                }
                return true;
            }"""

camera_up_new = """            case KeyEvent.KEYCODE_CAMERA: {
                if (getKeyguardManager().inKeyguardRestrictedInputMode()) {
                    break;
                }
                if (event.isTracking() && !event.isCanceled()) {
                    if (isUserSetupComplete()) {
                        Intent intent = new Intent(MediaStore.INTENT_ACTION_STILL_IMAGE_CAMERA);
                        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                        try {
                            sendCloseSystemWindows();
                            mContext.startActivity(intent);
                        } catch (ActivityNotFoundException e) {
                            Log.w(TAG, \"No camera activity found for camera key short press.\", e);
                        }
                    } else {
                        Log.i(TAG, \"Not starting camera activity because user setup is in progress.\");
                    }
                }
                return true;
            }"""

updated = text

if "import android.provider.MediaStore;" not in updated:
    if import_old not in updated:
        print(f"Did not find expected provider import block in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(import_old, import_new, 1)

if camera_up_new not in updated:
    if camera_up_old not in updated:
        print(f"Did not find expected camera key up block in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(camera_up_old, camera_up_new, 1)

if updated == text:
    print(f"Camera key short-press compatibility already updated in {path}")
    sys.exit(0)

path.write_text(updated)
print(f"Enabled camera-key short press launch in {path}")
PY
