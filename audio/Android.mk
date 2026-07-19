LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libdolbyshim
LOCAL_VENDOR_MODULE := true
LOCAL_MULTILIB := both
LOCAL_SRC_FILES := dolby_shim.c
LOCAL_CFLAGS := -Wall -Werror
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libtinyalsa_compat
LOCAL_VENDOR_MODULE := true
LOCAL_MULTILIB := both
LOCAL_SRC_FILES := tinyalsa_compat.c
LOCAL_CFLAGS := -Wall -Werror
LOCAL_SHARED_LIBRARIES := libtinyalsa
include $(BUILD_SHARED_LIBRARY)
