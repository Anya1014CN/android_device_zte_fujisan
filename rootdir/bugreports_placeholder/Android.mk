LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := bugreports_root_dir
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := .placeholder
LOCAL_MODULE_PATH := $(TARGET_OUT)/bugreports/bugreports
LOCAL_MODULE_STEM := .placeholder
LOCAL_POST_INSTALL_CMD := rm -f $(LOCAL_INSTALLED_MODULE)
include $(BUILD_PREBUILT)
