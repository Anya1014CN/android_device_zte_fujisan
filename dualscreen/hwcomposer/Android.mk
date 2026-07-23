LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := hwcomposer.fujisan
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := hwc2_fujisan.cpp
LOCAL_C_INCLUDES += \
    system/core/libsync \
    system/core/libsync/include \
    hardware/libhardware/include

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils \
    libdl \
    libhardware \
    libsync \
    libutils \
    libhidlbase \
    android.hardware.graphics.mapper@2.0 \
    android.hardware.graphics.common@1.0

LOCAL_CFLAGS := \
    -Wall \
    -Werror \
    -Wno-unused-parameter \
    -DLOG_TAG=\"HwcFujisan\"

# Secondary built-in panel geometry (right panel, portrait).
LOCAL_CFLAGS += \
    -DFUJISAN_SEC_WIDTH=1080 \
    -DFUJISAN_SEC_HEIGHT=1920 \
    -DFUJISAN_SEC_DPI_X=428625 \
    -DFUJISAN_SEC_DPI_Y=427789 \
    -DFUJISAN_SEC_VSYNC_NS=16666667

include $(BUILD_SHARED_LIBRARY)
