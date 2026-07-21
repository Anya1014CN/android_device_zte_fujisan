$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)
$(call inherit-product, vendor/lineage/config/common_full_phone.mk)

# The stock firmware has no separate vendor image.  Keep APEXes unpacked and
# allow the device manifest to provide the legacy vendor HAL declarations.
PRODUCT_COMPRESSED_APEX := false
PRODUCT_ENFORCE_VINTF_MANIFEST_OVERRIDE := true

# Non-Treble / non-SAR stock layout still boots through first-stage init on
# Android 12. Disable dm-verity while bring-up is unstable so a signed
# system image is not required for every boot-only kernel test.

$(call inherit-product, device/zte/fujisan/device.mk)

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

PRODUCT_SUPPORTS_BOOT_SIGNER := false
PRODUCT_SUPPORTS_VERITY := false
PRODUCT_SUPPORTS_VERITY_FEC := false
