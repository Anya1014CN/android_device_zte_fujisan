LOCAL_PATH := $(call my-dir)

# Early 18.1 builds used a separate vendor image and can leave an empty
# $(TARGET_ROOT_OUT)/vendor directory in incremental output.  rootdir's
# standard ln -sf then creates vendor/vendor instead of the legacy link.
#
# Fujisan also needs a /system -> / compatibility symlink inside the
# system-as-root image itself because several stock init scripts still
# reference /system/vendor/... after switch_root.
include $(CLEAR_VARS)
LOCAL_MODULE := fujisan_legacy_vendor_root
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := .vendor-root-placeholder
LOCAL_MODULE_PATH := $(TARGET_OUT)
LOCAL_MODULE_STEM := .fujisan_legacy_vendor_root
LOCAL_POST_INSTALL_CMD := rm -rf $(TARGET_ROOT_OUT)/vendor $(TARGET_RECOVERY_ROOT_OUT)/vendor $(TARGET_OUT)/system; ln -s /system/vendor $(TARGET_ROOT_OUT)/vendor; ln -s / $(TARGET_OUT)/system; mkdir -p $(TARGET_OUT)/bugreports/bugreports $(TARGET_OUT_VENDOR)/lib $(TARGET_OUT_VENDOR)/lib64; ln -sf /system/lib/libaudioroute.so $(TARGET_OUT_VENDOR)/lib/libaudioroute.so; ln -sf /system/lib64/libaudioroute.so $(TARGET_OUT_VENDOR)/lib64/libaudioroute.so; ln -sf /system/lib/libhidlbase.so $(TARGET_OUT_VENDOR)/lib/android.hidl.base@1.0.so; ln -sf /system/lib64/libhidlbase.so $(TARGET_OUT_VENDOR)/lib64/android.hidl.base@1.0.so; ln -sf /system/lib/libhidlbase.so $(TARGET_OUT_VENDOR)/lib/android.hidl.manager@1.0.so; ln -sf /system/lib64/libhidlbase.so $(TARGET_OUT_VENDOR)/lib64/android.hidl.manager@1.0.so
include $(BUILD_PREBUILT)

# msm8996 3.18 has no CONFIG_BPF_SYSCALL. AOSP bpfloader hard-reboots with
# reason bpfloader-failed when map creation returns ENOSYS. Replace the
# binary with a success stub so second-stage boot can continue.
include $(CLEAR_VARS)
LOCAL_MODULE := fujisan_bpfloader_override
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := bin/fujisan_bpfloader.sh
LOCAL_MODULE_PATH := $(TARGET_OUT_EXECUTABLES)
LOCAL_MODULE_STEM := bpfloader
LOCAL_OVERRIDES_MODULES := bpfloader
include $(BUILD_PREBUILT)
