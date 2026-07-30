LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libril-compat
LOCAL_VENDOR_MODULE := true
LOCAL_MULTILIB := both
LOCAL_SRC_FILES := \
    ril_compat.c \
    parcel_compat.cpp
LOCAL_SHARED_LIBRARIES := libc++ libdl
LOCAL_SHARED_LIBRARIES += liblog
LOCAL_LDFLAGS := -Wl,--version-script,$(LOCAL_PATH)/parcel_compat.map
LOCAL_CFLAGS := -Werror
include $(BUILD_SHARED_LIBRARY)
