$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/verity.mk)
$(call inherit-product, vendor/lineage/config/common_full_phone.mk)

$(call inherit-product, device/zte/fujisan/device.mk)

# The generic Qualcomm product file adds its Android 11 IPACM after device.mk.
# Fujisan's 3.18 IPA driver requires the matching stock daemon from vendor.
PRODUCT_PACKAGES := $(filter-out ipacm ipacm.rc,$(PRODUCT_PACKAGES))

PRODUCT_NAME := lineage_fujisan
PRODUCT_DEVICE := fujisan
PRODUCT_BRAND := ZTE
PRODUCT_MODEL := Z999
PRODUCT_MANUFACTURER := ZTE

PRODUCT_BUILD_PROP_OVERRIDES += \
    PRODUCT_NAME=P996A26 \
    PRIVATE_BUILD_DESC="P996A26-user 8.1.0 OPM1.171019.026 303 release-keys"

BUILD_FINGERPRINT := "ZTE/P996A26/fujisan:8.1.0/OPM1.171019.026/20190218.120220:user/release-keys"

TARGET_VENDOR := zte

PRODUCT_SUPPORTS_BOOT_SIGNER := true
PRODUCT_SUPPORTS_VERITY := true
PRODUCT_SUPPORTS_VERITY_FEC := true
PRODUCT_VERITY_SIGNING_KEY := build/make/target/product/security/verity
PRODUCT_SYSTEM_VERITY_PARTITION := /dev/block/bootdevice/by-name/system

PRODUCT_PACKAGES += \
    verity_key
