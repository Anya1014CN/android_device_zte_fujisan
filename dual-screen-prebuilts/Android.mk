LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := framework-zte-res
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_STEM := framework-zte-res.apk
LOCAL_SRC_FILES := ../prebuilt/dual-screen/framework/framework-zte-res.apk
LOCAL_MODULE_PATH := $(TARGET_OUT)/framework
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := ZTE_Camera
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := APPS
LOCAL_MODULE_SUFFIX := $(COMMON_ANDROID_PACKAGE_SUFFIX)
LOCAL_MODULE_STEM := ZTE_Camera
LOCAL_SRC_FILES := ../prebuilt/dual-screen/apps/ZTE_Camera/ZTE_Camera.apk
LOCAL_CERTIFICATE := platform
LOCAL_PRIVILEGED_MODULE := true
LOCAL_DEX_PREOPT := false
LOCAL_MULTILIB := 32
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := axonmcontroller_MFV_Multy
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := APPS
LOCAL_MODULE_SUFFIX := $(COMMON_ANDROID_PACKAGE_SUFFIX)
LOCAL_MODULE_STEM := axonmcontroller_MFV_Multy
LOCAL_SRC_FILES := ../prebuilt/dual-screen/apps/axonmcontroller_MFV_Multy/axonmcontroller_MFV_Multy.apk
LOCAL_CERTIFICATE := platform
LOCAL_PRIVILEGED_MODULE := true
LOCAL_DEX_PREOPT := false
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := launchmodetest1_MFV_Multy
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := APPS
LOCAL_MODULE_SUFFIX := $(COMMON_ANDROID_PACKAGE_SUFFIX)
LOCAL_MODULE_STEM := launchmodetest1_MFV_Multy
LOCAL_SRC_FILES := ../prebuilt/dual-screen/apps/launchmodetest1_MFV_Multy/launchmodetest1_MFV_Multy.apk
LOCAL_CERTIFICATE := platform
LOCAL_PRIVILEGED_MODULE := true
LOCAL_DEX_PREOPT := false
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := DoubleLay_MFV_Multy
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := APPS
LOCAL_MODULE_SUFFIX := $(COMMON_ANDROID_PACKAGE_SUFFIX)
LOCAL_MODULE_STEM := DoubleLay_MFV_Multy
LOCAL_SRC_FILES := ../prebuilt/dual-screen/apps/DoubleLay_MFV_Multy/DoubleLay_MFV_Multy.apk
LOCAL_CERTIFICATE := platform
LOCAL_PRIVILEGED_MODULE := true
LOCAL_DEX_PREOPT := false
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := handservice_mfv_mfv6
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := APPS
LOCAL_MODULE_SUFFIX := $(COMMON_ANDROID_PACKAGE_SUFFIX)
LOCAL_MODULE_STEM := handservice_mfv_mfv6
LOCAL_SRC_FILES := ../prebuilt/dual-screen/apps/handservice_mfv_mfv6/handservice_mfv_mfv6.apk
LOCAL_CERTIFICATE := platform
LOCAL_DEX_PREOPT := false
include $(BUILD_PREBUILT)
