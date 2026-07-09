LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := dualscreen-helper
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := dualscreen_helper.cpp
LOCAL_SHARED_LIBRARIES := \
    libhidlbase \
    libhidltransport \
    libhwbinder \
    libutils \
    liblog \
    libcutils \
    vendor.display.config@1.0
LOCAL_PROPRIETARY_MODULE := true
LOCAL_INIT_RC := dualscreen-helper.rc
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := libdualscreen-preload
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := dualscreen_preload.cpp
LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils \
    libdl \
    libhardware
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_SHARED_LIBRARY)
