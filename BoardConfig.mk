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

# Audio
AUDIO_FEATURE_ENABLED_ACDB_LICENSE := true
AUDIO_FEATURE_ENABLED_ALAC_OFFLOAD := true
AUDIO_FEATURE_ENABLED_ANC_HEADSET := true
AUDIO_FEATURE_ENABLED_COMPRESS_VOIP := true
AUDIO_FEATURE_ENABLED_DEV_ARBI := true
AUDIO_FEATURE_ENABLED_EXTENDED_COMPRESS_FORMAT := true
AUDIO_FEATURE_ENABLED_EXTN_FORMATS := true
AUDIO_FEATURE_ENABLED_FLAC_OFFLOAD := true
AUDIO_FEATURE_ENABLED_FLUENCE := true
AUDIO_FEATURE_ENABLED_HFP := true
AUDIO_FEATURE_ENABLED_KPI_OPTIMIZE := true
AUDIO_FEATURE_ENABLED_MULTI_VOICE_SESSIONS := true
AUDIO_FEATURE_ENABLED_NT_PAUSE_TIMEOUT := true
AUDIO_FEATURE_ENABLED_PCM_OFFLOAD := true
AUDIO_FEATURE_ENABLED_PCM_OFFLOAD_24 := true
AUDIO_FEATURE_ENABLED_PROXY_DEVICE := true
AUDIO_USE_LL_AS_PRIMARY_OUTPUT := true
BOARD_SUPPORTS_SOUND_TRIGGER := true
BOARD_USES_ALSA_AUDIO := true
USE_CUSTOM_AUDIO_POLICY := 1
USE_XML_AUDIO_POLICY_CONF := 1

# Kernel
BOARD_KERNEL_CMDLINE := console=ttyHSL0,115200,n8 androidboot.console=ttyHSL0 androidboot.hardware=qcom user_debug=31 msm_rtb.filter=0x237 ehci-hcd.park=3 lpm_levels.sleep_disabled=1 cma=32M@0-0xffffffff
BOARD_KERNEL_CMDLINE += androidboot.selinux=permissive
BOARD_KERNEL_CMDLINE += watchdog_v2.enable=0
BOARD_KERNEL_CMDLINE += panic=3
BOARD_KERNEL_BASE := 0x80000000
BOARD_KERNEL_PAGESIZE := 4096
BOARD_MKBOOTIMG_ARGS := --ramdisk_offset 0x01000000 --tags_offset 0x00000100
BOARD_KERNEL_IMAGE_NAME := Image.gz-dtb
TARGET_KERNEL_ARCH := arm64
TARGET_KERNEL_HEADER_ARCH := arm64
TARGET_KERNEL_SOURCE := kernel/zte/msm8996
TARGET_KERNEL_CONFIG := lineageos_fujisan_defconfig
# The 4.4 msm8996 base builds a 32-bit compat vDSO with clang.
TARGET_KERNEL_MAKE_ENV += CROSS_COMPILE_ARM32=arm-linux-gnueabi-
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
    /system/etc:/etc \
    /vendor/lib/dsp:/dsp \
    /sys/kernel/debug:/d

# VINTF
DEVICE_MANIFEST_FILE := vendor/zte/fujisan/proprietary/vendor/manifest.xml \
    $(DEVICE_PATH)/manifest.xml
DEVICE_MATRIX_FILE := vendor/zte/fujisan/proprietary/vendor/compatibility_matrix.xml

# Display
BOARD_USES_ADRENO := true
TARGET_CONTINUOUS_SPLASH_ENABLED := true
TARGET_SCREEN_DENSITY := 480
MAX_VIRTUAL_DISPLAY_DIMENSION := 4096
NUM_FRAMEBUFFER_SURFACE_BUFFERS := 3
TARGET_FORCE_HWC_FOR_VIRTUAL_DISPLAYS := true
TARGET_USES_GRALLOC1 := true
TARGET_USES_HWC2 := true
TARGET_USES_ION := true
TARGET_USES_OVERLAY := true
TARGET_USES_QCOM_DISPLAY_BSP := true
USE_OPENGL_RENDERER := true
MAX_EGL_CACHE_KEY_SIZE := 12*1024
MAX_EGL_CACHE_SIZE := 2048*1024
OVERRIDE_RS_DRIVER := libRSDriver_adreno.so

# GPS / radio
TARGET_NO_RPC := true
USE_DEVICE_SPECIFIC_GPS := true
TARGET_RIL_VARIANT := caf

# Wi-Fi
# The stock kernel already exposes a live wlan module/interface and does not
# expose the legacy fwpath sysfs node. Advertising legacy fwpath switching to
# libwifi-hal makes startup fail while trying to switch firmware mode on a
# non-existent node.

# Recovery
TARGET_RECOVERY_FSTAB := $(DEVICE_PATH)/rootdir/etc/fstab.qcom
TARGET_RECOVERY_PIXEL_FORMAT := "RGBX_8888"
RECOVERY_GRAPHICS_USE_LINELENGTH := true
BOARD_HAS_NO_SELECT_BUTTON := true
BOARD_USES_MMCUTILS := true

# SELinux
include device/qcom/sepolicy-legacy-um/SEPolicy.mk
BOARD_VENDOR_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy
SELINUX_IGNORE_NEVERALLOWS := true

-include vendor/zte/fujisan/BoardConfigVendor.mk
