#!/system/bin/sh
# Persist early boot breadcrumbs and kernel/userspace logs across reboots.
# Prefers the stock dcmlog partition; falls back to /cache.

export PATH=/system/bin:/system/xbin:/vendor/bin:/sbin:/bin

stamp="$(date +%Y%m%d_%H%M%S 2>/dev/null || echo unknown)"
uptime_s="$(cut -d. -f1 /proc/uptime 2>/dev/null || echo 0)"

mark() {
    msg="$1"
    echo "fujisan-bootlog[${uptime_s}s]: ${msg}" > /dev/kmsg 2>/dev/null
    echo "fujisan-bootlog[${uptime_s}s]: ${msg}" > /dev/pmsg0 2>/dev/null
}

prepare_dir() {
    base="$1"
    [ -d "$base" ] || return 1
    mkdir -p "$base/lineage" 2>/dev/null || return 1
    chmod 0775 "$base/lineage" 2>/dev/null
    echo "$base/lineage"
}

logdir=""
for candidate in /dcmlog /cache; do
    logdir="$(prepare_dir "$candidate")" && break
done

if [ -z "$logdir" ]; then
    mark "no writable logdir"
    exit 0
fi

run_dir="${logdir}/boot_${stamp}_${uptime_s}s"
mkdir -p "$run_dir" 2>/dev/null || run_dir="$logdir"

mark "writing to ${run_dir}"

{
    echo "stamp=${stamp}"
    echo "uptime=${uptime_s}"
    echo "date=$(date 2>/dev/null)"
    echo "pid=$$"
    echo "cmdline=$(cat /proc/cmdline 2>/dev/null)"
    echo "build=$(getprop ro.build.display.id 2>/dev/null)"
    echo "fingerprint=$(getprop ro.build.fingerprint 2>/dev/null)"
    echo "bootloader=$(getprop ro.bootloader 2>/dev/null)"
    echo "verifiedbootstate=$(getprop ro.boot.verifiedbootstate 2>/dev/null)"
    echo "veritymode=$(getprop ro.boot.veritymode 2>/dev/null)"
    echo "slot=$(getprop ro.boot.slot_suffix 2>/dev/null)"
    echo "stage=$(getprop init.svc. 2>/dev/null)"
} > "${run_dir}/bootinfo.txt" 2>/dev/null

getprop > "${run_dir}/getprop.txt" 2>/dev/null
cat /proc/cmdline > "${run_dir}/cmdline.txt" 2>/dev/null
cat /proc/mounts > "${run_dir}/mounts.txt" 2>/dev/null
ps -A > "${run_dir}/ps.txt" 2>/dev/null || ps > "${run_dir}/ps.txt" 2>/dev/null
ls -la / > "${run_dir}/root_ls.txt" 2>/dev/null
ls -la /system > "${run_dir}/system_ls.txt" 2>/dev/null
ls -la /vendor > "${run_dir}/vendor_ls.txt" 2>/dev/null
ls -la /system/vendor > "${run_dir}/system_vendor_ls.txt" 2>/dev/null

if [ -r /proc/last_kmsg ]; then
    cat /proc/last_kmsg > "${run_dir}/last_kmsg.txt" 2>/dev/null
fi

if command -v dmesg >/dev/null 2>&1; then
    dmesg -T > "${run_dir}/dmesg.txt" 2>/dev/null || dmesg > "${run_dir}/dmesg.txt" 2>/dev/null
fi

# Snapshot pstore if already mounted/available
if [ -d /sys/fs/pstore ]; then
    mkdir -p "${run_dir}/pstore" 2>/dev/null
    cp -a /sys/fs/pstore/* "${run_dir}/pstore/" 2>/dev/null
fi

# Capture logcat briefly if logd is alive; ignore failures.
if command -v logcat >/dev/null 2>&1; then
    timeout 3 logcat -b all -d > "${run_dir}/logcat.txt" 2>/dev/null || \
        logcat -b all -d -t 500 > "${run_dir}/logcat.txt" 2>/dev/null
fi

echo "${run_dir}" > "${logdir}/latest" 2>/dev/null
echo "ok ${stamp} ${run_dir}" > "${logdir}/stage_bootlog_done" 2>/dev/null
mark "done ${run_dir}"
exit 0
