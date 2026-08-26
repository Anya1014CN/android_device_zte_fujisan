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
    vendor/zte/fujisan \
    $(LOCAL_PATH)/bluetooth/libbt-vendor

# The panel exposes its native hue control through the standard LineageOS
# AIDL LiveDisplay interface.
PRODUCT_PACKAGES += \
    vendor.lineage.livedisplay-service.fujisan

PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    ro.product.first_api_level=25 \
    audio.smartpa.channel=right \
    persist.sys.usb.config.extra=none \
    sys.usb.configfs=1 \
    sys.usb.controller=6a00000.dwc3 \
    sys.usb.rndis.func.name=rndis_bam \

# Keep configfs/controller defaults, but do not force USB debugging on.
PRODUCT_PRODUCT_PROPERTIES += \
    sys.usb.configfs=1 \

PRODUCT_SYSTEM_EXT_PROPERTIES += \
    sys.usb.configfs=1 \

# Keep the modern framework-side Bluetooth stack source-built. Fujisan still
# uses the legacy wcnss_filter userspace path, so BoardConfigVendor enables the
# old wc_transport property contract for the source-built libbt-vendor.
PRODUCT_VENDOR_PROPERTIES += \
    vendor.qcom.bluetooth.soc=rome \
    bluetooth.le.disable_apcf_extended_features=1 \
    vendor.gralloc.disable_ubwc=1 \
    vendor.gralloc.enable_fb_ubwc=0 \
    persist.vendor.radio.hw_mbn_update=1 \
    persist.vendor.radio.sw_mbn_update=1 \
    persist.dbg.volte_avail_ovr=1 \
    persist.dbg.vt_avail_ovr=1 \
    ro.vendor.qti.config.zram=true \
    ro.vendor.fujisan.enable_legacy_radio=0

PRODUCT_SYSTEM_PROPERTIES += \
    ro.hardware.lights=msm8996 \
    ro.bpf.kver_override=5.4.299

# Use gestural navigation by default.
PRODUCT_PACKAGES += \
    fujisan_halld \
    FujisanCameraPanel \
    FujisanFlashlight \
    FujisanPrimaryPanel \
    android.hardware.vibrator-service.legacy \
    vibrator.default \
    android.hardware.wifi-service \
    libwifi-hal-qcom \
    wpa_supplicant \
    wpa_supplicant.conf \
    android.hardware.bluetooth@1.0-service \
    android.hardware.bluetooth@1.0-impl \
    libbt-vendor \
    FujisanNetworkStackOverlay \
    FujisanFoldFeatureOverlay \
    FujisanLargeScreenWindowOverlay \
    FujisanDesktopOverlay \
    FujisanWindowInfoProbe \
    androidx.window.sidecar \
    androidx.window.extensions \
    android.hardware.usb@1.0-service \
    android.hardware.sensors@1.0-impl \
    android.hardware.sensors@1.0-service \
    libpower.vendor \
    NavigationBarModeGesturalOverlay

PRODUCT_PACKAGES += \
    fujisan_legacy_vendor_root \
    fujisan_recovery_init_qcom \
    libtinyxml \
    hwcomposer.fujisan \
    android.hardware.security.keymint-service \
    android.hardware.graphics.allocator@2.0-impl \
    android.hardware.graphics.allocator@2.0-service \
    android.hardware.graphics.composer@2.4-service \
    android.hardware.graphics.mapper@2.0-impl \
    vendor.qti.hardware.memtrack-service

# The side fingerprint sensor is present during every boot.
PRODUCT_PACKAGES += \
    android.hardware.biometrics.fingerprint@2.1 \
    android.hardware.biometrics.fingerprint@2.1-service \
    android.hidl.base@1.0 \
    fujisan_fingerprint_blob_overlay

# The OEM msm8996 camera HAL is loaded by the standard AOSP legacy camera
# provider.
PRODUCT_PACKAGES += \
    android.hardware.camera.provider@2.4-impl:32 \
    android.hardware.camera.provider@2.4-service \
    camera.device@1.0-impl \
    camera.device@3.2-impl \
    camera.msm8996 \
    libfujisan_graphicbuffer_compat \
    libmmcamera_interface \
    libmmjpeg_interface \
    vendor.qti.hardware.camera.device@1.0

# The legacy radio stack is installed on every build. Its QCRIL blob requires
# the standard AOSP SQLite vendor variant (without an ICU/APEX dependency),
# the Lineage legacy-Protobuf compatibility library, and the Android 13
# libutils ABI used by the OEM Peripheral Manager service. Keep these
# dependencies so init can bring up the modem before framework telephony
# starts.
PRODUCT_PACKAGES += \
    libril-compat \
    libperipheral_client \
    android.hardware.radio@1.4.vendor \
    android.hardware.radio.config@1.2.vendor \
    android.hardware.radio.deprecated@1.0.vendor \
    android.hardware.secure_element@1.0.vendor \
    android.system.net.netd@1.0 \
    android.system.net.netd@1.1.vendor \
    libandroid_net \
    libjson \
    libnetutils.vendor \
    libqti_vndfwk_detect.vendor \
    librmnetctl \
    libsqlite.vendor \
    libxml2 \
    libprotobuf-cpp-full-vendorcompat \
    libutils-v33

# The Qualcomm IMS APK uses this public Qualcomm Java shared library for
# carrier-configuration handling.  LineageOS's MSM8996 reference device ships
# the same Android 16 compatible sources alongside the proprietary IMS APK.
PRODUCT_PACKAGES += \
    ims-ext-common \
    ims_ext_common.xml \
    qti-telephony-utils \
    qti_telephony_utils.xml \
    qti-telephony-hidl-wrapper \
    qti_telephony_hidl_wrapper.xml

# The matching Qualcomm IMS daemons link these standard libraries.  This is
# the same source-built pair selected by the public MSM8996 reference tree.
PRODUCT_PACKAGES += \
    libion.vendor \
    libwpa_client

# The OEM GNSS service and implementation are entirely vendor-provided.  Keep
# only the platform HIDL 1.0 interface library required by that implementation.
PRODUCT_PACKAGES += \
    android.hardware.gnss@1.0 \
    android.hardware.gnss@1.0.vendor

# Oreo vendor blobs depend on the vendor variants of the legacy HIDL runtime.
# LineageOS supplies the ABI shim for the removed Bn constructor maps.
PRODUCT_PACKAGES += \
    android.hidl.base@1.0 \
    android.hidl.base@1.0.vendor \
    android.frameworks.sensorservice@1.0.vendor \
    android.hidl.manager@1.0 \
    libhidlbase_shim \
    libhidlbase_fujisan_legacy_map_shim \
    libhidltransport.vendor \
    libhwbinder.vendor \
    libdrm.vendor

# AudioService blocks system_server startup until this standard HIDL service
# registers. The CAF msm8996 primary HAL is maintained in this device tree;
# the Oreo binary links against removed framework-private libraries.

PRODUCT_PACKAGES += \
    android.hardware.audio@6.0-impl \
    android.hardware.audio.effect@6.0-impl \
    android.hardware.audio.service \
    android.hardware.bluetooth.audio-impl \
    audio.primary.fujisan \
    audio.bluetooth.default \
    audio.r_submix.default \
    audio.usb.default \
    libaudio-resampler \
    libaudioroute.vendor \
    libdolbyshim \
    tinymix

# Android 16 HintManager requires the current AIDL power SupportInfo contract.
# Use the standard Qualcomm source service, as on maintained MSM8996 devices;
# the legacy HIDL entry in the Oreo vendor manifest remains only for blobs.
PRODUCT_PACKAGES += \
    android.hardware.power-service-qti

# The HIDL light service remains framework-provided for P1. The legacy module
# that drives the panel is brought back only with the audited display stack.
PRODUCT_PACKAGES += \
    android.hardware.light@2.0-impl \
    android.hardware.light@2.0-service

PRODUCT_PACKAGES += \
    lights.msm8996

# HardwarePropertiesManagerService acquires the declared thermal HAL during
# SystemServer startup.  The stock service is excluded from the vendor image,
# so provide the framework-compatible AOSP implementation.
PRODUCT_PACKAGES += \
    android.hardware.thermal@1.0-impl \
    android.hardware.thermal@1.0-service

# The Android 8 vendor manifest declared HIDL Keymaster 4.0, but its service
# is not part of the P1 image and Android 16 no longer provides the legacy
# wrapper.  Use AOSP's standard AIDL KeyMint service for the temporary P1
# userspace instead.  It is intentionally a bring-up implementation only: it
# is software-backed and must be replaced with a TrustZone-backed KeyMint HAL
# before this tree can be considered a secure daily-driver configuration.
#
# BiometricService requires the declared Gatekeeper HAL during SystemServer
# startup. Use the framework HIDL bridge with the existing 64-bit Qualcomm
# module, matching maintained MSM8996 device trees.
PRODUCT_PACKAGES += \
    android.hardware.gatekeeper@1.0-impl:64 \
    android.hardware.gatekeeper@1.0-service

# The stock 32-bit OMX service lives in /vendor and resolves its VNDK
# companion libraries from the vendor namespace.  Do not declare CAF OMX
# modules here: the corresponding CAF media source project is not part of
# this source manifest.
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

# The vendor manifest requires the legacy DRM 1.0 default factories.  The
# standard AOSP passthrough service supplies that instance.  Android 16's
# AOSP ClearKey implementation is AIDL and publishes its own VINTF fragment;
# do not use an Oreo Widevine service or blob.
PRODUCT_PACKAGES += \
    android.hardware.drm@1.0-impl \
    android.hardware.drm@1.0-service \
    android.hardware.drm-service.clearkey

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

# Match the AOSP charger deployment used by official LineageOS devices: install
# the platform module in /system/bin and replace the legacy vendor service from
# a vendor-context rc fragment.
PRODUCT_PACKAGES += \
    charger \
    charger_res_images

PRODUCT_PRODUCT_PROPERTIES += \
    ro.charger.enable_suspend=true \
    ro.charger.draw_split_screen=true \
    ro.charger.draw_split_offset=0

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootdir/cgroups.json:$(TARGET_COPY_OUT_VENDOR)/etc/cgroups.json \
    $(LOCAL_PATH)/configs/display/display_layout_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/displayconfig/display_layout_configuration.xml \
    $(LOCAL_PATH)/configs/android.hardware.telephony.ims.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.telephony.ims.xml \
    $(LOCAL_PATH)/system_ext/etc/permissions/privapp-permissions-org.codeaurora.ims.xml:$(TARGET_COPY_OUT_SYSTEM_EXT)/etc/permissions/privapp-permissions-org.codeaurora.ims.xml \
    $(LOCAL_PATH)/configs/task_profiles.json:$(TARGET_COPY_OUT_VENDOR)/etc/task_profiles.json \
    $(LOCAL_PATH)/rootdir/init.qcom.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/fujisan.rc \
    $(LOCAL_PATH)/rootdir/init.fujisan.qva.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/fujisan.qva.rc \
    $(LOCAL_PATH)/rootdir/init.fujisan.hwcomposer.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.fujisan.hwcomposer.rc \
    $(LOCAL_PATH)/rootdir/init.fujisan.radio.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.fujisan.radio.rc \
    $(LOCAL_PATH)/rootdir/init.fujisan.usb.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/fujisan.usb.rc \
    $(LOCAL_PATH)/rootdir/init.fujisan.charger.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/zz-fujisan-charger.rc \
    $(LOCAL_PATH)/rootdir/firmware/.placeholder:$(TARGET_COPY_OUT_RAMDISK)/firmware/.placeholder \
    $(LOCAL_PATH)/rootdir/dsp/.placeholder:$(TARGET_COPY_OUT_RAMDISK)/dsp/.placeholder \
    $(LOCAL_PATH)/rootdir/persist/.placeholder:$(TARGET_COPY_OUT_RAMDISK)/persist/.placeholder \
    $(LOCAL_PATH)/rootdir/etc/fstab.ramdisk.qcom:$(TARGET_COPY_OUT_RAMDISK)/fstab.qcom \
    $(LOCAL_PATH)/rootdir/etc/fstab.qcom:$(TARGET_COPY_OUT_VENDOR)/etc/fstab.qcom \
    $(LOCAL_PATH)/system/usr/idc/goodix-touchscreen.idc:$(TARGET_COPY_OUT_SYSTEM)/usr/idc/goodix-touchscreen.idc \
    $(LOCAL_PATH)/system/usr/idc/synaptics_dsx.idc:$(TARGET_COPY_OUT_SYSTEM)/usr/idc/synaptics_dsx.idc \
    $(LOCAL_PATH)/system/usr/idc/zte-touchscreen.idc:$(TARGET_COPY_OUT_SYSTEM)/usr/idc/zte-touchscreen.idc \
    $(LOCAL_PATH)/system/usr/idc/zte-touchscreen-2nd.idc:$(TARGET_COPY_OUT_SYSTEM)/usr/idc/zte-touchscreen-2nd.idc \
    $(LOCAL_PATH)/system/usr/idc/zte-touchsrceen-3nd.idc:$(TARGET_COPY_OUT_SYSTEM)/usr/idc/zte-touchsrceen-3nd.idc \
    $(LOCAL_PATH)/system/usr/keylayout/gpio-keys.kl:$(TARGET_COPY_OUT_SYSTEM)/usr/keylayout/gpio-keys.kl \
    $(LOCAL_PATH)/system/usr/keylayout/qpnp_pon.kl:$(TARGET_COPY_OUT_SYSTEM)/usr/keylayout/qpnp_pon.kl \
    $(LOCAL_PATH)/system/usr/keylayout/synaptics_dsx.kl:$(TARGET_COPY_OUT_SYSTEM)/usr/keylayout/synaptics_dsx.kl

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/audio/audio_effects.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_effects.xml \
    $(LOCAL_PATH)/audio/audio_output_policy.conf:$(TARGET_COPY_OUT_VENDOR)/etc/audio_output_policy.conf \
    $(LOCAL_PATH)/audio/mixer_paths.xml:$(TARGET_COPY_OUT_VENDOR)/etc/mixer_paths.xml \
    $(LOCAL_PATH)/audio/audio_platform_info.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_platform_info.xml \
    $(LOCAL_PATH)/audio/audio_platform_info_i2s.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_platform_info_i2s.xml \
    vendor/zte/fujisan/proprietary/vendor/etc/audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_configuration.xml \
    vendor/zte/fujisan/proprietary/vendor/etc/audio_policy_engine_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_engine_configuration.xml \
    vendor/zte/fujisan/proprietary/vendor/etc/audio_policy_engine_product_strategies.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_engine_product_strategies.xml \
    vendor/zte/fujisan/proprietary/vendor/etc/audio_policy_engine_stream_volumes.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_engine_stream_volumes.xml \
    vendor/zte/fujisan/proprietary/vendor/etc/audio_policy_engine_default_stream_volumes.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_engine_default_stream_volumes.xml \
    frameworks/av/services/audiopolicy/config/audio_policy_volumes.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_volumes.xml \
    frameworks/av/services/audiopolicy/config/default_volume_tables.xml:$(TARGET_COPY_OUT_VENDOR)/etc/default_volume_tables.xml \
    frameworks/av/services/audiopolicy/config/bluetooth_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/bluetooth_audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/usb_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/usb_audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/r_submix_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/r_submix_audio_policy_configuration.xml \
    frameworks/av/media/libstagefright/data/media_codecs_google_audio.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_google_audio.xml \
    frameworks/av/media/libstagefright/data/media_codecs_google_telephony.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_google_telephony.xml \
    frameworks/av/media/libstagefright/data/media_codecs_google_video.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_google_video.xml

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootdir/init.fujisan.hall.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.fujisan.hall.rc \
    $(LOCAL_PATH)/rootdir/init.fujisan.wifi.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/init.fujisan.wifi.rc \
    $(LOCAL_PATH)/rootdir/bin/init.fujisan.btaddr.sh:$(TARGET_COPY_OUT_VENDOR)/bin/init.fujisan.btaddr.sh \
    $(LOCAL_PATH)/rootdir/init.fujisan.bluetooth.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/fujisan.bluetooth.rc

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/system/etc/default-permissions/com.zte.fujisan.camerapanel.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/default-permissions/com.zte.fujisan.camerapanel.xml \
    $(LOCAL_PATH)/system/etc/permissions/privapp-permissions-com.zte.fujisan.flashlight.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/privapp-permissions-com.zte.fujisan.flashlight.xml \
    $(LOCAL_PATH)/configs/permissions/android.software.freeform_window_management.xml:$(TARGET_COPY_OUT_PRODUCT)/etc/permissions/android.software.freeform_window_management.xml \
    $(LOCAL_PATH)/configs/devicestate/device_state_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/devicestate/device_state_configuration.xml

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootdir/etc/seccomp_policy/mediacodec.policy:$(TARGET_COPY_OUT_VENDOR)/etc/seccomp_policy/mediacodec.policy \
    $(LOCAL_PATH)/rootdir/bt_firmware/.placeholder:$(TARGET_COPY_OUT_RAMDISK)/bt_firmware/.placeholder

# PackageManager must always advertise the physical fingerprint sensor.
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/system/etc/permissions/android.hardware.fingerprint.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.fingerprint.xml

PRODUCT_COPY_FILES += \
    $(call find-copy-subdir-files,*,$(LOCAL_PATH)/ramdisk,$(TARGET_COPY_OUT_RAMDISK))
