#!/bin/sh
set -eu

ROOT="${1:-$PWD}"
MANIFEST="$ROOT/packages/apps/Trebuchet/AndroidManifest.xml"
SECONDARY="$ROOT/packages/apps/Trebuchet/src/com/android/launcher3/searchlauncher/SecondarySearchLauncher.java"

if [ ! -f "$MANIFEST" ]; then
  echo "Missing target file: $MANIFEST" >&2
  exit 1
fi

mkdir -p "$(dirname "$SECONDARY")"

python3 - "$MANIFEST" "$SECONDARY" <<'PY'
from pathlib import Path
import sys

manifest = Path(sys.argv[1])
secondary = Path(sys.argv[2])

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

secondary_source = '''package com.android.launcher3.searchlauncher;

public class SecondarySearchLauncher extends SearchLauncher {
}
'''

if secondary.exists() and secondary.read_text() == secondary_source:
    print(f"Fujisan secondary Trebuchet class already present in {secondary}")
else:
    secondary.write_text(secondary_source)
    print(f"Added Fujisan secondary Trebuchet class in {secondary}")
PY
