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

if replacement not in updated:
    if not method_pattern.search(updated):
        print(f"Did not find setHasContentLocked method in {path}", file=sys.stderr)
        sys.exit(1)
    updated = method_pattern.sub(replacement, updated, count=1)

has_content_pattern = re.compile(
    r'''(    public boolean hasContentLocked\(\) \{\n)'''
    r'''(        return mHasContent;\n)'''
    r'''(    \}\n)'''
)
has_content_replacement = '''    public boolean hasContentLocked() {
        return mHasContent || isFujisanDockedSecondaryDisplay();
    }
'''

if has_content_replacement not in updated:
    if not has_content_pattern.search(updated):
        print(f"Did not find hasContentLocked method in {path}", file=sys.stderr)
        sys.exit(1)
    updated = has_content_pattern.sub(has_content_replacement, updated, count=1)

info_anchor = '''            mBaseDisplayInfo.ownerUid = deviceInfo.ownerUid;
            mBaseDisplayInfo.ownerPackageName = deviceInfo.ownerPackageName;

            mPrimaryDisplayDeviceInfo = deviceInfo;
'''
info_replacement = '''            mBaseDisplayInfo.ownerUid = deviceInfo.ownerUid;
            mBaseDisplayInfo.ownerPackageName = deviceInfo.ownerPackageName;

            applyFujisanDisplayInfoLocked();

            mPrimaryDisplayDeviceInfo = deviceInfo;
'''
if "applyFujisanDisplayInfoLocked();" not in updated:
    if info_anchor not in updated:
        print(f"Did not find DisplayInfo update anchor in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(info_anchor, info_replacement, 1)

projection_anchor = '''        int displayRectTop = (physHeight - displayRectHeight) / 2;
        int displayRectLeft = (physWidth - displayRectWidth) / 2;
        mTempDisplayRect.set(displayRectLeft, displayRectTop,
                displayRectLeft + displayRectWidth, displayRectTop + displayRectHeight);

        mTempDisplayRect.left += mDisplayOffsetX;
'''
projection_replacement = '''        int displayRectTop = (physHeight - displayRectHeight) / 2;
        int displayRectLeft = (physWidth - displayRectWidth) / 2;
        mTempDisplayRect.set(displayRectLeft, displayRectTop,
                displayRectLeft + displayRectWidth, displayRectTop + displayRectHeight);

        adjustFujisanDisplayProjectionLocked(displayInfo, orientation);

        mTempDisplayRect.left += mDisplayOffsetX;
'''
if "adjustFujisanDisplayProjectionLocked(displayInfo, orientation);" not in updated:
    if projection_anchor not in updated:
        print(f"Did not find display projection anchor in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(projection_anchor, projection_replacement, 1)

helper = '''    private static final int FUJISAN_MODE_SINGLE = 1;
    private static final int FUJISAN_MODE_ZOOM = 2;
    private static final int FUJISAN_MODE_DOCKED = 4;

    private void applyFujisanDisplayInfoLocked() {
        if (!isFujisanDualDisplayTarget()) {
            return;
        }

        final int mode = getFujisanDisplayMode();
        if (mode == FUJISAN_MODE_ZOOM) {
            setFujisanLogicalSize(2160, 1920);
        } else if (mode == FUJISAN_MODE_DOCKED || mode == FUJISAN_MODE_SINGLE) {
            setFujisanLogicalSize(1080, 1920);
        }
    }

    private void setFujisanLogicalSize(int width, int height) {
        mBaseDisplayInfo.appWidth = width;
        mBaseDisplayInfo.appHeight = height;
        mBaseDisplayInfo.logicalWidth = width;
        mBaseDisplayInfo.logicalHeight = height;
        mBaseDisplayInfo.smallestNominalAppWidth = width;
        mBaseDisplayInfo.smallestNominalAppHeight = height;
        mBaseDisplayInfo.largestNominalAppWidth = width;
        mBaseDisplayInfo.largestNominalAppHeight = height;
    }

    private void adjustFujisanDisplayProjectionLocked(DisplayInfo displayInfo, int orientation) {
        if (!isFujisanDualDisplayTarget()) {
            return;
        }

        final int mode = getFujisanDisplayMode();
        if (mode != FUJISAN_MODE_DOCKED && mode != FUJISAN_MODE_SINGLE) {
            return;
        }

        if (orientation == Surface.ROTATION_90 || orientation == Surface.ROTATION_270) {
            mTempLayerStackRect.set(0, 0, displayInfo.logicalWidth, displayInfo.logicalHeight);
            mTempDisplayRect.set(0, 0, displayInfo.logicalHeight, displayInfo.logicalWidth);
        } else {
            mTempLayerStackRect.set(0, 0, displayInfo.logicalWidth, displayInfo.logicalHeight);
            mTempDisplayRect.set(0, 0, displayInfo.logicalWidth, displayInfo.logicalHeight);
        }
    }

    private boolean isFujisanDockedSecondaryDisplay() {
        return mDisplayId == 1
                && isFujisanDualDisplayTarget()
                && getFujisanDisplayMode() == FUJISAN_MODE_DOCKED
                && ("3".equals(SystemProperties.get("persist.sys.zte.hallStatus", "1"))
                        || SystemProperties.getBoolean(
                                "persist.vendor.fujisan.force_dual_screen", false));
    }

    private static boolean isFujisanDualDisplayTarget() {
        return SystemProperties.getBoolean("ro.feature.target_dual_display", false);
    }

    private static int getFujisanDisplayMode() {
        return SystemProperties.getInt("persist.vendor.fujisan.display_mode",
                FUJISAN_MODE_SINGLE);
    }

'''
helper_pattern = re.compile(
    r'''(?:    private static final int FUJISAN_MODE_SINGLE = 1;\n'''
    r'''    .*?'''
    r'''    private static int getFujisanDisplayMode\(\) \{\n'''
    r'''        .*?'''
    r'''    \}\n\n'''
    r'''|    private boolean isFujisanDockedSecondaryDisplay\(\) \{\n'''
    r'''        return mDisplayId == 1\n'''
    r'''                && SystemProperties\.getBoolean\("ro\.feature\.target_dual_display", false\)\n'''
    r'''                && "4"\.equals\(SystemProperties\.get\("persist\.vendor\.fujisan\.display_mode", "1"\)\)\n'''
    r'''                && .*?\n'''
    r'''    \}\n\n)''',
    re.S,
)
anchor = "    /**\n     * Requests the given mode.\n"
if helper not in updated:
    if helper_pattern.search(updated):
        updated = helper_pattern.sub(lambda _match: helper, updated, count=1)
    elif anchor not in updated:
        print(f"Did not find helper anchor in {path}", file=sys.stderr)
        sys.exit(1)
    else:
        updated = updated.replace(anchor, helper + anchor, 1)

if updated == text:
    print(f"LogicalDisplay fujisan docked-secondary patch already present in {path}")
else:
    path.write_text(updated)
    print(f"Applied fujisan docked-secondary LogicalDisplay patch to {path}")
PY
