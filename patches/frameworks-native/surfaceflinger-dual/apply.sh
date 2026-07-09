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
import re, sys

path = Path(sys.argv[1])
text = path.read_text()

# Already patched?
if 'fujisanDualDisplay' in text:
    print(f"fujisan: dual-display patch already present in {path}")
    sys.exit(0)

# Remove any stale broken code from previous attempts
for stale in [
    'onHotplugReceived(0, HWC_DISPLAY_EXTERNAL,',
    'onHotplugReceived(HWC_DISPLAY_EXTERNAL,',
]:
    if stale in text:
        i = text.find(stale)
        # Find the enclosing block
        start = text.rfind('{', 0, i)
        if start >= 0:
            depth = 1
            end = start + 1
            while depth > 0 and end < len(text):
                if text[end] == '{': depth += 1
                elif text[end] == '}': depth -= 1
                end += 1
            block = text[start:end]
            if 'ro.feature.target_dual_display' in block:
                text = text[:start] + text[end:]
                print(f"fujisan: removed stale hotplug block")

# Find processDisplayChangesLocked and insert the dual-display
# property check at the beginning of the function.
# Also find isConnected() calls and override them.

# Strategy 1: Add a local variable at the top of processDisplayChangesLocked
add_var = '''    // Fujisan dual-display: bypass isConnected check for built-in external panel.
    bool fujisanDualDisplay = false;
    {
        char p[PROPERTY_VALUE_MAX];
        property_get("ro.feature.target_dual_display", p, "0");
        fujisanDualDisplay = (p[0] == '1');
    }
'''

# Find processDisplayChangesLocked body start
m = re.search(r'(void SurfaceFlinger::processDisplayChangesLocked\(\)\s*\{)', text)
if not m:
    print(f"fujisan: ERROR: processDisplayChangesLocked not found", file=sys.stderr)
    sys.exit(1)

# Find first statement after opening brace
body_start = m.end()
# Skip comments and blank lines to find first real code
pos = body_start
while pos < len(text) and text[pos] in ' \t\n\r':
    pos += 1
# Skip // comments
if pos < len(text) and text[pos:pos+2] == '//':
    pos = text.find('\n', pos) + 1

# Insert variable after the opening brace but before first real code
text = text[:body_start] + '\n' + add_var + '\n' + text[body_start:]
applied = True
print(f"fujisan: inserted fujisanDualDisplay variable in processDisplayChangesLocked")

# Strategy 2: Modify isConnected() to include our override
# Typically: getHwComposer().isConnected(displayToken)
# Change to: (getHwComposer().isConnected(displayToken) || fujisanDualDisplay)
override_count = 0
for pattern in [
    r'getHwComposer\(\)\.isConnected\((\w+)\)',
    r'mHwc->isConnected\((\w+)\)',
]:
    matches = list(re.finditer(pattern, text))
    for m in matches:
        if 'fujisanDualDisplay' not in text[m.start()-50:m.end()+50]:
            old = m.group(0)
            new = f'({old} || fujisanDualDisplay)'
            text = text[:m.start()] + new + text[m.end():]
            override_count += 1
            print(f"fujisan: modified isConnected call: {old}")

if override_count == 0:
    print(f"fujisan: WARNING: no isConnected calls found to override")
    print(f"fujisan: The external display will not be auto-connected.")
    print(f"fujisan: Check SurfaceFlinger.cpp for the correct connection check pattern.")
else:
    print(f"fujisan: overrode {override_count} isConnected call(s)")

path.write_text(text)
print(f"fujisan: SurfaceFlinger dual-display patch applied")

PY
