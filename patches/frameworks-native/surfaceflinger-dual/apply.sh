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

# We add a synthetic hotplug for the external display (displayId=1)
# during SurfaceFlinger initialization when ro.feature.target_dual_display=1.
# This bypasses the need for HWC to send a hotplug event for the second panel.

new_code = '''
    // Fujisan dual-display: synthetically hotplug the external display
    // so SurfaceFlinger creates a DisplayDevice for the second panel
    // without waiting for an HWC hotplug event that never fires.
    {
        char dualProp[PROPERTY_VALUE_MAX];
        property_get("ro.feature.target_dual_display", dualProp, "0");
        if (dualProp[0] == '1') {
            onHotplugReceived(0, HWC_DISPLAY_EXTERNAL,
                static_cast<hwc2_connection_t>(HWC2::Connection::Connected),
                false);
        }
    }
'''

if "ro.feature.target_dual_display" in text:
    print(f"fujisan: dual-display hotplug already present in {path}")
    sys.exit(0)

applied = False

# Option 1: Insert after HWC callback registration in init()
# Look for "registerCallback" call in init()
m = re.search(
    r'(mHwc->registerCallback\(\s*\w+\s*,\s*\w+\s*\)\s*[;,])(.*?)(\s*//.*?\n|\s*\n)',
    text,
    re.DOTALL
)
if m:
    pos = m.end()
    text = text[:pos] + new_code + text[pos:]
    applied = True
    print(f"fujisan: inserted dual-display hotplug after HWC callback registration")

# Option 2: If registerCallback pattern doesn't match, try after getHwComposer().registerCallback
if not applied:
    m = re.search(
        r'(getHwComposer\(\)\.registerCallback\(\s*\w+\s*,\s*\w+\s*\))',
        text
    )
    if m:
        pos = text.find(';', m.end())
        if pos >= 0:
            text = text[:pos+1] + new_code + text[pos+1:]
            applied = True
            print(f"fujisan: inserted dual-display hotplug after registerCallback")

# Option 3: Insert at the end of init() before the closing brace
if not applied:
    m = re.search(
        r'(void SurfaceFlinger::init\(\).*?)(\n\})',
        text,
        re.DOTALL
    )
    if m:
        pos = m.start(2)
        text = text[:pos] + new_code + text[pos:]
        applied = True
        print(f"fujisan: inserted dual-display hotplug at end of init()")

# Option 4: Insert in processDisplayChangesLocked
if not applied:
    m = re.search(
        r'(void SurfaceFlinger::processDisplayChangesLocked\(\)).*?(\{)',
        text,
        re.DOTALL
    )
    if m:
        pos = m.end(2)
        text = text[:pos] + new_code + text[pos:]
        applied = True
        print(f"fujisan: inserted dual-display hotplug in processDisplayChangesLocked")

if applied:
    path.write_text(text)
    print(f"fujisan: SurfaceFlinger dual-display patch applied")
else:
    print(f"fujisan: WARNING: could not find insertion point - SurfaceFlinger.cpp may have changed", file=sys.stderr)
    sys.exit(0)

PY
