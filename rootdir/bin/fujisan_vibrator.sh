#!/system/bin/sh
# Software timed_output node so vibrator.default / vibrator@1.0 can start.

VIB_SYS=/sys/class/timed_output
VIB_FAKE=/dev/fujisan_vibrator

mkdir -p "$VIB_FAKE/vibrator" 2>/dev/null
echo 0 > "$VIB_FAKE/vibrator/enable" 2>/dev/null
chmod 755 "$VIB_FAKE" "$VIB_FAKE/vibrator" 2>/dev/null
chmod 666 "$VIB_FAKE/vibrator/enable" 2>/dev/null
chown system:system "$VIB_FAKE/vibrator/enable" 2>/dev/null

if [ -d "$VIB_SYS" ] && [ ! -e "$VIB_SYS/vibrator/enable" ]; then
    mount -o bind "$VIB_FAKE" "$VIB_SYS" 2>/dev/null
fi

# Bind mount may leave root-only mode; force usable perms.
chmod 755 "$VIB_SYS" "$VIB_SYS/vibrator" 2>/dev/null
chmod 666 "$VIB_SYS/vibrator/enable" 2>/dev/null
chown system:system "$VIB_SYS/vibrator/enable" 2>/dev/null

exit 0
