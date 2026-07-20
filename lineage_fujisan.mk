$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/verity.mk)
$(call inherit-product, vendor/lineage/config/common_full_phone.mk)

$(call inherit-product, device/zte/fujisan/device.mk)

# common_full_phone or other inherited product fragments may pull in the
# source-built fingerprint wrapper service.  Fujisan keeps the proprietary
# vendor fingerprint service instead, so drop the AOSP wrapper at the final
# product level to avoid duplicate install rules and VINTF fragment conflicts.
PRODUCT_PACKAGES := $(filter-out \
    android.hardware.biometrics.fingerprint@2.1-service, \
    $(PRODUCT_PACKAGES))

PRODUCT_NAME := lineage_fujisan
PRODUCT_DEVICE := fujisan
PRODUCT_BRAND := ZTE
PRODUCT_MODEL := ZTE Axon M
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
