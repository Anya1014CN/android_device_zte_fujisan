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

for include in ("#include <thread>\n", "#include <unistd.h>\n"):
    if include not in text:
        anchor = "#include <utils/Trace.h>\n"
        if anchor in text:
            text = text.replace(anchor, anchor + include, 1)
        else:
            text = include + text

before_cleanup = text

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

if text != before_cleanup:
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
            bool createDisplay = true;
            if (fujisanDualDisplay) {
                const sp<IBinder> oldToken = mBuiltinDisplays[type];
                const wp<IBinder> oldDisplay(oldToken);
                if (oldToken != nullptr && mDisplays.indexOfKey(oldDisplay) < 0) {
                    ALOGI("fujisan: reusing stale secondary display token for hotplug replay");
                    mDrawingState.displays.removeItem(oldDisplay);
                    createDisplay = false;
                } else if (oldToken != nullptr) {
                    ALOGI("fujisan: secondary display token already has a DisplayDevice");
                    createDisplay = false;
                }
            }
            if (createDisplay) {
                createBuiltinDisplayLocked(type);
            }
'''

old = '''        if (connection == HWC2::Connection::Connected) {
            createBuiltinDisplayLocked(type);
'''

old_cleanup = '''        if (connection == HWC2::Connection::Connected) {
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

old_create_new_token_cleanup = '''        if (connection == HWC2::Connection::Connected) {
            bool fujisanDualDisplay = false;
            {
                char target[PROPERTY_VALUE_MAX];
                property_get("ro.feature.target_dual_display", target, "0");
                fujisanDualDisplay = target[0] == '1';
            }
            bool createDisplay = true;
            if (fujisanDualDisplay) {
                const sp<IBinder> oldToken = mBuiltinDisplays[type];
                const wp<IBinder> oldDisplay(oldToken);
                if (oldToken != nullptr && mDisplays.indexOfKey(oldDisplay) < 0) {
                    ALOGI("fujisan: dropping stale secondary display token before hotplug replay");
                    mCurrentState.displays.removeItem(oldDisplay);
                    mDrawingState.displays.removeItem(oldDisplay);
                    mBuiltinDisplays[type].clear();
                } else if (oldToken != nullptr) {
                    ALOGI("fujisan: secondary display token already has a DisplayDevice");
                    createDisplay = false;
                }
            }
            if (createDisplay) {
                createBuiltinDisplayLocked(type);
            }
'''

if cleanup in text:
    pass
elif old_create_new_token_cleanup in text:
    text = text.replace(old_create_new_token_cleanup, cleanup, 1)
elif old_cleanup in text:
    text = text.replace(old_cleanup, cleanup, 1)
elif old in text:
    text = text.replace(old, cleanup, 1)
else:
    print(f"fujisan: ERROR: could not find external hotplug connect block in {path}", file=sys.stderr)
    sys.exit(1)

boot_replay = '''    // Fujisan dual-display: stock HWC can expose display 1 before SurfaceFlinger
    // has a real DisplayDevice for it. After boot, vendor init marks the secondary
    // panel online; replay the external hotplug a few times so SF can bind it.
    // DisplayManager may already have sent the ON power request while SF still
    // had no DisplayDevice, so follow the replay with an explicit power-on.
    if (property_get_bool("ro.feature.target_dual_display", false)) {
        const auto sequenceId = mComposerSequenceId;
        std::thread([this, sequenceId]() {
            for (int attempt = 0; attempt < 3; attempt++) {
                sleep(2);
                char mode[PROPERTY_VALUE_MAX];
                property_get("persist.vendor.fujisan.display_mode", mode, "1");
                if (mode[0] != '2' && mode[0] != '4' && mode[0] != '8') {
                    return;
                }
                ALOGI("fujisan: replaying secondary built-in display hotplug after boot");
                onHotplugReceived(sequenceId, HWC_DISPLAY_EXTERNAL,
                        HWC2::Connection::Connected, false);
                for (int powerAttempt = 0; powerAttempt < 4; powerAttempt++) {
                    usleep(250000);
                    sp<IBinder> secondaryDisplay;
                    {
                        Mutex::Autolock _l(mStateLock);
                        secondaryDisplay = mBuiltinDisplays[DisplayDevice::DISPLAY_EXTERNAL];
                    }
                    if (secondaryDisplay != nullptr) {
                        ALOGI("fujisan: forcing secondary built-in display power on");
                        setPowerMode(secondaryDisplay, HWC_POWER_MODE_NORMAL);
                    }
                }
            }
        }).detach();
    }
'''

boot_anchor = '''    sp<LambdaMessage> readProperties = new LambdaMessage([&]() {
        readPersistentProperties();
    });
    postMessageAsync(readProperties);
'''

old_boot_replay_pattern = re.compile(
    r'''\n    // Fujisan dual-display: stock HWC can expose display 1 before SurfaceFlinger\n'''
    r'''    // has a real DisplayDevice for it\..*?'''
    r'''        \}\)\.detach\(\);\n'''
    r'''    \}\n''',
    re.S,
)

if boot_replay not in text:
    text = old_boot_replay_pattern.sub("", text, count=1)
    if boot_anchor not in text:
        print(f"fujisan: ERROR: could not find bootFinished readProperties anchor in {path}", file=sys.stderr)
        sys.exit(1)
    text = text.replace(boot_anchor, boot_anchor + "\n" + boot_replay, 1)

path.write_text(text)
print(f"fujisan: SurfaceFlinger boot-safe secondary hotplug replay applied to {path}")
PY
