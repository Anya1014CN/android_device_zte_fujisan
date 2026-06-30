LOCAL_PATH := device/zte/fujisan

PRODUCT_CHARACTERISTICS := nosdcard

PRODUCT_AAPT_CONFIG := normal
PRODUCT_AAPT_PREF_CONFIG := xxhdpi

PRODUCT_SHIPPING_API_LEVEL := 25

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootdir/firmware/.placeholder:root/firmware/.placeholder \
    $(LOCAL_PATH)/rootdir/bt_firmware/.placeholder:root/bt_firmware/.placeholder \
    $(LOCAL_PATH)/rootdir/dsp/.placeholder:root/dsp/.placeholder \
    $(LOCAL_PATH)/rootdir/persist/.placeholder:root/persist/.placeholder \
    $(LOCAL_PATH)/rootdir/etc/fstab.qcom:root/fstab.qcom \
    $(LOCAL_PATH)/rootdir/ueventd.qcom.rc:root/ueventd.qcom.rc \
    $(LOCAL_PATH)/system.prop:system/system.prop \
    $(LOCAL_PATH)/system/usr/idc/synaptics_dsx.idc:system/usr/idc/synaptics_dsx.idc \
    $(LOCAL_PATH)/system/usr/idc/zte-touchscreen.idc:system/usr/idc/zte-touchscreen.idc \
    $(LOCAL_PATH)/system/usr/keylayout/gpio-keys.kl:system/usr/keylayout/gpio-keys.kl \
    $(LOCAL_PATH)/system/usr/keylayout/qpnp_pon.kl:system/usr/keylayout/qpnp_pon.kl

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.bluetooth.xml:system/etc/permissions/android.hardware.bluetooth.xml \
    frameworks/native/data/etc/android.hardware.camera.flash-autofocus.xml:system/etc/permissions/android.hardware.camera.flash-autofocus.xml \
    frameworks/native/data/etc/android.hardware.camera.front.xml:system/etc/permissions/android.hardware.camera.front.xml \
    frameworks/native/data/etc/android.hardware.fingerprint.xml:system/etc/permissions/android.hardware.fingerprint.xml \
    frameworks/native/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml \
    frameworks/native/data/etc/android.hardware.sensor.proximity.xml:system/etc/permissions/android.hardware.sensor.proximity.xml \
    frameworks/native/data/etc/android.hardware.telephony.gsm.xml:system/etc/permissions/android.hardware.telephony.gsm.xml \
    frameworks/native/data/etc/android.hardware.touchscreen.multitouch.jazzhand.xml:system/etc/permissions/android.hardware.touchscreen.multitouch.jazzhand.xml \
    frameworks/native/data/etc/android.hardware.usb.host.xml:system/etc/permissions/android.hardware.usb.host.xml \
    frameworks/native/data/etc/android.hardware.wifi.direct.xml:system/etc/permissions/android.hardware.wifi.direct.xml \
    frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml

PRODUCT_PACKAGES += \
    libion

DEVICE_PACKAGE_OVERLAYS += \
    $(LOCAL_PATH)/overlay

PRODUCT_PROPERTY_OVERRIDES += \
    ro.product.board=fujisan \
    ro.board.platform=msm8996 \
    ro.build.product=fujisan \
    ro.hardware=qcom \
    ro.sf.lcd_density=480

PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    ro.adb.secure=0 \
    persist.sys.usb.config=adb

# Single-screen bring-up first. Secondary display, hinge and companion touch
# routing will be enabled after the primary panel build is stable.
PRODUCT_PROPERTY_OVERRIDES += \
    persist.radio.multisim.config=dsds \
    qcom.bluetooth.soc=rome \
    ro.telephony.default_network=22,20 \
    wifi.interface=wlan0

$(call inherit-product-if-exists, vendor/zte/fujisan/fujisan-vendor.mk)
