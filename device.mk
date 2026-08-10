LOCAL_PATH := device/zte/fujisan

# P1 builds only the services needed to reach a single-screen Android userspace.
# Re-enable a deferred hardware group only while working on that subsystem.
FUJISAN_ENABLE_DEFERRED_HARDWARE ?= false

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

PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    ro.product.first_api_level=25 \
    audio.smartpa.channel=right \
    persist.sys.usb.config=adb \
    persist.sys.usb.config.extra=none \
    sys.usb.config=adb \
    sys.usb.configfs=1 \
    sys.usb.controller=6a00000.dwc3 \
    sys.usb.rndis.func.name=rndis_bam \

# product/system_ext defaults ship persist.sys.usb.config=none; override them.
# Settings ships its 12L two-pane implementation but leaves it behind a
# feature flag on phone products. Extended mode is a 2160x1920 logical display
# (720dp wide at the device density), so opt into that existing code without a
# Settings fork.
PRODUCT_PRODUCT_PROPERTIES += \
    persist.sys.usb.config=adb \
    persist.sys.usb.config.extra=none \
    sys.usb.configfs=1 \
    persist.sys.fflag.override.settings_support_large_screen=true \

PRODUCT_SYSTEM_EXT_PROPERTIES += \
    persist.sys.usb.config=adb \
    persist.sys.usb.config.extra=none \
    sys.usb.configfs=1 \

# Keep the modern framework-side Bluetooth stack source-built. Fujisan still
# uses the legacy wcnss_filter userspace path, so BoardConfigVendor enables the
# old wc_transport property contract for the source-built libbt-vendor.
PRODUCT_VENDOR_PROPERTIES += \
    vendor.qcom.bluetooth.soc=rome \
    vendor.gralloc.disable_ubwc=1 \
    vendor.gralloc.enable_fb_ubwc=0 \
    ro.vendor.fujisan.enable_legacy_radio=0

# Use gestural navigation by default.
PRODUCT_PACKAGES += \
    fujisan_halld \
    android.hardware.usb@1.0-service \
    NavigationBarModeGesturalOverlay

ifeq ($(FUJISAN_ENABLE_DEFERRED_HARDWARE),true)
PRODUCT_PACKAGES += \
    FujisanFlashlight \
    FujisanCameraPanel \
    FujisanPrimaryPanel \
    FujisanWindowInfoProbe \
    FujisanRotationOverlay \
    FujisanNetworkStackOverlay \
    androidx.window.sidecar \
    androidx.window.extensions
endif

PRODUCT_PACKAGES += \
    fujisan_legacy_vendor_root \
    libtinyxml \
    android.hardware.security.keymint-service \
    android.hardware.graphics.allocator@2.0-impl \
    android.hardware.graphics.allocator@2.0-service \
    android.hardware.graphics.composer@2.1-service \
    android.hardware.graphics.mapper@2.0-impl \
    vendor.qti.hardware.memtrack-service

# LineageOS 23.2 no longer ships the MSM8996 CAF display project.  The
# matching HWC/gralloc implementation is an OEM closed component and must be
# reinstated only after its linker/VINTF audit during the single-panel stage.
ifeq ($(FUJISAN_ENABLE_DEFERRED_HARDWARE),true)
PRODUCT_PACKAGES += \
    bugreports_root_dir \
    copybit.msm8996 \
    gralloc.msm8996 \
    hwcomposer.msm8996 \
    libdisplayconfig \
    liboverlay \
    libqdMetaData.system
endif

# Fujisan's hinge topology is implemented by this device-side HWC2 wrapper.
# The wrapper loads the OEM msm8996 HWC internally; keep it independent from
# the deferred camera/radio hardware group.
PRODUCT_PACKAGES += \
    hwcomposer.fujisan

ifeq ($(FUJISAN_ENABLE_DEFERRED_HARDWARE),true)
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

# Keep the Bluetooth HIDL service/impl and the Qualcomm transport
# library source-built. Device-specific compatibility lives in
# BoardConfigVendor rather than by shipping a second prebuilt libbt-vendor.
PRODUCT_PACKAGES += \
    android.hardware.bluetooth@1.0-service \
    android.hardware.bluetooth@1.0-impl \
    libbt-vendor

# The OEM image provides the modem-facing location stack. Add only the
# framework-facing GNSS HIDL bridge; libloc_core and libgps.utils stay OEM.
PRODUCT_PACKAGES += \
    libgnss \
    libgnsspps \
    android.hardware.gnss@1.0-impl-qti \
    android.hardware.gnss@1.0-service-qti

# The Oreo Qualcomm data stack consumes its private xmllib parser ABI while
# loading netmgr_config.xml.  Use the matching OEM parser blob; the AOSP
# libxml2 API is not ABI-compatible with this legacy Qualcomm surface.
PRODUCT_PACKAGES += \
    libril-compat \
    libprotobuf-cpp-full-vendorcompat

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
endif

# Oreo vendor blobs depend on the vendor variants of the legacy HIDL runtime.
# LineageOS supplies the ABI shim for the removed Bn constructor maps.
PRODUCT_PACKAGES += \
    android.hidl.base@1.0 \
    android.hidl.manager@1.0 \
    libhidlbase_shim \
    libhidlbase_fujisan_legacy_map_shim \
    libhidltransport.vendor \
    libhwbinder.vendor

# AudioService blocks system_server startup until this standard HIDL service
# registers, so it is a P0 service rather than deferred hardware.  The CAF
# msm8996 primary HAL is maintained in this device tree; the Oreo binary links
# against removed framework-private libraries.

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

ifeq ($(FUJISAN_ENABLE_DEFERRED_HARDWARE),true)
PRODUCT_PACKAGES += \
    lights.msm8996
endif

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

# Keymaster remains deferred: its legacy OEM implementation requires a
# separate linker and interface audit.
ifeq ($(FUJISAN_ENABLE_DEFERRED_HARDWARE),true)
PRODUCT_PACKAGES += \
    android.hardware.keymaster@4.0-service \
    libhidltransport

# Fujisan exposes its Qualcomm sensor backends through the standard multi-HAL
# configuration in vendor/etc/sensors/hals.conf.  Use the source-built AOSP
# HIDL bridge; it loads those OEM backend modules without an Android 8 HIDL
# frontend ABI dependency.
PRODUCT_PACKAGES += \
    android.hardware.sensors@1.0-impl \
    android.hardware.sensors@1.0-service

# Use the msm8996 CAF V4L2 OMX implementation with the standard AOSP OMX
# service.  The kernel already exposes the Venus VIDC decoder/encoder nodes;
# these source-built libraries provide the missing userspace registration for
# all hardware codecs (AVC, HEVC, VP8/9, MPEG-2/4, H.263, VC-1 and DivX).
# Do not rely on incompatible Oreo video codec blobs.
PRODUCT_PACKAGES += \
    android.hardware.media.omx@1.0 \
    android.hidl.memory@1.0 \
    libminijail \
    libOmxCore \
    libOmxVdec \
    libOmxVenc \
    libstagefrighthw \
    libhypv_intercept \
    libgpustats \
    libc2d30-a5xx \
    libc2d30_bltlib

# The stock 32-bit CAS service is retained; keep its HIDL interface libraries
# available in the vendor namespace.
PRODUCT_PACKAGES += \
    android.hardware.cas@1.0 \
    android.hardware.cas@1.1 \
    android.hardware.cas@1.2 \
    android.hardware.cas.native@1.0

# The vendor manifest requires the legacy DRM 1.0 default factories.  The
# standard AOSP passthrough service supplies that instance and loads the
# source-built ClearKey plugin already installed under vendor/mediadrm.  Keep
# the AOSP ClearKey service for its explicit modern instance; do not use an
# Oreo Widevine service or blob on Android 12.
PRODUCT_PACKAGES += \
    android.hardware.drm@1.0-impl \
    android.hardware.drm@1.0-service \
    android.hardware.drm@1.4-service.clearkey

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
endif

# Android 12L requires health@2.1.  Use the AOSP default implementation
# instead of the stock health@1.0 prebuilt.
PRODUCT_PACKAGES += \
    android.hardware.health@2.1-service \
    android.hardware.health@2.1-impl

# The 4.4 kernel cannot run Android 16's mainline eBPF program set. Keep netd
# on the AOSP implementation and provide only its legacy eBPF ABI bridge.
PRODUCT_PACKAGES += \
    libnetd_fujisan_compat \
    netd_bpf_compat

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
    system/core/libprocessgroup/profiles/task_profiles_28.json:$(TARGET_COPY_OUT_VENDOR)/etc/task_profiles.json \
    $(LOCAL_PATH)/rootdir/init.qcom.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/fujisan.rc \
    $(LOCAL_PATH)/rootdir/zz-fujisan-bpfloader.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/zz-fujisan-bpfloader.rc \
    $(LOCAL_PATH)/rootdir/zz-fujisan-netd-compat.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/zz-fujisan-netd-compat.rc \
    $(LOCAL_PATH)/rootdir/init.fujisan.hwcomposer.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/fujisan.hwcomposer.rc \
    $(LOCAL_PATH)/rootdir/zz-fujisan-hwc2-compat.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/zz-fujisan-hwc2-compat.rc \
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
    $(LOCAL_PATH)/rootdir/init.dualscreen.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/init.dualscreen.rc \
    $(LOCAL_PATH)/rootdir/init.fujisan.hall.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.fujisan.hall.rc

ifeq ($(FUJISAN_ENABLE_DEFERRED_HARDWARE),true)
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootdir/bin/init.fujisan.btaddr.sh:$(TARGET_COPY_OUT_SYSTEM)/bin/init.fujisan.btaddr.sh \
    $(LOCAL_PATH)/system/etc/default-permissions/com.zte.fujisan.camerapanel.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/default-permissions/com.zte.fujisan.camerapanel.xml \
    $(LOCAL_PATH)/configs/permissions/android.software.freeform_window_management.xml:$(TARGET_COPY_OUT_PRODUCT)/etc/permissions/android.software.freeform_window_management.xml \
    $(LOCAL_PATH)/configs/display/display_settings.xml:$(TARGET_COPY_OUT_VENDOR)/etc/display_settings.xml \
    $(LOCAL_PATH)/configs/devicestate/device_state_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/devicestate/device_state_configuration.xml \
    $(LOCAL_PATH)/rootdir/etc/seccomp_policy/mediacodec.policy:$(TARGET_COPY_OUT_VENDOR)/etc/seccomp_policy/mediacodec.policy \
    $(LOCAL_PATH)/rootdir/init.fujisan.bluetooth.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/fujisan.bluetooth.rc \
    $(LOCAL_PATH)/rootdir/init.fujisan.wifi.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/fujisan.wifi.rc \
    $(LOCAL_PATH)/rootdir/bt_firmware/.placeholder:$(TARGET_COPY_OUT_RAMDISK)/bt_firmware/.placeholder \
    $(LOCAL_PATH)/system/etc/permissions/android.hardware.fingerprint.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/android.hardware.fingerprint.xml
endif

PRODUCT_COPY_FILES += \
    $(call find-copy-subdir-files,*,$(LOCAL_PATH)/ramdisk,$(TARGET_COPY_OUT_RAMDISK))
