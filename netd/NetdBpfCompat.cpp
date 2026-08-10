/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <limits.h>
#include <linux/bpf.h>
#include <log/log.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace {

constexpr char kBpfRoot[] = "/sys/fs/bpf/netd_shared";
constexpr const char* kPrograms[] = {
        "prog_netd_skfilter_denylist_xtbpf",
        "prog_netd_skfilter_allowlist_xtbpf",
        "prog_netd_skfilter_ingress_xtbpf",
        "prog_netd_skfilter_egress_xtbpf",
};

struct MapSpec {
    const char* name;
    enum bpf_map_type type;
    __u32 value_size;
    __u32 max_entries;
};

// These are the public map contracts consumed by DNS resolver and
// ConnectivityService.  Leave them empty: no UID can be blocked and no BPF
// accounting policy is enabled on the legacy kernel.
constexpr MapSpec kMaps[] = {
        {"map_netd_configuration_map", BPF_MAP_TYPE_ARRAY, sizeof(__u32), 2},
        {"map_netd_uid_owner_map", BPF_MAP_TYPE_HASH, 2 * sizeof(__u32), 10000},
        {"map_netd_data_saver_enabled_map", BPF_MAP_TYPE_ARRAY, sizeof(__u8), 1},
};

int bpf(enum bpf_cmd cmd, union bpf_attr* attr) {
    return syscall(__NR_bpf, cmd, attr, sizeof(*attr));
}

int openPinnedObject(const char* path) {
    union bpf_attr attr = {};
    attr.pathname = reinterpret_cast<__u64>(path);
    return bpf(BPF_OBJ_GET, &attr);
}

int createMap(const MapSpec& spec) {
    union bpf_attr attr = {};
    attr.map_type = spec.type;
    attr.key_size = sizeof(__u32);
    attr.value_size = spec.value_size;
    attr.max_entries = spec.max_entries;
    return bpf(BPF_MAP_CREATE, &attr);
}

int loadNoMatchProgram() {
    const bpf_insn instructions[] = {
            {.code = BPF_ALU64 | BPF_MOV | BPF_K, .dst_reg = BPF_REG_0, .imm = 0},
            {.code = BPF_JMP | BPF_EXIT},
    };
    static constexpr char kLicense[] = "GPL";

    union bpf_attr attr = {};
    attr.prog_type = BPF_PROG_TYPE_SOCKET_FILTER;
    attr.insn_cnt = sizeof(instructions) / sizeof(instructions[0]);
    attr.insns = reinterpret_cast<__u64>(instructions);
    attr.license = reinterpret_cast<__u64>(kLicense);
    return bpf(BPF_PROG_LOAD, &attr);
}

bool pinProgram(const char* path, int fd) {
    union bpf_attr attr = {};
    attr.pathname = reinterpret_cast<__u64>(path);
    attr.bpf_fd = fd;
    return bpf(BPF_OBJ_PIN, &attr) == 0;
}

bool ensureProgram(const char* name) {
    char path[PATH_MAX] = {};
    snprintf(path, sizeof(path), "%s/%s", kBpfRoot, name);

    const int existing = openPinnedObject(path);
    if (existing >= 0) {
        close(existing);
        return true;
    }
    if (errno != ENOENT) {
        ALOGE("BPF_OBJ_GET(%s) failed: %s", path, strerror(errno));
        return false;
    }

    const int program = loadNoMatchProgram();
    if (program < 0) {
        ALOGE("BPF_PROG_LOAD(%s) failed: %s", name, strerror(errno));
        return false;
    }
    const bool pinned = pinProgram(path, program);
    if (!pinned) ALOGE("BPF_OBJ_PIN(%s) failed: %s", path, strerror(errno));
    close(program);
    return pinned;
}

bool ensureMap(const MapSpec& spec) {
    char path[PATH_MAX] = {};
    snprintf(path, sizeof(path), "%s/%s", kBpfRoot, spec.name);

    const int existing = openPinnedObject(path);
    if (existing >= 0) {
        close(existing);
        return true;
    }
    if (errno != ENOENT) {
        ALOGE("BPF_OBJ_GET(%s) failed: %s", path, strerror(errno));
        return false;
    }

    const int map = createMap(spec);
    if (map < 0) {
        ALOGE("BPF_MAP_CREATE(%s) failed: %s", spec.name, strerror(errno));
        return false;
    }
    const bool pinned = pinProgram(path, map);
    if (!pinned) ALOGE("BPF_OBJ_PIN(%s) failed: %s", path, strerror(errno));
    close(map);
    return pinned;
}

}  // namespace

int main() {
    if (mkdir(kBpfRoot, 0755) && errno != EEXIST) {
        ALOGE("mkdir(%s) failed: %s", kBpfRoot, strerror(errno));
        return 1;
    }
    for (const char* program : kPrograms) {
        if (!ensureProgram(program)) return 1;
    }
    for (const MapSpec& map : kMaps) {
        if (!ensureMap(map)) return 1;
    }
    return 0;
}
