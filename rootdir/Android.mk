LOCAL_PATH := $(call my-dir)

# Early 18.1 builds used a separate vendor image and can leave an empty
# $(TARGET_ROOT_OUT)/vendor directory in incremental output.  rootdir's
# standard ln -sf then creates vendor/vendor instead of the legacy link.
include $(CLEAR_VARS)
LOCAL_MODULE := fujisan_legacy_vendor_root
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := .vendor-root-placeholder
LOCAL_MODULE_PATH := $(TARGET_ROOT_OUT)
LOCAL_MODULE_STEM := .fujisan_legacy_vendor_root
LOCAL_POST_INSTALL_CMD := rm -rf $(TARGET_ROOT_OUT)/vendor $(TARGET_RECOVERY_ROOT_OUT)/vendor; ln -s /system/vendor $(TARGET_ROOT_OUT)/vendor
include $(BUILD_PREBUILT)
