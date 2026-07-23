#!/system/bin/sh
# Clean configfs adb bind for msm8996 4.4.
# AOSP composition can leave UDC busy / f1 already linked; rebind once.

export PATH=/system/bin:/system/xbin:/vendor/bin

G=/config/usb_gadget/g1
MODE=/sys/devices/soc/6a00000.ssusb/mode
CTRL="$(getprop sys.usb.controller)"
[ -n "$CTRL" ] || CTRL=6a00000.dwc3

setprop sys.usb.configfs 1
setprop persist.sys.usb.config adb
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

[ -d "$G" ] || exit 0

echo none > "$G/UDC" 2>/dev/null
usleep 200000 2>/dev/null || sleep 0.2
rm -f "$G/configs/b.1/f1" "$G/configs/b.1/f2" "$G/configs/b.1/f3" \
      "$G/configs/b.1/f4" "$G/configs/b.1/f5" 2>/dev/null

echo 0x18d1 > "$G/idVendor" 2>/dev/null
echo 0x4ee7 > "$G/idProduct" 2>/dev/null
echo 0x0200 > "$G/bcdUSB" 2>/dev/null
echo "Google" > "$G/strings/0x409/manufacturer" 2>/dev/null
echo "Android" > "$G/strings/0x409/product" 2>/dev/null
echo "$(getprop ro.serialno)" > "$G/strings/0x409/serialnumber" 2>/dev/null
echo "adb" > "$G/configs/b.1/strings/0x409/configuration" 2>/dev/null

ln -s "$G/functions/ffs.adb" "$G/configs/b.1/f1" 2>/dev/null \
    || ln -sf "$G/functions/ffs.adb" "$G/configs/b.1/f1" 2>/dev/null

setprop sys.usb.config adb
start adbd 2>/dev/null
usleep 400000 2>/dev/null || sleep 0.4

echo "$CTRL" > "$G/UDC" 2>/dev/null
setprop sys.usb.state adb
exit 0
