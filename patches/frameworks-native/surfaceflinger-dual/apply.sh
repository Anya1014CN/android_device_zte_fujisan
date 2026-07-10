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
    print(f"fujisan: SurfaceFlinger secondary hotplug patch disabled in {path}")
else:
    path.write_text(text)
    print(f"fujisan: removed unsafe SurfaceFlinger secondary hotplug patch from {path}")
PY
