#!/bin/sh
set -eu

ROOT="${1:-$PWD}"
TARGET="$ROOT/hardware/qcom/display-caf/msm8996/libqdutils/qdMetaData.cpp"
DISPLAY_CONFIG_H="$ROOT/hardware/qcom/display-caf/msm8996/libqdutils/display_config.h"
GRALLOC_MK="$ROOT/hardware/qcom/display-caf/msm8996/libgralloc/Android.mk"
SDM_CORE_MK="$ROOT/hardware/qcom/display-caf/msm8996/sdm/libs/core/Android.mk"

if [ ! -f "$TARGET" ]; then
  echo "Missing target file: $TARGET" >&2
  exit 1
fi

if [ ! -f "$DISPLAY_CONFIG_H" ]; then
  echo "Missing target file: $DISPLAY_CONFIG_H" >&2
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

python3 - "$TARGET" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()

replacements = [
    (
        '''        ALOGE("%s: Private handle is invalid - handle:%p id: %" PRIu64,\n                __func__, handle, handle->id);''',
        '''        ALOGE("%s: Private handle is invalid - handle:%p",\n                __func__, handle);''',
    ),
    (
        '''        ALOGE("%s: Invalid metadata fd - handle:%p id: %" PRIu64 "fd: %d",\n                __func__, handle, handle->id, handle->fd_metadata);''',
        '''        ALOGE("%s: Invalid metadata fd - handle:%p fd: %d",\n                __func__, handle, handle->fd_metadata);''',
    ),
    (
        '''            ALOGE("%s: metadata mmap failed - handle:%p id: %" PRIu64  "fd: %d err: %s",\n                __func__, handle, handle->id, handle->fd_metadata, strerror(errno));''',
        '''            ALOGE("%s: metadata mmap failed - handle:%p fd: %d err: %s",\n                __func__, handle, handle->fd_metadata, strerror(errno));''',
    ),
]

updated = text
applied = 0
for old, new in replacements:
    if old in updated:
        updated = updated.replace(old, new)
        applied += 1

if applied == 0:
    print(f"No matching qdMetaData.cpp hunks found in {path}; skipping qdMetaData edits")
    sys.exit(0)

path.write_text(updated)
print(f"Applied {applied} qdMetaData.cpp replacements to {path}")
PY

python3 - "$DISPLAY_CONFIG_H" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()

old = '''#ifdef QTI_BSP
    DISPLAY_TERTIARY = HWC_DISPLAY_TERTIARY,
#endif
'''

if old not in text:
    if 'DISPLAY_TERTIARY = HWC_DISPLAY_TERTIARY' in text:
        print(f"Did not find expected tertiary display block in {path}", file=sys.stderr)
        sys.exit(1)
    print(f"Tertiary display compatibility already updated in {path}")
    sys.exit(0)

path.write_text(text.replace(old, '', 1))
print(f"Removed unsupported HWC_DISPLAY_TERTIARY reference in {path}")
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
