#!/system/bin/sh
# Force host-visible adb on this msm8996 kernel.
# Prefer the stock android_usb gadget nodes that this kernel exposes.

export PATH=/system/bin:/system/xbin:/vendor/bin:/sbin:/bin

log() {
    echo "fujisan-usb: $*" > /dev/kmsg 2>/dev/null
    echo "fujisan-usb: $*" > /dev/pmsg0 2>/dev/null
}

mkdir -p /dev/usb-ffs/adb 2>/dev/null
mount -t functionfs adb /dev/usb-ffs/adb -o uid=2000,gid=2000 2>/dev/null \
    || mount -t functionfs adb /dev/usb-ffs/adb 2>/dev/null

USB=/sys/class/android_usb/android0
if [ ! -e "$USB/enable" ]; then
    log "android_usb nodes missing"
    exit 0
fi

# Drop any multi-function persist state that stock scripts reapply.
setprop persist.sys.usb.config adb
setprop sys.usb.configfs 0
setprop sys.usb.config adb
setprop sys.usb.controller 6a00000.dwc3

echo 0 > "$USB/enable" 2>/dev/null
echo 18D1 > "$USB/idVendor" 2>/dev/null
echo 4EE7 > "$USB/idProduct" 2>/dev/null
echo adb > "$USB/f_ffs/aliases" 2>/dev/null
echo adb > "$USB/functions" 2>/dev/null || echo ffs > "$USB/functions" 2>/dev/null
echo Fujisan > "$USB/iManufacturer" 2>/dev/null
echo AxonM > "$USB/iProduct" 2>/dev/null
echo 1 > "$USB/enable" 2>/dev/null

start adbd 2>/dev/null
log "forced android_usb functions=$(cat $USB/functions 2>/dev/null) enable=$(cat $USB/enable 2>/dev/null) state=$(cat $USB/state 2>/dev/null)"
exit 0
