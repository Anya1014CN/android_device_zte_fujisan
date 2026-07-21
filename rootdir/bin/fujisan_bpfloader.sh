#!/system/bin/sh
# msm8996 3.18 lacks the 4.4-era eBPF stack used by Xiaomi gemini.
# AOSP bpfloader hard-reboots with reason bpfloader-failed on ENOSYS/map
# failures. Return success and mark programs loaded so boot can continue.

export PATH=/system/bin:/system/xbin:/vendor/bin:/sbin

echo "fujisan-bpfloader: stub start" > /dev/kmsg 2>/dev/null
echo "fujisan-bpfloader: stub start" > /dev/pmsg0 2>/dev/null

/system/bin/setprop bpf.progs_loaded 1 2>/dev/null
/system/bin/setprop bpf.has_net_cgroup 0 2>/dev/null

for d in /dcmlog/lineage /cache/lineage; do
    if [ -d "$d" ] || mkdir -p "$d" 2>/dev/null; then
        echo "stub $(date 2>/dev/null) uptime=$(cut -d. -f1 /proc/uptime 2>/dev/null)" > "$d/stage_bpfloader_stub" 2>/dev/null
        break
    fi
done

echo "fujisan-bpfloader: stub done bpf.progs_loaded=1" > /dev/kmsg 2>/dev/null
echo "fujisan-bpfloader: stub done" > /dev/pmsg0 2>/dev/null
exit 0
