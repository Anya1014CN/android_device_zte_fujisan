$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)

ifneq ($(wildcard vendor/cm/config/common_full_phone.mk),)
$(call inherit-product, vendor/cm/config/common_full_phone.mk)
else
$(call inherit-product, vendor/lineage/config/common_full_phone.mk)
endif

$(call inherit-product, device/zte/fujisan/device.mk)

PRODUCT_DEVICE := fujisan
PRODUCT_NAME := lineage_fujisan
PRODUCT_BRAND := ZTE
PRODUCT_MODEL := Z999
PRODUCT_MANUFACTURER := ZTE

PRODUCT_BUILD_PROP_OVERRIDES += \
    PRODUCT_NAME=P996A26 \
    PRIVATE_BUILD_DESC="P996A26-user 8.1.0 OPM1.171019.026 20190218.120220 release-keys"

BUILD_FINGERPRINT := ZTE/P996A26/fujisan:8.1.0/OPM1.171019.026/20190218.120220:user/release-keys
