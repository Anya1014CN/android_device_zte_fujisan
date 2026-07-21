/*
 * msm8996 3.18 lacks the 4.4-era eBPF stack used by Xiaomi gemini.
 * AOSP bpfloader hard-reboots with reason bpfloader-failed when maps/syscalls
 * are missing or incomplete. Return success and mark programs loaded.
 */
#include <stdio.h>
#include <sys/system_properties.h>

int main(void) {
    __system_property_set("bpf.progs_loaded", "1");
    __system_property_set("bpf.has_net_cgroup", "0");

    FILE *kmsg = fopen("/dev/kmsg", "w");
    if (kmsg) {
        fputs("fujisan-bpfloader: stub done bpf.progs_loaded=1
", kmsg);
        fclose(kmsg);
    }
    return 0;
}
