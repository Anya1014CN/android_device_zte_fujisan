LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := fujisan_halld
LOCAL_SRC_FILES := fujisan_halld.cpp
LOCAL_SHARED_LIBRARIES := liblog libcutils
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := -Wall -Werror
include $(BUILD_EXECUTABLE)
