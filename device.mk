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

# Fujisan exposes panel-native color balance through /proc/panel_hue_0_set.
# This LiveDisplay HAL maps Lineage color balance to that main-panel control.
PRODUCT_PACKAGES += \
    vendor.lineage.livedisplay@2.0-service.fujisan

PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    ro.product.first_api_level=25 \
    audio.smartpa.channel=right

# The Android 11 Qualcomm Bluetooth HAL selects the ROME UART transport from
# this vendor property. The legacy qcom.bluetooth.soc key is not sufficient.
PRODUCT_VENDOR_PROPERTIES += \
    vendor.qcom.bluetooth.soc=rome

# Use Android 11 gestural navigation by default.
PRODUCT_PACKAGES += \
    NavigationBarModeGesturalOverlay

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

# Use the Android 11 Qualcomm Bluetooth HIDL stack.  BOARD_HAS_QCA_BT_ROME
# selects the transport at build time, but does not install these modules.
PRODUCT_PACKAGES += \
    android.hardware.bluetooth@1.0-service-qti \
    android.hardware.bluetooth@1.0-impl-qti \
    com.qualcomm.qti.ant@1.0 \
    com.qualcomm.qti.ant@1.0-impl

# The FPC service is proprietary, but its standard HIDL interfaces must be
# provided by the Android 11 build for the stock service and extension library.
PRODUCT_PACKAGES += \
    android.hardware.biometrics.fingerprint@2.1 \
    android.hidl.base@1.0

# AudioService waits synchronously for media.audio_policy, while audioserver
# cannot publish it until the declared audio@2.0 device factory is available.
# Follow the msm8996 LineageOS pattern: keep the Android 11 HIDL service stack
# source-built, and provide the device-specific codec glue through the stock
# audio.primary.msm8996 HAL plus vendor helpers such as libdolbyshim.
PRODUCT_PACKAGES += \
    android.hardware.audio@2.0 \
    android.hardware.audio.common@2.0 \
    android.hardware.audio.effect@2.0 \
    android.hardware.audio@2.0-impl \
    android.hardware.audio.effect@2.0-impl \
    android.hardware.audio@2.0-service \
    libdolbyshim

# The legacy vendor manifest exposes IPower 1.0.  SystemServer waits for this
# HAL while creating PowerManagerService, so use the Android 11 wrapper for
# the already-installed generic power.default module.
PRODUCT_PACKAGES += \
    android.hardware.power@1.0-impl \
    android.hardware.power@1.0-service

# The HIDL light service wraps the legacy msm8996 module that drives the
# primary panel backlight.
PRODUCT_PACKAGES += \
    lights.msm8996 \
    android.hardware.light@2.0-impl \
    android.hardware.light@2.0-service

# VibratorService synchronously acquires IVibrator during SystemServer startup.
# Supply the AOSP HIDL wrapper for the legacy vibrator.default implementation.
PRODUCT_PACKAGES += \
    android.hardware.vibrator@1.0-impl \
    android.hardware.vibrator@1.0-service

# HardwarePropertiesManagerService acquires the declared thermal HAL during
# SystemServer startup.  The stock service is excluded from the vendor image,
# so provide the framework-compatible AOSP implementation.
PRODUCT_PACKAGES += \
    android.hardware.thermal@1.0-impl \
    android.hardware.thermal@1.0-service

# BatteryExternalStatsWorker synchronously queries the Wi-Fi HAL before
# returning battery statistics.  Without a registered IWifi service, Settings
# exhausts its parallel controller pool and ANRs while opening dashboards.
PRODUCT_PACKAGES += \
    android.hardware.wifi@1.0 \
    android.hardware.wifi@1.0-impl \
    android.hardware.wifi@1.0-service \
    android.hardware.wifi@1.1 \
    android.hardware.wifi@1.2 \
    android.hardware.wifi@1.3 \
    android.hardware.wifi@1.4 \
    android.hardware.wifi.supplicant@1.0 \
    android.hardware.wifi.supplicant@1.1 \
    android.hardware.wifi.supplicant@1.2 \
    android.hardware.wifi.supplicant@1.3 \
    wpa_supplicant \
    wpa_supplicant.conf

# The stock security HAL services are excluded from the generated vendor
# image.  Keystore otherwise waits forever for Keymaster and never publishes
# android.security.keystore, which crashes SystemServer during boot phase 600.
PRODUCT_PACKAGES += \
    android.hardware.gatekeeper@1.0-impl \
    android.hardware.gatekeeper@1.0-service \
    gatekeeper.default \
    android.hardware.keymaster@4.0-service \
    libhidltransport

# Build the framework-compatible HIDL service; Qualcomm sensor backends remain
# supplied by the stock vendor image.
PRODUCT_PACKAGES += \
    android.hardware.sensors@1.0-impl \
    android.hardware.sensors@1.0-service

# The stock 32-bit OMX service lives in /vendor and resolves its VNDK
# companion libraries from the vendor namespace.
PRODUCT_PACKAGES += \
    android.hardware.media.omx@1.0 \
    android.hidl.memory@1.0 \
    libminijail

# The stock 32-bit CAS service is retained; keep its HIDL interface libraries
# available in the vendor namespace.
PRODUCT_PACKAGES += \
    android.hardware.cas@1.0 \
    android.hardware.cas@1.1 \
    android.hardware.cas@1.2 \
    android.hardware.cas.native@1.0

# The stock RIL links against legacy radio HIDL interfaces from the vendor
# namespace.
PRODUCT_PACKAGES += \
    android.hardware.radio@1.0 \
    android.hardware.radio@1.1 \
    android.hardware.radio.deprecated@1.0

# Android 11 requires health@2.1.  Use the AOSP default implementation
# instead of the stock health@1.0 prebuilt.
PRODUCT_PACKAGES += \
    android.hardware.health@2.1-service \
    android.hardware.health@2.1-impl

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootdir/init.qcom.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/fujisan.rc \
    $(LOCAL_PATH)/rootdir/init.fujisan.bluetooth.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/fujisan.bluetooth.rc \
    $(LOCAL_PATH)/rootdir/init.fujisan.wifi.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/fujisan.wifi.rc \
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
    $(LOCAL_PATH)/system/usr/keylayout/synaptics_dsx.kl:$(TARGET_COPY_OUT_SYSTEM)/usr/keylayout/synaptics_dsx.kl \
    $(LOCAL_PATH)/system/etc/permissions/android.hardware.fingerprint.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.fingerprint.xml

PRODUCT_COPY_FILES += \
    $(call find-copy-subdir-files,*,$(LOCAL_PATH)/ramdisk,$(TARGET_COPY_OUT_RAMDISK))
