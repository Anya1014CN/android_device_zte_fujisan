LOCAL_PATH := device/zte/fujisan

$(call inherit-product, $(SRC_TARGET_DIR)/product/product_launched_with_n_mr1.mk)
$(call inherit-product, vendor/zte/fujisan/fujisan-vendor.mk)

DEVICE_PACKAGE_OVERLAYS += \
    $(LOCAL_PATH)/overlay

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
    bpf.progs_loaded=1 \
    audio.smartpa.channel=right \
    ro.adb.secure=0 \
    persist.sys.usb.config=adb \
    sys.usb.config=adb \
    sys.usb.configfs=0 \
    sys.usb.controller=6a00000.dwc3 \

# Keep the modern framework-side Bluetooth stack source-built. Fujisan still
# uses the legacy wcnss_filter userspace path, so BoardConfigVendor enables the
# old wc_transport property contract for the source-built libbt-vendor.
PRODUCT_VENDOR_PROPERTIES += \
    vendor.qcom.bluetooth.soc=rome

# Use gestural navigation by default.
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

# Keep the Bluetooth HIDL service/impl and the Qualcomm transport
# library source-built. Device-specific compatibility lives in
# BoardConfigVendor rather than by shipping a second prebuilt libbt-vendor.
PRODUCT_PACKAGES += \
    android.hardware.bluetooth@1.0-service \
    android.hardware.bluetooth@1.0-impl \
    libbt-vendor

# The FPC service itself remains proprietary on this device.  Keep only the
# standard framework interface libraries and wrapper service source-built, then
# replace the installed wrapper binary with the proprietary FPC implementation
# at packaging time.  This keeps the standard init/VINTF wiring while avoiding
# the incompatible AOSP implementation at runtime.
PRODUCT_PACKAGES += \
    android.hardware.biometrics.fingerprint@2.1 \
    android.hardware.biometrics.fingerprint@2.1-service \
    android.hidl.base@1.0 \
    fujisan_fingerprint_blob_overlay

# Oreo vendor blobs depend on vendor-visible HIDL runtime libraries. The
# legacy interface sonames android.hidl.base@1.0.so and
# android.hidl.manager@1.0.so no longer build as standalone libraries on
# Android 11, so rootdir/Android.mk provides vendor-side compatibility
# symlinks to the source-built libhidlbase.so instead.
PRODUCT_PACKAGES += \
    libhidltransport.vendor \
    libhwbinder.vendor

# Follow the msm8996 LineageOS pattern: keep the legacy HIDL service stack
# and the primary/amplifier audio HALs source-built, while shipping only the
# device-specific calibration, mixer, firmware, and smartpa userspace blobs.
PRODUCT_PACKAGES += \
    audiod \
    android.hardware.audio@2.0 \
    android.hardware.audio.common@2.0 \
    android.hardware.audio.effect@2.0 \
    android.hardware.audio@2.0-impl \
    android.hardware.audio.effect@2.0-impl \
    android.hardware.audio@2.0-service \
    android.hardware.audio@6.0 \
    android.hardware.audio.common@6.0 \
    android.hardware.audio.common@6.0-util \
    android.hardware.audio@6.0-impl \
    android.hardware.audio.effect@6.0 \
    android.hardware.audio.effect@6.0-impl \
    audio.bluetooth.default \
    audio.primary.msm8996 \
    audio.r_submix.default \
    audio.usb.default \
    audio_amplifier.msm8996 \
    libaudio-resampler \
    libaudioroute \
    libqcompostprocbundle \
    libqcomvisualizer \
    libqcomvoiceprocessing \
    libvolumelistener \
    tinymix

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

# Keep the framework-side Gatekeeper interface available for Keystore.
PRODUCT_PACKAGES += \
    android.hardware.gatekeeper@1.0 \
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
    android.hardware.radio.deprecated@1.0 \
    libhidlbase \
    libhwbinder \
    librmnetctl \
    libxml2 \
    libprotobuf-cpp-full

# Android 12L requires health@2.1.  Use the AOSP default implementation
# instead of the stock health@1.0 prebuilt.
PRODUCT_PACKAGES += \
    android.hardware.health@2.1-service \
    android.hardware.health@2.1-impl

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/audio/audio_effects.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_effects.xml \
    $(LOCAL_PATH)/audio/audio_output_policy.conf:$(TARGET_COPY_OUT_VENDOR)/etc/audio_output_policy.conf \
    $(LOCAL_PATH)/audio/audio_platform_info.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_platform_info.xml \
    $(LOCAL_PATH)/audio/audio_platform_info_i2s.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_platform_info_i2s.xml \
    $(LOCAL_PATH)/audio/audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_configuration.xml \
    $(LOCAL_PATH)/audio/audio_tuning_mixer.txt:$(TARGET_COPY_OUT_VENDOR)/etc/audio_tuning_mixer.txt \
    $(LOCAL_PATH)/audio/mixer_paths.xml:$(TARGET_COPY_OUT_VENDOR)/etc/mixer_paths.xml \
    $(LOCAL_PATH)/audio/sound_trigger_mixer_paths.xml:$(TARGET_COPY_OUT_VENDOR)/etc/sound_trigger_mixer_paths.xml \
    $(LOCAL_PATH)/audio/sound_trigger_mixer_paths_wcd9330.xml:$(TARGET_COPY_OUT_VENDOR)/etc/sound_trigger_mixer_paths_wcd9330.xml \
    $(LOCAL_PATH)/audio/sound_trigger_platform_info.xml:$(TARGET_COPY_OUT_VENDOR)/etc/sound_trigger_platform_info.xml \
    frameworks/av/services/audiopolicy/config/audio_policy_volumes.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_volumes.xml \
    frameworks/av/services/audiopolicy/config/default_volume_tables.xml:$(TARGET_COPY_OUT_VENDOR)/etc/default_volume_tables.xml \
    frameworks/av/services/audiopolicy/config/a2dp_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/a2dp_audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/usb_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/usb_audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/r_submix_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/r_submix_audio_policy_configuration.xml \
    system/core/libprocessgroup/profiles/cgroups_28.json:$(TARGET_COPY_OUT_VENDOR)/etc/cgroups.json \
    system/core/libprocessgroup/profiles/task_profiles_28.json:$(TARGET_COPY_OUT_VENDOR)/etc/task_profiles.json \
    frameworks/av/media/libstagefright/data/media_codecs_google_audio.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_google_audio.xml \
    frameworks/av/media/libstagefright/data/media_codecs_google_telephony.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_google_telephony.xml \
    frameworks/av/media/libstagefright/data/media_codecs_google_video.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_google_video.xml \
    $(LOCAL_PATH)/rootdir/bin/init.fujisan.btaddr.sh:$(TARGET_COPY_OUT_SYSTEM)/bin/init.fujisan.btaddr.sh \
    $(LOCAL_PATH)/rootdir/bin/fujisan_bootlog.sh:$(TARGET_COPY_OUT_SYSTEM)/bin/fujisan_bootlog.sh \
    $(LOCAL_PATH)/rootdir/bin/fujisan_usb.sh:$(TARGET_COPY_OUT_SYSTEM)/bin/fujisan_usb.sh \
    $(LOCAL_PATH)/rootdir/bin/fujisan_bpfloader.sh:$(TARGET_COPY_OUT_SYSTEM)/bin/fujisan_bpfloader \
    $(LOCAL_PATH)/rootdir/init.qcom.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/fujisan.rc \
    $(LOCAL_PATH)/rootdir/init.fujisan.bootlog.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/fujisan.bootlog.rc \
    $(LOCAL_PATH)/rootdir/init.fujisan.bpf.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/fujisan.bpf.rc \
    $(LOCAL_PATH)/rootdir/etc/init/a-fujisan-bpfloader.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/a-fujisan-bpfloader.rc \
    $(LOCAL_PATH)/rootdir/init.fujisan.hwcomposer.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/fujisan.hwcomposer.rc \
    $(LOCAL_PATH)/rootdir/init.fujisan.usb.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/fujisan.usb.rc \
    $(LOCAL_PATH)/rootdir/init.fujisan.bluetooth.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/fujisan.bluetooth.rc \
    $(LOCAL_PATH)/rootdir/init.fujisan.wifi.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/fujisan.wifi.rc \
    $(LOCAL_PATH)/rootdir/firmware/.placeholder:$(TARGET_COPY_OUT_RAMDISK)/firmware/.placeholder \
    $(LOCAL_PATH)/rootdir/bt_firmware/.placeholder:$(TARGET_COPY_OUT_RAMDISK)/bt_firmware/.placeholder \
    $(LOCAL_PATH)/rootdir/dsp/.placeholder:$(TARGET_COPY_OUT_RAMDISK)/dsp/.placeholder \
    $(LOCAL_PATH)/rootdir/persist/.placeholder:$(TARGET_COPY_OUT_RAMDISK)/persist/.placeholder \
    $(LOCAL_PATH)/rootdir/etc/fstab.ramdisk.qcom:$(TARGET_COPY_OUT_RAMDISK)/fstab.qcom \
    $(LOCAL_PATH)/rootdir/etc/fstab.qcom:$(TARGET_COPY_OUT_VENDOR)/etc/fstab.qcom \
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

# Fujisan uses the panel-hue LiveDisplay HAL above.  The generic SDM service
# is for Qualcomm SDM display stacks and crashes here after failing to load
# libsdm-disp-vndapis.  Exclude both install variants at product definition
# time; do not ship an init override for a service this device never uses.


PRODUCT_PACKAGES := $(filter-out \
    lineage.livedisplay@2.0-service-sdm \
    vendor.lineage.livedisplay@2.0-service-sdm, \
    $(PRODUCT_PACKAGES))
