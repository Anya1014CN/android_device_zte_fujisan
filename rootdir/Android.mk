LOCAL_PATH := $(call my-dir)

# Early 18.1 builds used a separate vendor image and can leave an empty
# $(TARGET_ROOT_OUT)/vendor directory in incremental output.  rootdir's
# standard ln -sf then creates vendor/vendor instead of the legacy link.
# The generic recovery ramdisk imports /init.recovery.${ro.hardware}.rc.
# Install the QCOM fragment in TARGET_ROOT_OUT so build/make copies it into
# the dedicated recovery ramdisk before packing recovery.img.
include $(CLEAR_VARS)
LOCAL_MODULE := fujisan_recovery_init_qcom
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := init.recovery.qcom.rc
LOCAL_MODULE_PATH := $(TARGET_ROOT_OUT)
LOCAL_MODULE_STEM := init.recovery.qcom.rc
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := fujisan_legacy_vendor_root
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := .vendor-root-placeholder
LOCAL_MODULE_PATH := $(TARGET_OUT)
LOCAL_MODULE_STEM := .fujisan_legacy_vendor_root
LOCAL_POST_INSTALL_CMD := rm -rf $(TARGET_ROOT_OUT)/vendor $(TARGET_RECOVERY_ROOT_OUT)/vendor $(TARGET_OUT)/system; ln -s /system/vendor $(TARGET_ROOT_OUT)/vendor; mkdir -p $(TARGET_OUT)/bugreports/bugreports $(TARGET_OUT_VENDOR)/lib $(TARGET_OUT_VENDOR)/lib64
include $(BUILD_PREBUILT)
