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
    $(LOCAL_PATH)/rootdir/init.qcom.rc:root/init.qcom.rc \
    $(LOCAL_PATH)/rootdir/stock-root/default.prop:root/default.prop \
    $(LOCAL_PATH)/rootdir/stock-root/init.rc:root/init.rc \
    $(LOCAL_PATH)/rootdir/stock-root/init.recovery.qcom.rc:root/init.recovery.qcom.rc \
    $(LOCAL_PATH)/rootdir/stock-root/init.usb.rc:root/init.usb.rc \
    $(LOCAL_PATH)/rootdir/stock-root/ueventd.rc:root/ueventd.rc \
    $(LOCAL_PATH)/rootdir/stock-root/init.usb.configfs.rc:root/init.usb.configfs.rc \
    $(LOCAL_PATH)/rootdir/stock-root/nonplat_file_contexts:root/nonplat_file_contexts \
    $(LOCAL_PATH)/rootdir/stock-root/nonplat_hwservice_contexts:root/nonplat_hwservice_contexts \
    $(LOCAL_PATH)/rootdir/stock-root/nonplat_property_contexts:root/nonplat_property_contexts \
    $(LOCAL_PATH)/rootdir/stock-root/nonplat_seapp_contexts:root/nonplat_seapp_contexts \
    $(LOCAL_PATH)/rootdir/stock-root/nonplat_service_contexts:root/nonplat_service_contexts \
    $(LOCAL_PATH)/rootdir/stock-root/plat_file_contexts:root/plat_file_contexts \
    $(LOCAL_PATH)/rootdir/stock-root/plat_hwservice_contexts:root/plat_hwservice_contexts \
    $(LOCAL_PATH)/rootdir/stock-root/plat_property_contexts:root/plat_property_contexts \
    $(LOCAL_PATH)/rootdir/stock-root/plat_seapp_contexts:root/plat_seapp_contexts \
    $(LOCAL_PATH)/rootdir/stock-root/plat_service_contexts:root/plat_service_contexts \
    $(LOCAL_PATH)/rootdir/stock-root/sepolicy:root/sepolicy \
    $(LOCAL_PATH)/rootdir/stock-root/vndservice_contexts:root/vndservice_contexts \
    $(LOCAL_PATH)/rootdir/sbin/jnl.ko:root/sbin/jnl.ko \
    $(LOCAL_PATH)/rootdir/sbin/ufsd.ko:root/sbin/ufsd.ko \
    $(LOCAL_PATH)/rootdir/etc/fstab.qcom:root/fstab.qcom \
    $(LOCAL_PATH)/rootdir/ueventd.qcom.rc:root/ueventd.qcom.rc \
    $(LOCAL_PATH)/system/usr/idc/goodix-touchscreen.idc:system/usr/idc/goodix-touchscreen.idc \
    $(LOCAL_PATH)/system.prop:system/system.prop \
    $(LOCAL_PATH)/system/usr/idc/synaptics_dsx.idc:system/usr/idc/synaptics_dsx.idc \
    $(LOCAL_PATH)/system/usr/idc/zte-touchscreen.idc:system/usr/idc/zte-touchscreen.idc \
    $(LOCAL_PATH)/system/usr/idc/zte-touchscreen-2nd.idc:system/usr/idc/zte-touchscreen-2nd.idc \
    $(LOCAL_PATH)/system/usr/idc/zte-touchsrceen-3nd.idc:system/usr/idc/zte-touchsrceen-3nd.idc \
    $(LOCAL_PATH)/system/usr/keylayout/gpio-keys.kl:system/usr/keylayout/gpio-keys.kl \
    $(LOCAL_PATH)/system/usr/keylayout/qpnp_pon.kl:system/usr/keylayout/qpnp_pon.kl \
    $(LOCAL_PATH)/system/usr/keylayout/synaptics_dsx.kl:system/usr/keylayout/synaptics_dsx.kl \
    $(LOCAL_PATH)/system/etc/camera/baby1.mp4:system/etc/camera/baby1.mp4 \
    $(LOCAL_PATH)/system/etc/camera/baby2.mp4:system/etc/camera/baby2.mp4 \
    $(LOCAL_PATH)/system/etc/camera/blackboard.rgb:system/etc/camera/blackboard.rgb \
    $(LOCAL_PATH)/system/etc/camera/children.zip:system/etc/camera/children.zip \
    $(LOCAL_PATH)/system/etc/camera/cool_background.rgb:system/etc/camera/cool_background.rgb \
    $(LOCAL_PATH)/system/etc/camera/cool_map.rgb:system/etc/camera/cool_map.rgb \
    $(LOCAL_PATH)/system/etc/camera/lomo_map.rgb:system/etc/camera/lomo_map.rgb \
    $(LOCAL_PATH)/system/etc/camera/overlay_map.rgb:system/etc/camera/overlay_map.rgb \
    $(LOCAL_PATH)/system/etc/permissions/privapp-permissions-camera.xml:system/etc/permissions/privapp-permissions-camera.xml \
    $(LOCAL_PATH)/system/etc/camera/vd/VD_blurness_parameter.dat:system/etc/camera/vd/VD_blurness_parameter.dat \
    $(LOCAL_PATH)/system/framework/com.qualcomm.qti.camera.jar:system/framework/com.qualcomm.qti.camera.jar \
    $(LOCAL_PATH)/system/lib/libdualcameraddm.so:system/lib/libdualcameraddm.so \
    $(LOCAL_PATH)/system/lib/libZTE_ImagePost.so:system/lib/libZTE_ImagePost.so \
    $(LOCAL_PATH)/system/lib/libjni_dualcamera.so:system/lib/libjni_dualcamera.so \
    $(LOCAL_PATH)/system/etc/camera/vignette_map.rgb:system/etc/camera/vignette_map.rgb \
    $(LOCAL_PATH)/system/etc/camera/warm_map.rgb:system/etc/camera/warm_map.rgb

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.bluetooth.xml:system/etc/permissions/android.hardware.bluetooth.xml \
    frameworks/native/data/etc/android.hardware.camera.flash-autofocus.xml:system/etc/permissions/android.hardware.camera.flash-autofocus.xml \
    frameworks/native/data/etc/android.hardware.camera.front.xml:system/etc/permissions/android.hardware.camera.front.xml \
    frameworks/native/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml \
    frameworks/native/data/etc/android.hardware.sensor.proximity.xml:system/etc/permissions/android.hardware.sensor.proximity.xml \
    frameworks/native/data/etc/android.software.activities_on_secondary_displays.xml:system/etc/permissions/android.software.activities_on_secondary_displays.xml \
    frameworks/native/data/etc/android.hardware.telephony.gsm.xml:system/etc/permissions/android.hardware.telephony.gsm.xml \
    frameworks/native/data/etc/android.hardware.touchscreen.multitouch.jazzhand.xml:system/etc/permissions/android.hardware.touchscreen.multitouch.jazzhand.xml \
    frameworks/native/data/etc/android.hardware.usb.host.xml:system/etc/permissions/android.hardware.usb.host.xml \
    frameworks/native/data/etc/android.hardware.wifi.direct.xml:system/etc/permissions/android.hardware.wifi.direct.xml \
    frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml

PRODUCT_PACKAGES += \
    android.hardware.biometrics.fingerprint@2.1 \
    libdolbyshim \
    libion \
    libjpeg_encode_hw_jni \
    libsftrans \
    libZTE_ImagePost \
    libzte_night_jni \
    dualscreen-helper \
    miframework \
    framework-zte-res \
    ZTE_Camera \
    DoubleLay_MFV_Multy \
    axonmcontroller_MFV_Multy \
    launchmodetest1_MFV_Multy \

PRODUCT_BOOT_JARS += \
    miframework

DEVICE_PACKAGE_OVERLAYS += \
    $(LOCAL_PATH)/overlay

PRODUCT_PROPERTY_OVERRIDES += \
    ro.product.board=fujisan \
    ro.board.platform=msm8996 \
    ro.build.product=fujisan \
    ro.hardware=qcom \
    ro.sf.lcd_density=480

# Keep the boot ramdisk default properties close to stock while bring-up still
# uses an eng lunch target. Recovery ADB remains available separately; system
# boot should not advertise an always-on insecure adb configuration.
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    ro.secure=1 \
    security.perf_harden=1 \
    ro.adb.secure=1 \
    ro.allow.mock.location=0 \
    ro.debuggable=0 \
    ro.frp.pst=/dev/block/bootdevice/by-name/frp \
    ro.oem_unlock_supported=false \
    pm.dexopt.first-boot=quicken \
    pm.dexopt.boot=verify \
    ro.logdumpd.enabled=0 \
    persist.sys.usb.config=none

PRODUCT_PROPERTY_OVERRIDES += \
    persist.radio.multisim.config=dsds \
    qcom.bluetooth.soc=rome \
    ro.telephony.default_network=22,20 \
    wifi.interface=wlan0

$(call inherit-product-if-exists, vendor/zte/fujisan/fujisan-vendor.mk)
