#!/system/bin/sh
# Bootstrap configfs for msm8996 4.4.
# USB function selection belongs to UsbDeviceManager and init.usb.configfs.rc.

export PATH=/system/bin:/system/xbin:/vendor/bin

G=/config/usb_gadget/g1
MODE=/sys/devices/soc/6a00000.ssusb/mode
CTRL="$(getprop sys.usb.controller)"
[ -n "$CTRL" ] || CTRL=6a00000.dwc3

setprop sys.usb.configfs 1
setprop sys.usb.controller "$CTRL"

[ -e "$MODE" ] && echo peripheral > "$MODE" 2>/dev/null

if [ ! -d /config/usb_gadget ]; then
    mount -t configfs none /config 2>/dev/null
fi

mkdir -p "$G/strings/0x409" 2>/dev/null
mkdir -p "$G/configs/b.1/strings/0x409" 2>/dev/null
mkdir -p "$G/functions/ffs.adb" 2>/dev/null
mkdir -p /dev/usb-ffs/adb 2>/dev/null
mount -t functionfs adb /dev/usb-ffs/adb -o uid=2000,gid=2000 2>/dev/null \
    || mount -t functionfs adb /dev/usb-ffs/adb 2>/dev/null

exit 0
