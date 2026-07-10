#!/bin/sh
set -eu

ROOT="${1:-$PWD}"
MANIFEST="$ROOT/packages/apps/Trebuchet/AndroidManifest.xml"
SEARCH="$ROOT/packages/apps/Trebuchet/src/com/android/launcher3/searchlauncher/SearchLauncher.java"
SECONDARY="$ROOT/packages/apps/Trebuchet/src/com/android/launcher3/searchlauncher/SecondarySearchLauncher.java"

if [ ! -f "$MANIFEST" ]; then
  echo "Missing target file: $MANIFEST" >&2
  exit 1
fi

if [ ! -f "$SEARCH" ]; then
  echo "Missing target file: $SEARCH" >&2
  exit 1
fi

rm -f "$SECONDARY"

python3 - "$MANIFEST" "$SEARCH" <<'PY'
from pathlib import Path
import sys

manifest = Path(sys.argv[1])
search = Path(sys.argv[2])

text = manifest.read_text()
updated = text

activity = '''        <activity
            android:name="com.android.launcher3.searchlauncher.SecondarySearchLauncher"
            android:launchMode="singleTask"
            android:clearTaskOnLaunch="true"
            android:stateNotNeeded="true"
            android:windowSoftInputMode="adjustPan"
            android:screenOrientation="nosensor"
            android:configChanges="keyboard|keyboardHidden|navigation"
            android:resizeableActivity="true"
            android:resumeWhilePausing="true"
            android:taskAffinity="org.lineageos.trebuchet.secondary"
            android:excludeFromRecents="true"
            android:enabled="true"
            android:exported="true" />

'''

anchor = '''        <!--
        The settings activity. When extending keep the intent filter present
        -->
'''

if activity not in updated:
    if anchor not in updated:
        print(f"Did not find Trebuchet settings activity anchor in {manifest}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(anchor, activity + anchor, 1)

if updated != text:
    manifest.write_text(updated)
    print(f"Added Fujisan secondary Trebuchet activity in {manifest}")
else:
    print(f"Fujisan secondary Trebuchet activity already present in {manifest}")

search_text = search.read_text()
search_updated = search_text

secondary_class = '''
class SecondarySearchLauncher extends SearchLauncher {
}
'''

if secondary_class not in search_updated:
    if not search_updated.endswith('\n'):
        search_updated += '\n'
    search_updated += secondary_class
    search.write_text(search_updated)
    print(f"Added Fujisan secondary Trebuchet class in {search}")
else:
    print(f"Fujisan secondary Trebuchet class already present in {search}")
PY
