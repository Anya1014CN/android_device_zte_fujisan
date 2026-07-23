LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := fujisan_fb1_fill
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := fujisan_fb1_fill.c
LOCAL_SHARED_LIBRARIES := libcutils liblog
LOCAL_CFLAGS := -Wall
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_EXECUTABLES)
include $(BUILD_EXECUTABLE)

include $(call all-makefiles-under,$(LOCAL_PATH))
