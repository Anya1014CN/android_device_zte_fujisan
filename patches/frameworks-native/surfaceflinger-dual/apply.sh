#!/bin/sh
set -eu

ROOT="${1:-$PWD}"
TARGET="$ROOT/frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp"

if [ ! -f "$TARGET" ]; then
    echo "fujisan: ERROR: $TARGET not found" >&2
    exit 1
fi

python3 - "$TARGET" <<'PY'
from pathlib import Path
import re
import sys

path = Path(sys.argv[1])
text = path.read_text()

# Remove earlier broad bring-up attempts if they are present in an already
# patched tree. SurfaceFlinger must not synthesize hotplug events during boot:
# doing so can race default display publication and leave system_server stuck
# in DisplayManagerService.waitForDefaultDisplay().
text = re.sub(
    r'''\n    // Fujisan dual-display: bypass isConnected check for built-in external panel\.\n'''
    r'''    bool fujisanDualDisplay = false;\n'''
    r'''    \{\n'''
    r'''        char p\[PROPERTY_VALUE_MAX\];\n'''
    r'''        property_get\("ro\.feature\.target_dual_display", p, "0"\);\n'''
    r'''        fujisanDualDisplay = \(p\[0\] == '1'\);\n'''
    r'''    \}\n''',
    "\n",
    text,
)

text = re.sub(
    r'''\n    // Fujisan dual-display: the secondary built-in panel can emit its\n'''
    r'''    // connect uevent before SurfaceFlinger has registered as the HWC listener\.\n'''
    r'''    // Replay the external hotplug once so SF creates a real DisplayDevice for it\.\n'''
    r'''    bool forceFujisanExternalDisplayHotplug = false;\n'''
    r'''    \{\n'''
    r'''        char p\[PROPERTY_VALUE_MAX\];\n'''
    r'''        property_get\("ro\.feature\.target_dual_display", p, "0"\);\n'''
    r'''        forceFujisanExternalDisplayHotplug = \(p\[0\] == '1'\);\n'''
    r'''    \}\n'''
    r'''    if \(forceFujisanExternalDisplayHotplug\) \{\n'''
    r'''        ALOGI\("fujisan: forcing secondary built-in display hotplug"\);\n'''
    r'''        onHotplugReceived\(mComposerSequenceId, HWC_DISPLAY_EXTERNAL,\n'''
    r'''                HWC2::Connection::Connected, false\);\n'''
    r'''    \}\n''',
    "\n",
    text,
)

if text == path.read_text():
    pass
else:
    path.write_text(text)
    print(f"fujisan: removed unsafe SurfaceFlinger secondary hotplug patch from {path}")
    text = path.read_text()

cleanup = '''        if (connection == HWC2::Connection::Connected) {
            bool fujisanDualDisplay = false;
            {
                char target[PROPERTY_VALUE_MAX];
                property_get("ro.feature.target_dual_display", target, "0");
                fujisanDualDisplay = target[0] == '1';
            }
            if (fujisanDualDisplay) {
                const sp<IBinder> oldToken = mBuiltinDisplays[type];
                const wp<IBinder> oldDisplay(oldToken);
                if (oldToken != nullptr && mDisplays.indexOfKey(oldDisplay) < 0) {
                    ALOGI("fujisan: dropping stale secondary display token before hotplug replay");
                    mCurrentState.displays.removeItem(oldDisplay);
                    mDrawingState.displays.removeItem(oldDisplay);
                    mBuiltinDisplays[type].clear();
                }
            }
            createBuiltinDisplayLocked(type);
'''

old = '''        if (connection == HWC2::Connection::Connected) {
            createBuiltinDisplayLocked(type);
'''

if cleanup in text:
    print(f"fujisan: SurfaceFlinger stale secondary token cleanup already present in {path}")
    sys.exit(0)

if old not in text:
    print(f"fujisan: ERROR: could not find external hotplug connect block in {path}", file=sys.stderr)
    sys.exit(1)

text = text.replace(old, cleanup, 1)
path.write_text(text)
print(f"fujisan: SurfaceFlinger stale secondary token cleanup applied to {path}")
PY
