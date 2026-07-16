LOCAL_PATH := device/zte/fujisan

$(call inherit-product, $(SRC_TARGET_DIR)/product/product_launched_with_n_mr1.mk)
$(call inherit-product-if-exists, vendor/zte/fujisan/fujisan-vendor.mk)

PRODUCT_CHARACTERISTICS := nosdcard
PRODUCT_AAPT_CONFIG := normal
PRODUCT_AAPT_PREF_CONFIG := xxhdpi
PRODUCT_SHIPPING_API_LEVEL := 25

TARGET_SCREEN_HEIGHT := 1920
TARGET_SCREEN_WIDTH := 1080

PRODUCT_SOONG_NAMESPACES += \
    $(LOCAL_PATH) \
    vendor/zte/fujisan

PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    ro.product.first_api_level=25

PRODUCT_PACKAGES += \
    fujisan_legacy_vendor_root \
    bugreports_root_dir \
    copybit.msm8996 \
    gralloc.msm8996 \
    hwcomposer.msm8996 \
    memtrack.msm8996 \
    libdisplayconfig \
    liboverlay \
    libqdMetaData.system \
    libtinyxml \
    android.hardware.configstore@1.1-service \
    android.hardware.graphics.allocator@2.0-impl \
    android.hardware.graphics.allocator@2.0-service \
    android.hardware.graphics.composer@2.1-impl \
    android.hardware.graphics.composer@2.1-service \
    android.hardware.graphics.mapper@2.0-impl \
    android.hardware.memtrack@1.0-impl \
    android.hardware.memtrack@1.0-service

# AudioService waits synchronously for media.audio_policy, while audioserver
# cannot publish it until the declared audio@2.0 device factory is available.
# Build the AOSP wrapper instead of packaging the stock service executable.
PRODUCT_PACKAGES += \
    android.hardware.audio@2.0-impl \
    android.hardware.audio@2.0-service \
    libdolbyshim

# The legacy vendor manifest exposes IPower 1.0.  SystemServer waits for this
# HAL while creating PowerManagerService, so use the Android 11 wrapper for
# the already-installed generic power.default module.
PRODUCT_PACKAGES += \
    android.hardware.power@1.0-impl \
    android.hardware.power@1.0-service

# The stock light HAL is intentionally excluded from the legacy vendor image.
# Build the framework-compatible AOSP implementation instead; without it,
# DisplayManagerService blocks forever while entering the display boot phase.
PRODUCT_PACKAGES += \
    android.hardware.light@2.0-impl \
    android.hardware.light@2.0-service

# VibratorService synchronously acquires IVibrator during SystemServer startup.
# Supply the AOSP HIDL wrapper for the legacy vibrator.default implementation.
PRODUCT_PACKAGES += \
    android.hardware.vibrator@1.0-impl \
    android.hardware.vibrator@1.0-service

# Build the framework-compatible HIDL service; Qualcomm sensor backends remain
# supplied by the stock vendor image.
PRODUCT_PACKAGES += \
    android.hardware.sensors@1.0-impl \
    android.hardware.sensors@1.0-service

# Android 11 requires health@2.1.  Use the AOSP default implementation
# instead of the stock health@1.0 prebuilt.
PRODUCT_PACKAGES += \
    android.hardware.health@2.1-service \
    android.hardware.health@2.1-impl

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootdir/init.qcom.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/fujisan.rc \
    $(LOCAL_PATH)/rootdir/init.fujisan.sensors.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.fujisan.sensors.rc \
    $(LOCAL_PATH)/rootdir/firmware/.placeholder:$(TARGET_COPY_OUT_RAMDISK)/firmware/.placeholder \
    $(LOCAL_PATH)/rootdir/bt_firmware/.placeholder:$(TARGET_COPY_OUT_RAMDISK)/bt_firmware/.placeholder \
    $(LOCAL_PATH)/rootdir/dsp/.placeholder:$(TARGET_COPY_OUT_RAMDISK)/dsp/.placeholder \
    $(LOCAL_PATH)/rootdir/persist/.placeholder:$(TARGET_COPY_OUT_RAMDISK)/persist/.placeholder \
    $(LOCAL_PATH)/rootdir/etc/fstab.ramdisk.qcom:$(TARGET_COPY_OUT_RAMDISK)/fstab.qcom \
    $(LOCAL_PATH)/system/usr/idc/goodix-touchscreen.idc:$(TARGET_COPY_OUT_SYSTEM)/usr/idc/goodix-touchscreen.idc \
    $(LOCAL_PATH)/system/usr/idc/synaptics_dsx.idc:$(TARGET_COPY_OUT_SYSTEM)/usr/idc/synaptics_dsx.idc \
    $(LOCAL_PATH)/system/usr/idc/zte-touchscreen.idc:$(TARGET_COPY_OUT_SYSTEM)/usr/idc/zte-touchscreen.idc \
    $(LOCAL_PATH)/system/usr/idc/zte-touchscreen-2nd.idc:$(TARGET_COPY_OUT_SYSTEM)/usr/idc/zte-touchscreen-2nd.idc \
    $(LOCAL_PATH)/system/usr/keylayout/gpio-keys.kl:$(TARGET_COPY_OUT_SYSTEM)/usr/keylayout/gpio-keys.kl \
    $(LOCAL_PATH)/system/usr/keylayout/qpnp_pon.kl:$(TARGET_COPY_OUT_SYSTEM)/usr/keylayout/qpnp_pon.kl \
    $(LOCAL_PATH)/system/usr/keylayout/synaptics_dsx.kl:$(TARGET_COPY_OUT_SYSTEM)/usr/keylayout/synaptics_dsx.kl

PRODUCT_COPY_FILES += \
    $(call find-copy-subdir-files,*,$(LOCAL_PATH)/ramdisk,$(TARGET_COPY_OUT_RAMDISK))
