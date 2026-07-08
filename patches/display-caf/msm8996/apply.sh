#!/bin/sh
set -eu

ROOT="${1:-$PWD}"
find_first() {
  find "$1" $2 2>/dev/null | head -n 1
}

DISPLAY_SEARCH_ROOT="$ROOT/hardware/qcom"

QDUTILS_MK="$(find_first "$DISPLAY_SEARCH_ROOT" "-path */libqdutils/Android.mk")"
QDUTILS_BP="$(find_first "$DISPLAY_SEARCH_ROOT" "-path */libqdutils/Android.bp")"
DISPLAY_CONFIG_H="$(find_first "$DISPLAY_SEARCH_ROOT" "-path */libqdutils/display_config.h")"
GRALLOC_MK="$(find_first "$DISPLAY_SEARCH_ROOT" "-path */libgralloc/Android.mk")"
LIGHTS_PRV_CPP="$(find_first "$DISPLAY_SEARCH_ROOT" "-name lights_prv.cpp")"
SDM_CORE_MK="$(find_first "$DISPLAY_SEARCH_ROOT" "-path */sdm/libs/core/Android.mk")"
HWC_SESSION_CPP="$(find_first "$DISPLAY_SEARCH_ROOT" "-path */sdm/libs/hwc2/hwc_session.cpp")"

if [ -z "$QDUTILS_MK" ] && [ -z "$QDUTILS_BP" ]; then
  echo "Missing target file: libqdutils/Android.mk or libqdutils/Android.bp under $DISPLAY_SEARCH_ROOT" >&2
  exit 1
fi

if [ -z "$DISPLAY_CONFIG_H" ] || [ ! -f "$DISPLAY_CONFIG_H" ]; then
  echo "Missing target file: libqdutils/display_config.h under $DISPLAY_SEARCH_ROOT" >&2
  exit 1
fi

if [ -z "$GRALLOC_MK" ] || [ ! -f "$GRALLOC_MK" ]; then
  echo "Missing target file: libgralloc/Android.mk under $DISPLAY_SEARCH_ROOT" >&2
  exit 1
fi

if [ -z "$LIGHTS_PRV_CPP" ] || [ ! -f "$LIGHTS_PRV_CPP" ]; then
  echo "Missing target file: lights_prv.cpp under $DISPLAY_SEARCH_ROOT" >&2
  exit 1
fi

if [ -z "$SDM_CORE_MK" ] || [ ! -f "$SDM_CORE_MK" ]; then
  echo "Missing target file: sdm/libs/core/Android.mk under $DISPLAY_SEARCH_ROOT" >&2
  exit 1
fi

if [ -z "$HWC_SESSION_CPP" ] || [ ! -f "$HWC_SESSION_CPP" ]; then
  echo "Missing target file: sdm/libs/hwc2/hwc_session.cpp under $DISPLAY_SEARCH_ROOT" >&2
  exit 1
fi

python3 - "$DISPLAY_CONFIG_H" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()

old = '''#ifdef QTI_BSP
    DISPLAY_TERTIARY = HWC_DISPLAY_TERTIARY,
#endif
'''

new = '''#ifndef HWC_DISPLAY_TERTIARY
#define HWC_DISPLAY_TERTIARY HWC_DISPLAY_EXTERNAL
#endif
#ifdef QTI_BSP
    DISPLAY_TERTIARY = HWC_DISPLAY_TERTIARY,
#endif
'''

if new in text:
    print(f"Tertiary display compatibility already updated in {path}")
    sys.exit(0)

updated = text

if old in updated:
    updated = updated.replace(old, new, 1)
elif 'DISPLAY_EXTERNAL = HWC_DISPLAY_EXTERNAL,' in updated and 'DISPLAY_TERTIARY = HWC_DISPLAY_TERTIARY' not in updated:
    updated = updated.replace(
        'DISPLAY_EXTERNAL = HWC_DISPLAY_EXTERNAL,\n',
        'DISPLAY_EXTERNAL = HWC_DISPLAY_EXTERNAL,\n'
        '#ifndef HWC_DISPLAY_TERTIARY\n'
        '#define HWC_DISPLAY_TERTIARY HWC_DISPLAY_EXTERNAL\n'
        '#endif\n'
        '#ifdef QTI_BSP\n'
        '    DISPLAY_TERTIARY = HWC_DISPLAY_TERTIARY,\n'
        '#endif\n',
        1,
    )
else:
    print(f"Did not find expected tertiary display block in {path}", file=sys.stderr)
    sys.exit(1)

path.write_text(updated)
print(f"Restored tertiary display compatibility in {path}")
PY

python3 - "$QDUTILS_MK" "$QDUTILS_BP" <<'PY'
from pathlib import Path
import sys

mk_path = Path(sys.argv[1])
bp_path = Path(sys.argv[2])

if mk_path.is_file():
    path = mk_path
    text = path.read_text()

    if 'LOCAL_MODULE                    := libqdMetaData' in text:
        print(f"libqdMetaData module already defined in {path}")
        sys.exit(0)

    anchor = 'include $(BUILD_SHARED_LIBRARY)\n'
    if anchor not in text:
        print(f"Did not find expected libqdutils module terminator in {path}", file=sys.stderr)
        sys.exit(1)

    addition = '''

include $(CLEAR_VARS)

LOCAL_EXPORT_C_INCLUDE_DIRS   := $(LOCAL_PATH)
LOCAL_SHARED_LIBRARIES        := liblog libcutils
LOCAL_C_INCLUDES              := $(common_includes)
LOCAL_ADDITIONAL_DEPENDENCIES := $(common_deps)
LOCAL_SRC_FILES               := qdMetaData.cpp
LOCAL_CFLAGS                  := $(common_flags) -Wno-sign-conversion
LOCAL_CFLAGS                  += -DLOG_TAG="DisplayMetaData"
LOCAL_CLANG                   := true
LOCAL_MODULE_TAGS             := optional
LOCAL_MODULE                  := libqdMetaData
LOCAL_PROPRIETARY_MODULE      := true
include $(BUILD_SHARED_LIBRARY)
'''

    first = text.find(anchor)
    updated = text[:first + len(anchor)] + addition + text[first + len(anchor):]
    path.write_text(updated)
    print(f"Restored libqdMetaData module definition in {path}")
    sys.exit(0)

path = bp_path
text = path.read_text()

if 'name: "libqdMetaData"' in text:
    print(f"libqdMetaData module already defined in {path}")
    sys.exit(0)

anchor = 'cc_library_shared {\n    name: "libqdutils",'
if anchor not in text:
    print(f"Did not find expected libqdutils module block in {path}", file=sys.stderr)
    sys.exit(1)

block_end = '\n}\n'
first = text.find(anchor)
end = text.find(block_end, first)
if end == -1:
    print(f"Did not find end of libqdutils module block in {path}", file=sys.stderr)
    sys.exit(1)

end += len(block_end)
addition = '''

cc_library_shared {
    name: "libqdMetaData",
    vendor: true,
    defaults: ["display_defaults"],
    cflags: [
        "-Wno-sign-conversion",
        "-DLOG_TAG=\\"qdmetadata\\"",
    ],
    srcs: ["qdMetaData.cpp", "qd_utils.cpp"],
}
'''

updated = text[:end] + addition + text[end:]
path.write_text(updated)
print(f"Restored libqdMetaData module definition in {path}")
PY

python3 - "$GRALLOC_MK" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()

old = 'LOCAL_SHARED_LIBRARIES        := $(common_libs) libmemalloc libqdMetaData'
new = 'LOCAL_SHARED_LIBRARIES        := $(common_libs) libmemalloc libqdMetaData libqdutils'

if new in text:
    print(f"libqdutils already linked in {path}")
    sys.exit(0)

if old not in text:
    print(f"Did not find expected gralloc link line in {path}", file=sys.stderr)
    sys.exit(1)

path.write_text(text.replace(old, new, 1))
print(f"Linked libqdutils into gralloc module in {path}")
PY

python3 - "$LIGHTS_PRV_CPP" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()

old = '''#include <hardware/hwcomposer_defs.h>
#include "disp_color_apis.h"
#include "lights_prv.h"

/******************************************************************************/
static DISPAPI_HANDLE g_ctx;

/**
 * device methods
 */

void set_brightness_ext_init(void)
{
   disp_api_init((DISPAPI_HANDLE*) &g_ctx, 0);
}

int set_brightness_ext_level(int level)
{
    int err = disp_api_set_panel_brightness_level_ext(g_ctx, HWC_DISPLAY_PRIMARY,
                                                 level, 0);

    return err;
}
'''

new = '''#include "lights_prv.h"

void set_brightness_ext_init(void)
{
}

int set_brightness_ext_level(int level)
{
    (void) level;
    return 0;
}
'''

if old not in text:
    if '#include "disp_color_apis.h"' not in text:
        print(f"liblight color API compatibility already updated in {path}")
        sys.exit(0)
    print(f"Did not find expected lights_prv.cpp contents in {path}", file=sys.stderr)
    sys.exit(1)

path.write_text(text.replace(old, new, 1))
print(f"Replaced liblight proprietary color API usage in {path}")
PY

python3 - "$SDM_CORE_MK" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()

old_blocks = [
    (
        '''ifneq ($(TARGET_IS_HEADLESS), true)
    LOCAL_CFLAGS              += -DCOMPILE_DRM -isystem external/libdrm
    LOCAL_SHARED_LIBRARIES    += libdrm libdrmutils
    LOCAL_HW_INTF_PATH_2      := drm
endif''',
        '''ifeq ($(TARGET_USES_DRM_SDM), true)
    LOCAL_CFLAGS              += -DCOMPILE_DRM -isystem external/libdrm
    LOCAL_SHARED_LIBRARIES    += libdrm libdrmutils
    LOCAL_HW_INTF_PATH_2      := drm
endif''',
    ),
    (
        '''ifneq ($(TARGET_IS_HEADLESS), true)
    LOCAL_SRC_FILES           += $(LOCAL_HW_INTF_PATH_2)/hw_info_drm.cpp \\
                                 $(LOCAL_HW_INTF_PATH_2)/hw_device_drm.cpp \\
                                 $(LOCAL_HW_INTF_PATH_2)/hw_events_drm.cpp \\
                                 $(LOCAL_HW_INTF_PATH_2)/hw_scale_drm.cpp \\
                                 $(LOCAL_HW_INTF_PATH_2)/hw_color_manager_drm.cpp
endif''',
        '''ifeq ($(TARGET_USES_DRM_SDM), true)
    LOCAL_SRC_FILES           += $(LOCAL_HW_INTF_PATH_2)/hw_info_drm.cpp \\
                                 $(LOCAL_HW_INTF_PATH_2)/hw_device_drm.cpp \\
                                 $(LOCAL_HW_INTF_PATH_2)/hw_events_drm.cpp \\
                                 $(LOCAL_HW_INTF_PATH_2)/hw_scale_drm.cpp \\
                                 $(LOCAL_HW_INTF_PATH_2)/hw_color_manager_drm.cpp
endif''',
    ),
]

updated = text
applied = 0
for old, new in old_blocks:
    if old in updated:
        updated = updated.replace(old, new, 1)
        applied += 1

if applied == 0:
    if 'ifeq ($(TARGET_USES_DRM_SDM), true)' in updated:
        print(f"DRM SDM gating already updated in {path}")
        sys.exit(0)
    print(f"Did not find expected SDM DRM blocks in {path}", file=sys.stderr)
    sys.exit(1)

path.write_text(updated)
print(f"Updated {applied} SDM DRM gating blocks in {path}")
PY

python3 - "$HWC_SESSION_CPP" <<'PY'
from pathlib import Path
import re
import sys

path = Path(sys.argv[1])
text = path.read_text()

helper_old = '''#define HWC_UEVENT_SWITCH_HDMI "change@/devices/virtual/switch/hdmi"
#define HWC_UEVENT_GRAPHICS_FB0 "change@/devices/virtual/graphics/fb0"
'''

helper_new = '''#define HWC_UEVENT_SWITCH_HDMI "change@/devices/virtual/switch/hdmi"
#define HWC_UEVENT_GRAPHICS_FB0 "change@/devices/virtual/graphics/fb0"

static bool IsFujisanDualDisplayTarget() {
  char value[PROPERTY_VALUE_MAX] = {};
  property_get("ro.feature.target_dual_display", value, "0");
  return value[0] == '1';
}
'''

updated = text
applied = 0

if helper_new not in updated:
    if helper_old not in updated:
        print(f"Did not find expected HWCSession helper block in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(helper_old, helper_new, 1)
    applied += 1

deinit_marker = '''int HWCSession::Deinit() {
  if (hwc_display_[HWC_DISPLAY_EXTERNAL]) {
    HWCDisplayExternal::Destroy(hwc_display_[HWC_DISPLAY_EXTERNAL]);
    hwc_display_[HWC_DISPLAY_EXTERNAL] = 0;
  }

  HWCDisplayPrimary::Destroy(hwc_display_[HWC_DISPLAY_PRIMARY]);
'''

if deinit_marker not in updated:
    deinit_pattern = re.compile(
        r'int HWCSession::Deinit\(\) \{\n'
        r'  HWCDisplayPrimary::Destroy\(hwc_display_\[HWC_DISPLAY_PRIMARY\]\);\n',
        re.M,
    )
    if not deinit_pattern.search(updated):
        print(f"Did not find expected HWCSession Deinit block in {path}", file=sys.stderr)
        sys.exit(1)
    updated = deinit_pattern.sub(
        'int HWCSession::Deinit() {\n'
        '  if (hwc_display_[HWC_DISPLAY_EXTERNAL]) {\n'
        '    HWCDisplayExternal::Destroy(hwc_display_[HWC_DISPLAY_EXTERNAL]);\n'
        '    hwc_display_[HWC_DISPLAY_EXTERNAL] = 0;\n'
        '  }\n'
        '\n'
        '  HWCDisplayPrimary::Destroy(hwc_display_[HWC_DISPLAY_PRIMARY]);\n',
        updated,
        count=1,
    )
    applied += 1

register_marker = '''  if (descriptor == HWC2_CALLBACK_HOTPLUG) {
    hwc_session->callbacks_.Hotplug(HWC_DISPLAY_PRIMARY, HWC2::Connection::Connected);
    if (IsFujisanDualDisplayTarget()) {
      int secondary_status = hwc_session->HotPlugHandler(true);
      if (secondary_status) {
        DLOGW("Failed to bring up dual-screen secondary display, status = %d",
              secondary_status);
      }
    }
  }'''

if register_marker not in updated:
    register_pattern = re.compile(
        r'  if \(descriptor == HWC2_CALLBACK_HOTPLUG\)\n'
        r'    hwc_session->callbacks_\.Hotplug\(HWC_DISPLAY_PRIMARY, HWC2::Connection::Connected\);\n',
        re.M,
    )
    if not register_pattern.search(updated):
        print(f"Did not find expected HWCSession callback block in {path}", file=sys.stderr)
        sys.exit(1)
    updated = register_pattern.sub(
        '  if (descriptor == HWC2_CALLBACK_HOTPLUG) {\n'
        '    hwc_session->callbacks_.Hotplug(HWC_DISPLAY_PRIMARY, HWC2::Connection::Connected);\n'
        '    if (IsFujisanDualDisplayTarget()) {\n'
        '      int secondary_status = hwc_session->HotPlugHandler(true);\n'
        '      if (secondary_status) {\n'
        '        DLOGW("Failed to bring up dual-screen secondary display, status = %d",\n'
        '              secondary_status);\n'
        '      }\n'
        '    }\n'
        '  }\n',
        updated,
        count=1,
    )
    applied += 1

online_marker = '''        status = ConnectDisplay(HWC_DISPLAY_EXTERNAL);
        if (status) {
          return status;
        }
        if (IsFujisanDualDisplayTarget()) {
          int online_status = hwc_display_[HWC_DISPLAY_EXTERNAL]->SetDisplayStatus(EXTERNAL_ONLINE);
          if (online_status) {
            DLOGW("Failed to mark dual-screen secondary display online, status = %d",
                  online_status);
          }
        }
        notify_hotplug = true;'''

if online_marker not in updated:
    online_pattern = re.compile(
        r'        status = ConnectDisplay\(HWC_DISPLAY_EXTERNAL\);\n'
        r'        if \(status\) \{\n'
        r'          return status;\n'
        r'        \}\n'
        r'        notify_hotplug = true;',
        re.M,
    )
    if not online_pattern.search(updated):
        print(f"Did not find expected HWCSession hotplug connect block in {path}", file=sys.stderr)
        sys.exit(1)
    updated = online_pattern.sub(
        '        status = ConnectDisplay(HWC_DISPLAY_EXTERNAL);\n'
        '        if (status) {\n'
        '          return status;\n'
        '        }\n'
        '        if (IsFujisanDualDisplayTarget()) {\n'
        '          int online_status = hwc_display_[HWC_DISPLAY_EXTERNAL]->SetDisplayStatus(EXTERNAL_ONLINE);\n'
        '          if (online_status) {\n'
        '            DLOGW("Failed to mark dual-screen secondary display online, status = %d",\n'
        '                  online_status);\n'
        '          }\n'
        '        }\n'
        '        notify_hotplug = true;',
        updated,
        count=1,
    )
    applied += 1

if updated == text:
    print(f"HWC2 secondary hotplug compatibility already updated in {path}")
    sys.exit(0)

path.write_text(updated)
print(f"Updated {applied} HWC2 dual-display blocks in {path}")
PY
