DEVICE_PATH := device/zte/fujisan

# Architecture
TARGET_ARCH := arm64
TARGET_ARCH_VARIANT := armv8-a
TARGET_CPU_ABI := arm64-v8a
TARGET_CPU_ABI2 :=
TARGET_CPU_VARIANT := generic

TARGET_2ND_ARCH := arm
TARGET_2ND_ARCH_VARIANT := armv7-a-neon
TARGET_2ND_CPU_ABI := armeabi-v7a
TARGET_2ND_CPU_ABI2 := armeabi
TARGET_2ND_CPU_VARIANT := generic

# Platform
TARGET_BOARD_PLATFORM := msm8996
TARGET_BOOTLOADER_BOARD_NAME := fujisan
TARGET_NO_BOOTLOADER := true
TARGET_NO_RADIOIMAGE := true
TARGET_USES_64_BIT_BINDER := true
BOARD_USES_QCOM_HARDWARE := true
TARGET_USES_QCOM_BSP := true

# This device is Oreo non-Treble and keeps vendor under /system/vendor.
TARGET_COPY_OUT_VENDOR := system/vendor

# Kernel
BOARD_KERNEL_IMAGE_NAME := Image.gz-dtb
BOARD_KERNEL_BASE := 0x80000000
BOARD_KERNEL_PAGESIZE := 4096
BOARD_KERNEL_CMDLINE := console=ttyHSL0,115200,n8 androidboot.console=ttyHSL0 androidboot.hardware=qcom user_debug=31 msm_rtb.filter=0x237 ehci-hcd.park=3 lpm_levels.sleep_disabled=1 cma=32M@0-0xffffffff
BOARD_MKBOOTIMG_ARGS := --ramdisk_offset 0x01000000 --tags_offset 0x00000100
BOARD_FLASH_BLOCK_SIZE := 262144
FUJISAN_PREBUILT_KERNEL ?=
ifeq ($(FUJISAN_PREBUILT_KERNEL),)
TARGET_KERNEL_SOURCE := kernel/zte/fujisan
TARGET_KERNEL_CONFIG := msm-perf_fujisan_defconfig
TARGET_KERNEL_ARCH := arm64
else
TARGET_PREBUILT_KERNEL := $(FUJISAN_PREBUILT_KERNEL)
endif

# Partitions
BOARD_BOOTIMAGE_PARTITION_SIZE := 67108864
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 67108864
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 6979321856
BOARD_CACHEIMAGE_PARTITION_SIZE := 1718616064
BOARD_USERDATAIMAGE_PARTITION_SIZE := 52766424576
BOARD_PERSISTIMAGE_PARTITION_SIZE := 33554432
BOARD_HAS_NO_REAL_SDCARD := true
TARGET_USERIMAGES_USE_EXT4 := true
TARGET_USERIMAGES_USE_F2FS := true
BOARD_SUPPRESS_SECURE_ERASE := true

# Filesystems
TARGET_RECOVERY_FSTAB := $(DEVICE_PATH)/rootdir/etc/fstab.qcom

# Display
TARGET_SCREEN_DENSITY := 480

# Recovery
TARGET_RECOVERY_PIXEL_FORMAT := "RGBX_8888"
RECOVERY_GRAPHICS_USE_LINELENGTH := true
BOARD_HAS_NO_SELECT_BUTTON := true
BOARD_USES_MMCUTILS := true
BOARD_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy

# Keep SELinux permissive for the first bring-up pass. Tightening policy can
# happen once the primary display, touch and WiFi path are verified.
BOARD_KERNEL_CMDLINE += androidboot.selinux=permissive

-include vendor/zte/fujisan/BoardConfigVendor.mk
