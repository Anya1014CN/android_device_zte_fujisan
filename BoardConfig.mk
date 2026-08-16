DEVICE_PATH := device/zte/fujisan
BOARD_VENDOR := zte
TARGET_SPECIFIC_HEADER_PATH := $(DEVICE_PATH)/include

# Platform
TARGET_BOARD_PLATFORM := msm8996
TARGET_BOARD_PLATFORM_GPU := qcom-adreno530
TARGET_BOOTLOADER_BOARD_NAME := fujisan
TARGET_NO_BOOTLOADER := true
TARGET_NO_RADIOIMAGE := true
BOARD_USES_QCOM_HARDWARE := true
AB_OTA_UPDATER := false

# LineageOS 23.2 no longer publishes the MSM8996 CAF display project.  Keep
# the generic CAF configuration in use, but satisfy its header-only
# SurfaceFlinger contract from this device tree instead of selecting another
# platform's display stack.
QCOM_SOONG_NAMESPACE := $(DEVICE_PATH)

# Android 12L still accepts the legacy system/vendor layout used by the
# Oreo firmware.  Permit the prebuilt product copies from that image while
# keeping the device out of the unsupported VNDK-Lite path.
BUILD_BROKEN_ELF_PREBUILT_PRODUCT_COPY_FILES := true

# Architecture
TARGET_ARCH := arm64
TARGET_ARCH_VARIANT := armv8-a
TARGET_CPU_ABI := arm64-v8a
TARGET_CPU_ABI2 :=
TARGET_CPU_VARIANT := kryo

TARGET_2ND_ARCH := arm
TARGET_2ND_ARCH_VARIANT := armv8-a
TARGET_2ND_CPU_ABI := armeabi-v7a
TARGET_2ND_CPU_ABI2 := armeabi
TARGET_2ND_CPU_VARIANT := kryo

# Legacy vendor layout
TARGET_COPY_OUT_VENDOR := system/vendor
TARGET_SYSTEM_PROP += $(DEVICE_PATH)/system.prop

# File capabilities required by the imported Qualcomm IMS daemons.
TARGET_FS_CONFIG_GEN := $(DEVICE_PATH)/config.fs

# Audio: use the source-built CAF msm8996 ALSA HAL.  The Fujisan card is an
# AK4962 SLIMbus codec, so the generic AOSP in-memory primary HAL cannot
# drive its mixer routes or capture paths.
USE_XML_AUDIO_POLICY_CONF := 1
USE_CUSTOM_AUDIO_POLICY := 1
BOARD_USES_ALSA_AUDIO := true
# The Fujisan AK4962/TFA path is configured by the device mixer routes.  Do
# not request CAF's optional audio_amplifier.* plugin: this device has none.
AUDIO_FEATURE_ENABLED_EXT_AMPLIFIER := false
BOARD_SUPPORTS_SOUND_TRIGGER := false

# Kernel
BOARD_KERNEL_CMDLINE := console=ttyHSL0,115200,n8 androidboot.console=ttyHSL0 androidboot.hardware=qcom user_debug=31 msm_rtb.filter=0x237 ehci-hcd.park=3 cma=32M@0-0xffffffff
BOARD_KERNEL_CMDLINE += androidboot.selinux=permissive
BOARD_KERNEL_CMDLINE += watchdog_v2.enable=0
BOARD_KERNEL_CMDLINE += panic=3
BOARD_KERNEL_CMDLINE += printk.always_kmsg_dump=1
BOARD_KERNEL_BASE := 0x80000000
BOARD_KERNEL_PAGESIZE := 4096
BOARD_MKBOOTIMG_ARGS := --ramdisk_offset 0x01000000 --tags_offset 0x00000100
BOARD_KERNEL_IMAGE_NAME := Image.gz-dtb
TARGET_KERNEL_ARCH := arm64
TARGET_KERNEL_HEADER_ARCH := arm64
TARGET_KERNEL_SOURCE := kernel/zte/msm8996
TARGET_KERNEL_VERSION := 4.4
TARGET_KERNEL_CONFIG := lineageos_fujisan_defconfig
# The legacy bootloader verifies an AOSP BootSignature DER trailer rather than
# an AVB footer. Keep that device-specific post-processing outside AOSP.
BOARD_CUSTOM_BOOTIMG_MK := $(DEVICE_PATH)/bootimg.mk
# Target-files normally reconstructs boot.img from BOOT/.  Preserve the
# post-processed BootSignature trailer by copying the installed boot image.
BOARD_COPY_BOOT_IMAGE_TO_TARGET_FILES := true
# The 4.4 msm8996 base builds a 32-bit compat vDSO with clang.
TARGET_KERNEL_MAKE_ENV += CROSS_COMPILE_ARM32=arm-linux-gnueabi-
TARGET_KERNEL_MAKE_ENV += CLANG_TRIPLE_ARM32=arm-linux-androideabi-
NEED_KERNEL_MODULE_ROOT := true

# Partitions
BOARD_BOOTIMAGE_PARTITION_SIZE := 67108864
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 67108864
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 6979321856
BOARD_SYSTEMIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_CACHEIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_CACHEIMAGE_PARTITION_SIZE := 1718616064
BOARD_USERDATAIMAGE_PARTITION_SIZE := 52766424576
BOARD_PERSISTIMAGE_PARTITION_SIZE := 33554432
BOARD_FLASH_BLOCK_SIZE := 262144
BOARD_HAS_NO_REAL_SDCARD := true
TARGET_USERIMAGES_USE_EXT4 := true
TARGET_USERIMAGES_USE_F2FS := true
TARGET_USES_MKE2FS := true
BOARD_SUPPRESS_SECURE_ERASE := true

# Ramdisk layout
BOARD_ROOT_EXTRA_FOLDERS += \
    acct \
    bt_firmware \
    bugreports \
    cache \
    config \
    data \
    dsp \
    firmware \
    mnt \
    oem \
    persist \
    storage \
    system

BOARD_ROOT_EXTRA_SYMLINKS += \
    /vendor/lib/dsp:/dsp

# VINTF
DEVICE_FRAMEWORK_COMPATIBILITY_MATRIX_FILE += \
    hardware/qcom-caf/common/vendor_framework_compatibility_matrix.xml \
    hardware/qcom-caf/common/vendor_framework_compatibility_matrix_legacy.xml \
    $(DEVICE_PATH)/framework_compatibility_matrix.xml
DEVICE_MANIFEST_FILE := vendor/zte/fujisan/proprietary/vendor/manifest.xml
DEVICE_MATRIX_FILE := vendor/zte/fujisan/proprietary/vendor/compatibility_matrix.xml

# Display
# Fujisan's OEM camera HAL uses the device-specific MCT capability ABI.
BOARD_QTI_CAMERA_32BIT_ONLY := true
TARGET_SUPPORT_HAL1 := false
TARGET_USES_FUJISAN_SOURCE_QCAMERA := false

BOARD_USES_ADRENO := true
TARGET_CONTINUOUS_SPLASH_ENABLED := true
TARGET_SCREEN_DENSITY := 480
MAX_VIRTUAL_DISPLAY_DIMENSION := 4096
NUM_FRAMEBUFFER_SURFACE_BUFFERS := 3
TARGET_FORCE_HWC_FOR_VIRTUAL_DISPLAYS := false
TARGET_USES_GRALLOC1 := true
TARGET_USES_HWC2 := true
TARGET_USES_ION := true
TARGET_USES_OVERLAY := true
USE_OPENGL_RENDERER := true
MAX_EGL_CACHE_KEY_SIZE := 12*1024
MAX_EGL_CACHE_SIZE := 2048*1024
OVERRIDE_RS_DRIVER := libRSDriver_adreno.so

# GPS / radio
TARGET_NO_RPC := true
USE_DEVICE_SPECIFIC_GPS := true
BOARD_VENDOR_QCOM_GPS_LOC_API_HARDWARE := default
BOARD_VENDOR_QCOM_LOC_PDK_FEATURE_SET := true
TARGET_RIL_VARIANT := caf
# The device supplies its own dual-SIM RIL services in vendor init.
ENABLE_VENDOR_RIL_SERVICE := true

# Wi-Fi
# QCACLD is built into the 4.4 kernel, but its built-in init is deliberately
# deferred until libwifi-hal selects a firmware mode through this parameter.
# These are the standard msm8996 qcwcn declarations used by AOSP/LineageOS.
BOARD_WLAN_DEVICE := qcwcn
BOARD_WPA_SUPPLICANT_DRIVER := NL80211
WIFI_HIDL_UNIFIED_SUPPLICANT_SERVICE_RC_ENTRY := true
# QCWCN reports its concurrency matrix dynamically.  Declare the matching
# single-STA V3 mode up front, as done by the LineageOS msm8996 common tree,
# so the AIDL Wi-Fi HAL keeps the configured mode after reloading that matrix.
WIFI_HAL_INTERFACE_COMBINATIONS := {{{STA}, 1}}
WIFI_DRIVER_FW_PATH_PARAM := "/sys/module/wlan/parameters/fwpath"
WIFI_DRIVER_FW_PATH_STA := "sta"
WIFI_DRIVER_FW_PATH_AP := "ap"
WIFI_DRIVER_FW_PATH_P2P := "p2p"

# Recovery
TARGET_RECOVERY_FSTAB := $(DEVICE_PATH)/rootdir/etc/fstab.qcom
TARGET_RECOVERY_PIXEL_FORMAT := RGBX_8888
RECOVERY_GRAPHICS_USE_LINELENGTH := true

# Keep user-installed recovery images intact when installing a non-A/B OTA.
TARGET_RELEASETOOLS_EXTENSIONS := $(DEVICE_PATH)/releasetools
BOARD_HAS_NO_SELECT_BUTTON := true
BOARD_USES_MMCUTILS := true

# SELinux
include device/qcom/sepolicy-legacy-um/SEPolicy.mk
BOARD_VENDOR_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy
BOARD_VENDOR_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy/vendor
SELINUX_IGNORE_NEVERALLOWS := true

-include vendor/zte/fujisan/BoardConfigVendor.mk
