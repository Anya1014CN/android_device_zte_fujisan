#!/system/bin/sh
#
# The legacy 4.4 kernel exposes gpu_mem_total but cannot load the Android
# tracepoint eBPF program that normally pins this map.  libmeminfo treats the
# pin as mandatory, so provide an ABI-compatible empty map as a safe fallback.
# A real gpuMem.bpf map wins: never replace an existing pin.

set -u

MAP=/sys/fs/bpf/map_gpuMem_gpu_mem_total_map

[ -e "${MAP}" ] && exit 0

if ! /system/bin/bpftool map create "${MAP}" \
        type hash key 8 value 8 entries 1024 name gpu_mem_total_map flags 0; then
    # This statistic is optional.  Never turn a failed compatibility setup
    # into a boot failure.
    exit 0
fi

# system_server opens the pinned map read-only through libmeminfo.
if ! chown root:system "${MAP}" || ! chmod 0640 "${MAP}"; then
    rm -f "${MAP}"
fi

exit 0
