LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libperipheral_client
LOCAL_VENDOR_MODULE := true
LOCAL_MULTILIB := both
LOCAL_SRC_FILES := peripheral_client.cpp
LOCAL_SHARED_LIBRARIES := \
    libbinder \
    libc++ \
    liblog \
    libutils
LOCAL_CFLAGS := -Werror -Wall -Wextra -DDO_NOT_CHECK_MANUAL_BINDER_INTERFACES
include $(BUILD_SHARED_LIBRARY)
