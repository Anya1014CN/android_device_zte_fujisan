#!/bin/sh
set -eu

ROOT="${1:-$PWD}"
TARGET="$ROOT/frameworks/base/services/core/java/com/android/server/display/LocalDisplayAdapter.java"

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

active_config_new = '''        int activeConfig = SurfaceControl.getActiveConfig(displayToken);
        if (activeConfig < 0) {
            if (SystemProperties.getBoolean("ro.feature.target_dual_display", false)
                    && builtInDisplayId != SurfaceControl.BUILT_IN_DISPLAY_ID_MAIN
                    && configs.length > 0) {
                Slog.w(TAG, "No active config found for display device " + builtInDisplayId
                        + ", forcing config 0 for dual-display bring-up");
                SurfaceControl.setActiveConfig(displayToken, 0);
                activeConfig = 0;
            } else {
                // There is no active config, and for now we don't have the
                // policy to set one.
                Slog.w(TAG, "No active config found for display device " +
                        builtInDisplayId);
                return;
            }
        }'''

secondary_info_new = '''            } else {
                final boolean isFujisanDualDisplay =
                        SystemProperties.getBoolean("ro.feature.target_dual_display", false);
                if (isFujisanDualDisplay) {
                    mInfo.type = Display.TYPE_BUILT_IN;
                    mInfo.flags |= DisplayDeviceInfo.FLAG_ROTATES_WITH_CONTENT;
                    mInfo.name = getContext().getResources().getString(
                            com.android.internal.R.string.display_manager_hdmi_display_name);
                    mInfo.touch = DisplayDeviceInfo.TOUCH_EXTERNAL;
                    mInfo.densityDpi = SystemProperties.getInt(
                            "ro.sf.lcd_density", (int) (phys.density * 160 + 0.5f));
                    mInfo.xDpi = phys.xDpi;
                    mInfo.yDpi = phys.yDpi;
                } else {
                    mInfo.type = Display.TYPE_HDMI;
                    mInfo.flags |= DisplayDeviceInfo.FLAG_PRESENTATION;
                    mInfo.name = getContext().getResources().getString(
                            com.android.internal.R.string.display_manager_hdmi_display_name);
                    mInfo.touch = DisplayDeviceInfo.TOUCH_EXTERNAL;
                    mInfo.setAssumedDensityForExternalDisplay(phys.width, phys.height);
                    // For demonstration purposes, allow rotation of the external display.
                    // In the future we might allow the user to configure this directly.
                    if ("portrait".equals(SystemProperties.get("persist.demo.hdmirotation"))) {
                        mInfo.rotation = Surface.ROTATION_270;
                    }
                    // For demonstration purposes, allow rotation of the external display
                    // to follow the built-in display.
                    if (SystemProperties.getBoolean("persist.demo.hdmirotates", false)) {
                        mInfo.flags |= DisplayDeviceInfo.FLAG_ROTATES_WITH_CONTENT;
                    }

                    if (!res.getBoolean(
                                com.android.internal.R.bool.config_localDisplaysMirrorContent)) {
                        mInfo.flags |= DisplayDeviceInfo.FLAG_OWN_CONTENT_ONLY;
                    }
                }
            }'''

updated = text
active_applied = False
secondary_applied = False

if active_config_new not in updated:
    active_config_pattern = re.compile(
        r'''int activeConfig = SurfaceControl\.getActiveConfig\(displayToken\);\s*'''
        r'''if \(activeConfig < 0\) \{.*?'''
        r'''Slog\.w\(TAG, "No active config found for display device " \+\s*'''
        r'''builtInDisplayId\);\s*'''
        r'''return;\s*\}\s*'''
        r'''int activeColorMode = SurfaceControl\.getActiveColorMode\(displayToken\);''',
        re.S,
    )
    if not active_config_pattern.search(updated):
        print(f"Did not find expected active-config block in {path}", file=sys.stderr)
        sys.exit(1)
    updated = active_config_pattern.sub(
        active_config_new + '\n        int activeColorMode = SurfaceControl.getActiveColorMode(displayToken);',
        updated,
        count=1,
    )
    active_applied = True
elif active_config_new in text:
    active_applied = True

if secondary_info_new not in updated:
    secondary_info_pattern = re.compile(
        r'''\} else \{\s*'''
        r'''mInfo\.type = Display\.TYPE_HDMI;.*?'''
        r'''if \(SystemProperties\.getBoolean\("persist\.demo\.hdmirotates", false\)\) \{\s*'''
        r'''mInfo\.flags \|= DisplayDeviceInfo\.FLAG_ROTATES_WITH_CONTENT;\s*'''
        r'''\}\s*'''
        r'''(?:if \(!res\.getBoolean\(\s*'''
        r'''com\.android\.internal\.R\.bool\.config_localDisplaysMirrorContent\)\) \{\s*'''
        r'''mInfo\.flags \|= DisplayDeviceInfo\.FLAG_OWN_CONTENT_ONLY;\s*'''
        r'''\}\s*)?'''
        r'''\}''',
        re.S,
    )
    if secondary_info_pattern.search(updated):
        updated = secondary_info_pattern.sub(secondary_info_new, updated, count=1)
        secondary_applied = True
    else:
        print(f"Warning: did not find secondary-display block in {path}; keeping active-config patch only")
elif secondary_info_new in updated:
    secondary_applied = True

if updated == text:
    print(f"LocalDisplayAdapter dual-display compatibility already updated in {path}")
    sys.exit(0)

path.write_text(updated)
if active_applied and secondary_applied:
    print(f"Applied full fujisan dual-display LocalDisplayAdapter compatibility to {path}")
elif active_applied:
    print(f"Applied active-config-only fujisan dual-display compatibility to {path}")
else:
    print(f"Applied fujisan dual-display LocalDisplayAdapter compatibility to {path}")
PY
