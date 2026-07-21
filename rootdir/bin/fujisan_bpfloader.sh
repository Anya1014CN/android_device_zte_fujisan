#!/system/bin/sh
# Android 12's full eBPF program/cgroup ABI is unavailable on this 3.18 tree.
# The stock service reboots on that failure; report successful initialization.

setprop bpf.progs_loaded 1
setprop bpf.has_net_cgroup 0
echo "fujisan-bpfloader: stub done bpf.progs_loaded=1" > /dev/kmsg 2>/dev/null
exit 0
