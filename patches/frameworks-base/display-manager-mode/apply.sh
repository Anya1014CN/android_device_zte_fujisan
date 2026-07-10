#!/bin/sh
set -eu

ROOT="${1:-$PWD}"
TARGET="$ROOT/frameworks/base/services/core/java/com/android/server/display/DisplayManagerService.java"

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

imports = {
    "import android.database.ContentObserver;\n": "import android.content.res.Resources;\n",
    "import android.provider.Settings;\n": "import android.os.ServiceManager;\n",
}
for new_import, anchor in imports.items():
    if new_import not in updated:
        if anchor not in updated:
            print(f"Did not find import anchor {anchor.strip()} in {path}", file=sys.stderr)
            sys.exit(1)
        updated = updated.replace(anchor, anchor + new_import, 1)

updated = re.sub(
    r'''\n    private final ContentObserver mFujisanDisplayModeObserver =\n'''
    r'''            new ContentObserver\(mHandler\) \{\n'''
    r'''                @Override\n'''
    r'''                public void onChange\(boolean selfChange\) \{\n'''
    r'''                    synchronized \(mSyncRoot\) \{\n'''
    r'''                        scheduleTraversalLocked\(false\);\n'''
    r'''                    \}\n'''
    r'''                \}\n'''
    r'''            \};\n''',
    "\n    private ContentObserver mFujisanDisplayModeObserver;\n",
    updated,
)

field_anchor = '''    private final DisplayViewport mTempDefaultViewport = new DisplayViewport();
    private final DisplayViewport mTempExternalTouchViewport = new DisplayViewport();
    private final ArrayList<DisplayViewport> mTempVirtualTouchViewports = new ArrayList<>();
'''
fields = '''    private final DisplayViewport mTempDefaultViewport = new DisplayViewport();
    private final DisplayViewport mTempExternalTouchViewport = new DisplayViewport();
    private final ArrayList<DisplayViewport> mTempVirtualTouchViewports = new ArrayList<>();

    private static final int FUJISAN_MODE_SINGLE = 1;
    private static final int FUJISAN_MODE_ZOOM = 2;
    private static final int FUJISAN_MODE_DOCKED = 4;
    private static final int FUJISAN_SINGLE_A = 0;
    private static final int FUJISAN_SINGLE_B = 1;

    private ContentObserver mFujisanDisplayModeObserver;
'''
if "FUJISAN_MODE_DOCKED" not in updated:
    if field_anchor not in updated:
        print(f"Did not find viewport field anchor in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(field_anchor, fields, 1)

constructor_anchor = '''        mHandler = new DisplayManagerHandler(DisplayThread.get().getLooper());
        mUiHandler = UiThread.getHandler();
        mDisplayAdapterListener = new DisplayAdapterListener();
'''
constructor_replacement = '''        mHandler = new DisplayManagerHandler(DisplayThread.get().getLooper());
        mUiHandler = UiThread.getHandler();
        mFujisanDisplayModeObserver = new ContentObserver(mHandler) {
            @Override
            public void onChange(boolean selfChange) {
                synchronized (mSyncRoot) {
                    scheduleTraversalLocked(false);
                }
            }
        };
        mDisplayAdapterListener = new DisplayAdapterListener();
'''
if "mFujisanDisplayModeObserver = new ContentObserver(mHandler)" not in updated:
    if constructor_anchor not in updated:
        print(f"Did not find constructor handler anchor in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(constructor_anchor, constructor_replacement, 1)

system_ready_old = '''    public void systemReady(boolean safeMode, boolean onlyCore) {
        synchronized (mSyncRoot) {
            mSafeMode = safeMode;
            mOnlyCore = onlyCore;
        }

        mHandler.sendEmptyMessage(MSG_REGISTER_ADDITIONAL_DISPLAY_ADAPTERS);
    }
'''
system_ready_new = '''    public void systemReady(boolean safeMode, boolean onlyCore) {
        synchronized (mSyncRoot) {
            mSafeMode = safeMode;
            mOnlyCore = onlyCore;
        }

        if (isFujisanDualDisplayTarget()) {
            mContext.getContentResolver().registerContentObserver(
                    Settings.System.getUriFor("display_mode"), false,
                    mFujisanDisplayModeObserver);
            mContext.getContentResolver().registerContentObserver(
                    Settings.System.getUriFor("displayid_single_mode"), false,
                    mFujisanDisplayModeObserver);
            mContext.getContentResolver().registerContentObserver(
                    Settings.System.getUriFor("hallC_display_mode"), false,
                    mFujisanDisplayModeObserver);
        }

        mHandler.sendEmptyMessage(MSG_REGISTER_ADDITIONAL_DISPLAY_ADAPTERS);
    }
'''
if system_ready_new not in updated:
    if system_ready_old not in updated:
        print(f"Did not find systemReady block in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(system_ready_old, system_ready_new, 1)

configure_pattern = re.compile(
    r'''    private void configureDisplayInTransactionLocked\(DisplayDevice device\) \{\n'''
    r'''        final DisplayDeviceInfo info = device\.getDisplayDeviceInfoLocked\(\);\n'''
    r'''        final boolean ownContent = \(info\.flags & DisplayDeviceInfo\.FLAG_OWN_CONTENT_ONLY\) != 0;\n\n'''
    r'''        // Find the logical display that the display device is showing\.\n'''
    r'''        // Certain displays only ever show their own content\.\n'''
    r'''        LogicalDisplay display = findLogicalDisplayForDeviceLocked\(device\);\n'''
    r'''        if \(!ownContent\) \{\n'''
    r'''            if \(display != null && !display\.hasContentLocked\(\)\) \{\n'''
    r'''                // If the display does not have any content of its own, then\n'''
    r'''                // automatically mirror the default logical display contents\.\n'''
    r'''                display = null;\n'''
    r'''            \}\n'''
    r'''            if \(display == null\) \{\n'''
    r'''                display = mLogicalDisplays\.get\(Display\.DEFAULT_DISPLAY\);\n'''
    r'''            \}\n'''
    r'''        \}\n''',
    re.M,
)
configure_replacement = '''    private void configureDisplayInTransactionLocked(DisplayDevice device) {
        final DisplayDeviceInfo info = device.getDisplayDeviceInfoLocked();
        final boolean ownContent = (info.flags & DisplayDeviceInfo.FLAG_OWN_CONTENT_ONLY) != 0;

        // Find the logical display that the display device is showing.
        // Certain displays only ever show their own content.
        LogicalDisplay display = findLogicalDisplayForDeviceLocked(device);
        if (isFujisanDualDisplayTarget()) {
            display = chooseFujisanLogicalDisplayLocked(device, display, info, ownContent);
            if (display == null) {
                return;
            }
        } else if (!ownContent) {
            if (display != null && !display.hasContentLocked()) {
                // If the display does not have any content of its own, then
                // automatically mirror the default logical display contents.
                display = null;
            }
            if (display == null) {
                display = mLogicalDisplays.get(Display.DEFAULT_DISPLAY);
            }
        }
'''
if "chooseFujisanLogicalDisplayLocked" not in updated:
    if not configure_pattern.search(updated):
        print(f"Did not find configureDisplayInTransactionLocked routing block in {path}", file=sys.stderr)
        sys.exit(1)
    updated = configure_pattern.sub(configure_replacement, updated, count=1)

external_viewport_old = '''        if (!mExternalTouchViewport.valid
                && info.touch == DisplayDeviceInfo.TOUCH_EXTERNAL) {
            setViewportLocked(mExternalTouchViewport, display, device);
        }
'''
external_viewport_new = '''        if (!mExternalTouchViewport.valid
                && info.touch == DisplayDeviceInfo.TOUCH_EXTERNAL) {
            setViewportLocked(mExternalTouchViewport, display, device);
            adjustFujisanExternalTouchViewportLocked(mExternalTouchViewport);
        }
'''
if external_viewport_new not in updated:
    if external_viewport_old not in updated:
        print(f"Did not find external viewport block in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(external_viewport_old, external_viewport_new, 1)

helper_anchor = '''    private LogicalDisplay findLogicalDisplayForDeviceLocked(DisplayDevice device) {
'''
helpers = '''    private LogicalDisplay chooseFujisanLogicalDisplayLocked(DisplayDevice device,
            LogicalDisplay ownDisplay, DisplayDeviceInfo info, boolean ownContent) {
        final boolean isDefaultDevice =
                (info.flags & DisplayDeviceInfo.FLAG_DEFAULT_DISPLAY) != 0;
        final boolean isSecondaryDevice = info.touch == DisplayDeviceInfo.TOUCH_EXTERNAL;
        final int mode = getFujisanDisplayMode();

        if (mode == FUJISAN_MODE_SINGLE) {
            final int single = getFujisanSingleDisplay();
            if (isDefaultDevice && single == FUJISAN_SINGLE_B) {
                blankFujisanDisplayDeviceLocked(device, ownDisplay);
                return null;
            }
            if (isSecondaryDevice) {
                if (single == FUJISAN_SINGLE_A) {
                    blankFujisanDisplayDeviceLocked(device, ownDisplay);
                    return null;
                }
                return mLogicalDisplays.get(Display.DEFAULT_DISPLAY);
            }
        } else if (mode == FUJISAN_MODE_ZOOM && isSecondaryDevice) {
            return mLogicalDisplays.get(Display.DEFAULT_DISPLAY);
        } else if (mode == FUJISAN_MODE_DOCKED && isSecondaryDevice) {
            if (ownDisplay != null) {
                return ownDisplay;
            }
            return mLogicalDisplays.get(1);
        }

        if (!ownContent) {
            if (ownDisplay != null && !ownDisplay.hasContentLocked()) {
                ownDisplay = null;
            }
            if (ownDisplay == null) {
                ownDisplay = mLogicalDisplays.get(Display.DEFAULT_DISPLAY);
            }
        }
        return ownDisplay;
    }

    private void blankFujisanDisplayDeviceLocked(DisplayDevice device, LogicalDisplay ownDisplay) {
        LogicalDisplay display = ownDisplay != null
                ? ownDisplay : mLogicalDisplays.get(Display.DEFAULT_DISPLAY);
        if (display != null) {
            display.configureDisplayInTransactionLocked(device, true);
        }
    }

    private void adjustFujisanExternalTouchViewportLocked(DisplayViewport viewport) {
        if (!isFujisanDualDisplayTarget() || !viewport.valid) {
            return;
        }

        final int mode = getFujisanDisplayMode();
        if (mode == FUJISAN_MODE_ZOOM) {
            viewport.displayId = Display.DEFAULT_DISPLAY;
            viewport.logicalFrame.set(0, 0, 2160, 1920);
            viewport.physicalFrame.set(-1080, 0, 1080, 1920);
            viewport.deviceWidth = 2160;
            viewport.deviceHeight = 1920;
        } else if (mode == FUJISAN_MODE_DOCKED) {
            viewport.displayId = 1;
            viewport.logicalFrame.set(0, 0, 1080, 1920);
            viewport.physicalFrame.set(0, 0, 1080, 1920);
            viewport.deviceWidth = 1080;
            viewport.deviceHeight = 1920;
        }
    }

    private static boolean isFujisanDualDisplayTarget() {
        return SystemProperties.getBoolean("ro.feature.target_dual_display", false);
    }

    private static int getFujisanDisplayMode() {
        return SystemProperties.getInt("persist.vendor.fujisan.display_mode",
                FUJISAN_MODE_SINGLE);
    }

    private static int getFujisanSingleDisplay() {
        return SystemProperties.getInt("persist.vendor.fujisan.single_display_id",
                FUJISAN_SINGLE_A);
    }

'''
if helpers not in updated:
    if helper_anchor not in updated:
        print(f"Did not find logical-display helper anchor in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(helper_anchor, helpers + helper_anchor, 1)

updated = updated.replace('''        } else if (mode == FUJISAN_MODE_DOCKED) {
            viewport.displayId = 1;
            viewport.logicalFrame.set(0, 0, 1080, 1920);
            viewport.physicalFrame.set(0, 0, 1080, 1920);
            viewport.deviceWidth = 2160;
            viewport.deviceHeight = 1920;
        }
''', '''        } else if (mode == FUJISAN_MODE_DOCKED) {
            viewport.displayId = 1;
            viewport.logicalFrame.set(0, 0, 1080, 1920);
            viewport.physicalFrame.set(0, 0, 1080, 1920);
            viewport.deviceWidth = 1080;
            viewport.deviceHeight = 1920;
        }
''')

if updated == text:
    print(f"DisplayManagerService fujisan mode routing patch already present in {path}")
else:
    path.write_text(updated)
    print(f"Applied fujisan mode routing DisplayManagerService patch to {path}")
PY
