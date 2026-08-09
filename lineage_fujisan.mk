$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)
$(call inherit-product, vendor/lineage/config/common_full_phone.mk)

# The stock firmware has no separate vendor image. Keep APEXes unpacked and
# use the device manifests selected in BoardConfig.mk.
PRODUCT_COMPRESSED_APEX := false

# The device has a persistent, non-A/B userdata partition.  A factory
# userdata image is neither flashable by the OTA path nor needed for bring-up,
# and expanding its 52 GiB sparse image makes OTA packaging impractical.
PRODUCT_BUILD_USERDATA_IMAGE := false

$(call inherit-product, device/zte/fujisan/device.mk)

PRODUCT_NAME := lineage_fujisan
PRODUCT_DEVICE := fujisan
PRODUCT_BRAND := ZTE
PRODUCT_MODEL := ZTE Axon M
PRODUCT_MANUFACTURER := ZTE

PRODUCT_BUILD_PROP_OVERRIDES += \
    DeviceProduct=P996A26 \
    BuildDesc="P996A26-user 8.1.0 OPM1.171019.026 303 release-keys"

BUILD_FINGERPRINT := ZTE/P996A26/fujisan:8.1.0/OPM1.171019.026/20190218.120220:user/release-keys

TARGET_VENDOR := zte

# The physical system block device is retained as a P2 boot-metadata input.
# Android 16 removed the old VB1 product flags; select and validate AVB only
# after a recovery-bootable image exists.
PRODUCT_SYSTEM_VERITY_PARTITION := /dev/block/bootdevice/by-name/system
