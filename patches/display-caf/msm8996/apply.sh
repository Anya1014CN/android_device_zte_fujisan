#!/bin/sh
set -eu

ROOT="${1:-$PWD}"
DISPLAY_ROOT=""
for candidate in \
  "$ROOT/hardware/qcom/display/msm8996" \
  "$ROOT/hardware/qcom/display-caf/msm8996"
do
  if [ -d "$candidate" ]; then
    DISPLAY_ROOT="$candidate"
    break
  fi
done

if [ -z "$DISPLAY_ROOT" ]; then
  echo "Missing target directory: hardware/qcom/display/msm8996 or hardware/qcom/display-caf/msm8996" >&2
  exit 1
fi

QDUTILS_MK="$DISPLAY_ROOT/libqdutils/Android.mk"
QDUTILS_BP="$DISPLAY_ROOT/libqdutils/Android.bp"
DISPLAY_TOP_MK="$DISPLAY_ROOT/Android.mk"
DISPLAY_CONFIG_H="$DISPLAY_ROOT/libqdutils/display_config.h"
GRALLOC_MK="$DISPLAY_ROOT/libgralloc/Android.mk"
LIGHTS_PRV_CPP="$DISPLAY_ROOT/liblight/lights_prv.cpp"
SDM_CORE_MK="$DISPLAY_ROOT/sdm/libs/core/Android.mk"
HWC_SESSION_CPP="$DISPLAY_ROOT/sdm/libs/hwc2/hwc_session.cpp"

if [ -z "$QDUTILS_MK" ] && [ -z "$QDUTILS_BP" ]; then
  echo "Missing target file: $QDUTILS_MK or $QDUTILS_BP" >&2
  exit 1
fi

if [ ! -f "$QDUTILS_MK" ] && [ ! -f "$QDUTILS_BP" ]; then
  echo "Missing target file: $QDUTILS_MK or $QDUTILS_BP" >&2
  exit 1
fi

if [ ! -f "$DISPLAY_CONFIG_H" ]; then
  echo "Missing target file: $DISPLAY_CONFIG_H" >&2
  exit 1
fi

if [ ! -f "$DISPLAY_TOP_MK" ]; then
  echo "Missing target file: $DISPLAY_TOP_MK" >&2
  exit 1
fi

if [ ! -f "$GRALLOC_MK" ]; then
  echo "Missing target file: $GRALLOC_MK" >&2
  exit 1
fi

if [ ! -f "$SDM_CORE_MK" ]; then
  echo "Missing target file: $SDM_CORE_MK" >&2
  exit 1
fi

if [ ! -f "$HWC_SESSION_CPP" ]; then
  echo "Missing target file: $HWC_SESSION_CPP" >&2
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

python3 - "$DISPLAY_TOP_MK" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()

updated = text
applied = 0

if updated.startswith('ifeq ($(TARGET_QCOM_DISPLAY_VARIANT),caf-msm8996)\n'):
    updated = updated[len('ifeq ($(TARGET_QCOM_DISPLAY_VARIANT),caf-msm8996)\n'):]
    if updated.endswith('\nendif\n'):
        updated = updated[:-len('\nendif\n')] + '\n'
    elif updated.endswith('\nendif'):
        updated = updated[:-len('\nendif')] + '\n'
    applied += 1

display_hals_line = next(
    (line for line in updated.splitlines() if line.strip().startswith('display-hals :=')),
    '',
)

if 'libqdutils' in display_hals_line and 'libqservice' in display_hals_line:
    if applied == 0:
        print(f"Display HAL module list already exports libqdutils in {path}")
        sys.exit(0)
    path.write_text(updated)
    print(f"Removed variant gate from display HAL makefile in {path}")
    sys.exit(0)

old = 'display-hals := include $(sdm-libs)/utils $(sdm-libs)/core'
new = 'display-hals := include libqdutils libqservice $(sdm-libs)/utils $(sdm-libs)/core'

if old in updated:
    updated = updated.replace(old, new, 1)
    applied += 1
else:
    if applied == 0:
        print(f"Skipping Display HAL module-list patch; no display-hals definition found in {path}")
        sys.exit(0)
    path.write_text(updated)
    print(f"Removed variant gate from display HAL makefile in {path}")
    sys.exit(0)

path.write_text(updated)
if applied == 0:
    print(f"Display HAL module list already exports libqdutils in {path}")
else:
    print(f"Updated display HAL makefile for libqdutils bring-up in {path}")
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

if [ -f "$LIGHTS_PRV_CPP" ]; then
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
else
  echo "Skipping optional liblight compatibility patch; $LIGHTS_PRV_CPP not present"
fi

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
    if 'LOCAL_HW_INTF_PATH            := fb' in updated and 'hw_info_drm.cpp' not in updated:
        print(f"Skipping SDM DRM gating patch for fb-only layout in {path}")
        sys.exit(0)
    print(f"Skipping SDM DRM gating patch for unmatched layout in {path}")
    sys.exit(0)

path.write_text(updated)
print(f"Updated {applied} SDM DRM gating blocks in {path}")
PY

python3 - "$HWC_SESSION_CPP" <<'PY'
from pathlib import Path
import re
import sys

path = Path(sys.argv[1])
text = path.read_text()

updated = text
applied = 0

updated = re.sub(
    r'''        if \(IsFujisanDualDisplayTarget\(\)\) \{\n'''
    r'''          int online_status = hwc_display_\[HWC_DISPLAY_EXTERNAL\]->SetDisplayStatus\(EXTERNAL_ONLINE\);\n'''
    r'''          if \(online_status\) \{\n'''
    r'''            DLOGW\("Failed to mark dual-screen secondary display online, status = %d",\n'''
    r'''                  online_status\);\n'''
    r'''          \}\n'''
    r'''        \}\n''',
    '',
    updated,
    count=1,
)
if updated != text:
    applied += 1

helper_pattern = re.compile(
    r'''\nstatic bool IsFujisanDualDisplayTarget\(\) \{\n'''
    r'''  .*?'''
    r'''\}\n''',
    re.S,
)
updated_without_helper = helper_pattern.sub("\n", updated, count=1)
if updated_without_helper != updated:
    updated = updated_without_helper
    applied += 1

if 'HWCDisplayExternal::Destroy(hwc_display_[HWC_DISPLAY_EXTERNAL])' not in updated:
    deinit_pattern = re.compile(
        r'(int HWCSession::Deinit\(\) \{\n)'
        r'((?:  Locker::SequenceCancelScopeLock lock_[vep]'
        r'\(locker_\[HWC_DISPLAY_(?:VIRTUAL|EXTERNAL|PRIMARY)\]\);\n)*)',
        re.M,
    )
    if not deinit_pattern.search(updated):
        print(f"Did not find expected HWCSession Deinit block in {path}", file=sys.stderr)
        sys.exit(1)
    updated = deinit_pattern.sub(
        r'\1\2'
        '  if (hwc_display_[HWC_DISPLAY_EXTERNAL]) {\n'
        '    HWCDisplayExternal::Destroy(hwc_display_[HWC_DISPLAY_EXTERNAL]);\n'
        '    hwc_display_[HWC_DISPLAY_EXTERNAL] = nullptr;\n'
        '  }\n'
        '\n'
        ,
        updated,
        count=1,
    )
    applied += 1

register_pattern = re.compile(
    r'  if \(descriptor == HWC2_CALLBACK_HOTPLUG\) \{\n'
    r'(?:    if \(hwc_session->hwc_display_\[HWC_DISPLAY_PRIMARY\]\) \{\n)?'
    r'      hwc_session->callbacks_\.Hotplug\(HWC_DISPLAY_PRIMARY, HWC2::Connection::Connected\);\n'
    r'(?:    \}\n)?'
    r'(?:    if \(IsFujisanDualDisplayTarget\(\)\) \{\n'
    r'      int secondary_status = hwc_session->HotPlugHandler\(true\);\n'
    r'      if \(secondary_status\) \{\n'
    r'        DLOGW\("Failed to bring up dual-screen secondary display, status = %d",\n'
    r'              secondary_status\);\n'
    r'      \}\n'
    r'    \}\n)?'
    r'  \}\n',
    re.M,
)
if not register_pattern.search(updated):
    print(f"Did not find expected HWCSession callback block in {path}", file=sys.stderr)
    sys.exit(1)
register_replacement = (
    '  if (descriptor == HWC2_CALLBACK_HOTPLUG) {\n'
    '    if (hwc_session->hwc_display_[HWC_DISPLAY_PRIMARY]) {\n'
    '      hwc_session->callbacks_.Hotplug(HWC_DISPLAY_PRIMARY, HWC2::Connection::Connected);\n'
    '    }\n'
    '  }\n'
)
updated_after_register = register_pattern.sub(register_replacement, updated, count=1)
if updated_after_register != updated:
    updated = updated_after_register
    applied += 1

if updated == text:
    print(f"HWC2 boot-safe hotplug compatibility already updated in {path}")
    sys.exit(0)

path.write_text(updated)
print(f"Updated {applied} HWC2 boot-safe dual-display blocks in {path}")
PY
