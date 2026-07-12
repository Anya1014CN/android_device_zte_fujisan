LOCAL_PATH := device/zte/fujisan

$(call inherit-product, $(SRC_TARGET_DIR)/product/product_launched_with_n_mr1.mk)
$(call inherit-product-if-exists, vendor/zte/fujisan/fujisan-vendor.mk)

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
    ro.product.first_api_level=25

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootdir/init.qcom.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/fujisan.rc \
    $(LOCAL_PATH)/rootdir/firmware/.placeholder:$(TARGET_COPY_OUT_RAMDISK)/firmware/.placeholder \
    $(LOCAL_PATH)/rootdir/bt_firmware/.placeholder:$(TARGET_COPY_OUT_RAMDISK)/bt_firmware/.placeholder \
    $(LOCAL_PATH)/rootdir/dsp/.placeholder:$(TARGET_COPY_OUT_RAMDISK)/dsp/.placeholder \
    $(LOCAL_PATH)/rootdir/persist/.placeholder:$(TARGET_COPY_OUT_RAMDISK)/persist/.placeholder \
    $(LOCAL_PATH)/rootdir/etc/fstab.ramdisk.qcom:$(TARGET_COPY_OUT_RAMDISK)/fstab.qcom \
    $(LOCAL_PATH)/system/usr/idc/goodix-touchscreen.idc:$(TARGET_COPY_OUT_SYSTEM)/usr/idc/goodix-touchscreen.idc \
    $(LOCAL_PATH)/system/usr/idc/synaptics_dsx.idc:$(TARGET_COPY_OUT_SYSTEM)/usr/idc/synaptics_dsx.idc \
    $(LOCAL_PATH)/system/usr/keylayout/gpio-keys.kl:$(TARGET_COPY_OUT_SYSTEM)/usr/keylayout/gpio-keys.kl \
    $(LOCAL_PATH)/system/usr/keylayout/qpnp_pon.kl:$(TARGET_COPY_OUT_SYSTEM)/usr/keylayout/qpnp_pon.kl \
    $(LOCAL_PATH)/system/usr/keylayout/synaptics_dsx.kl:$(TARGET_COPY_OUT_SYSTEM)/usr/keylayout/synaptics_dsx.kl

PRODUCT_COPY_FILES += \
    $(call find-copy-subdir-files,*,$(LOCAL_PATH)/ramdisk,$(TARGET_COPY_OUT_RAMDISK))
