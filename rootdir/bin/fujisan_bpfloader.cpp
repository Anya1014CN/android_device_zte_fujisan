/*
 * Android 12's stock bpfloader treats unavailable eBPF program and cgroup
 * support as fatal. The 3.18 kernel cannot provide that ABI.
 */
#include <stdio.h>
#include <sys/system_properties.h>

int main(void)
{
    __system_property_set("bpf.progs_loaded", "1");
    __system_property_set("bpf.has_net_cgroup", "0");

    FILE *kmsg = fopen("/dev/kmsg", "w");
    if (kmsg) {
        fputs("fujisan-bpfloader: stub done bpf.progs_loaded=1\n", kmsg);
        fclose(kmsg);
    }

    return 0;
}
