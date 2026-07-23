#!/system/bin/sh
# Provide a software timed_output node so vibrator@1.0 can start without HW.

VIB_SYS=/sys/class/timed_output
VIB_FAKE=/dev/fujisan_vibrator

mkdir -p "$VIB_FAKE/vibrator" 2>/dev/null
if [ ! -e "$VIB_FAKE/vibrator/enable" ]; then
    echo 0 > "$VIB_FAKE/vibrator/enable" 2>/dev/null
fi
chmod 666 "$VIB_FAKE/vibrator/enable" 2>/dev/null

# Bind only when the real class is empty / missing the device node.
if [ -d "$VIB_SYS" ] && [ ! -e "$VIB_SYS/vibrator/enable" ]; then
    mount -o bind "$VIB_FAKE" "$VIB_SYS" 2>/dev/null
fi

exit 0
