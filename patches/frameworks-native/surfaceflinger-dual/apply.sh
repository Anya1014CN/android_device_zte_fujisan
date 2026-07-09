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

# Remove the earlier broad bring-up attempt if it is present in an already
# patched tree. It was harmless, but it did not create a SurfaceFlinger display.
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

force_hotplug = '''    // Fujisan dual-display: the secondary built-in panel can emit its
    // connect uevent before SurfaceFlinger has registered as the HWC listener.
    // Replay it only when the device is open, or when explicitly forced for debugging.
    bool forceFujisanExternalDisplayHotplug = false;
    {
        char target[PROPERTY_VALUE_MAX];
        char hall[PROPERTY_VALUE_MAX];
        char force[PROPERTY_VALUE_MAX];
        property_get("ro.feature.target_dual_display", target, "0");
        property_get("persist.sys.zte.hallStatus", hall, "1");
        property_get("persist.vendor.fujisan.force_dual_screen", force, "0");
        forceFujisanExternalDisplayHotplug =
                target[0] == '1' && ((hall[0] == '3' && hall[1] == '\\0') || force[0] == '1');
    }
    if (forceFujisanExternalDisplayHotplug) {
        ALOGI("fujisan: forcing secondary built-in display hotplug");
        onHotplugReceived(mComposerSequenceId, HWC_DISPLAY_EXTERNAL,
                HWC2::Connection::Connected, false);
    }
'''

insert_after = '''    // initialize our drawing state
    mDrawingState = mCurrentState;
'''

if insert_after not in text:
    print(f"fujisan: ERROR: could not find drawing-state initialization in {path}", file=sys.stderr)
    sys.exit(1)

if force_hotplug in text:
    print(f"fujisan: SurfaceFlinger secondary hotplug patch already present in {path}")
    sys.exit(0)

text = text.replace(insert_after, insert_after + "\n" + force_hotplug + "\n", 1)
path.write_text(text)
print(f"fujisan: SurfaceFlinger secondary hotplug patch applied to {path}")
PY
