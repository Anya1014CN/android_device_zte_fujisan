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

constexpr char kNetdBpfRoot[] = "/sys/fs/bpf/netd_shared";
constexpr char kClatBpfRoot[] = "/sys/fs/bpf/net_shared";
constexpr uid_t kSystemUid = 1000;
constexpr const char* kPrograms[] = {
        "prog_netd_skfilter_denylist_xtbpf",
        "prog_netd_skfilter_allowlist_xtbpf",
        "prog_netd_skfilter_ingress_xtbpf",
        "prog_netd_skfilter_egress_xtbpf",
};

struct MapSpec {
    const char* name;
    enum bpf_map_type type;
    __u32 key_size;
    __u32 value_size;
    __u32 max_entries;
};

// These are the public map contracts consumed by DNS resolver and
// ConnectivityService.  Leave them empty: no UID can be blocked and no BPF
// accounting policy is enabled on the legacy kernel.
constexpr MapSpec kMaps[] = {
        {"map_netd_configuration_map", BPF_MAP_TYPE_ARRAY, sizeof(__u32), sizeof(__u32), 2},
        {"map_netd_uid_owner_map", BPF_MAP_TYPE_HASH, sizeof(__u32), 2 * sizeof(__u32), 20000},
        {"map_netd_uid_permission_map", BPF_MAP_TYPE_HASH, sizeof(__u32), sizeof(__u8), 6000},
        {"map_netd_cookie_tag_map", BPF_MAP_TYPE_HASH, sizeof(__u64), 2 * sizeof(__u32), 10000},
        {"map_netd_uid_counterset_map", BPF_MAP_TYPE_HASH, sizeof(__u32), sizeof(__u8), 20000},
        {"map_netd_stats_map_A", BPF_MAP_TYPE_HASH, 16, 32, 10000},
        {"map_netd_stats_map_B", BPF_MAP_TYPE_HASH, 16, 32, 10000},
        {"map_netd_app_uid_stats_map", BPF_MAP_TYPE_HASH, sizeof(__u32), 32, 10000},
        {"map_netd_iface_stats_map", BPF_MAP_TYPE_HASH, sizeof(__u32), 32, 1000},
        {"map_netd_iface_index_name_map", BPF_MAP_TYPE_HASH, sizeof(__u32), 16, 1000},
        {"map_netd_data_saver_enabled_map", BPF_MAP_TYPE_ARRAY, sizeof(__u32), sizeof(__u8), 1},
        {"map_netd_ingress_discard_map", BPF_MAP_TYPE_HASH, 16, 2 * sizeof(__u32), 100},
        {"map_netd_local_net_access_map", BPF_MAP_TYPE_HASH, 28, sizeof(__u8), 1000},
        {"map_netd_local_net_blocked_uid_map", BPF_MAP_TYPE_HASH, sizeof(__u32), sizeof(__u8), 1000},
};

// ClatCoordinator verifies these Android T+ objects during SystemServer JNI
// loading.  The legacy kernel cannot support Android's Clat tc program set,
// so retain empty maps and no-match placeholder programs only.  This keeps
// IPv4 connectivity available while deliberately leaving 464XLAT disabled.
constexpr const char* kClatPrograms[] = {
        "prog_clatd_schedcls_egress4_clat_rawip",
        "prog_clatd_schedcls_ingress6_clat_rawip",
        "prog_clatd_schedcls_ingress6_clat_ether",
};

constexpr MapSpec kClatMaps[] = {
        {"map_clatd_clat_egress4_map", BPF_MAP_TYPE_HASH, 8, 56, 16},
        {"map_clatd_clat_ingress6_map", BPF_MAP_TYPE_HASH, 36, 24, 16},
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
    attr.key_size = spec.key_size;
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

bool pinObject(const char* path, int fd) {
    union bpf_attr attr = {};
    attr.pathname = reinterpret_cast<__u64>(path);
    attr.bpf_fd = fd;
    return bpf(BPF_OBJ_PIN, &attr) == 0;
}

bool setSystemObjectPermissions(const char* path, mode_t mode) {
    if (chmod(path, mode) || chown(path, 0, kSystemUid)) {
        ALOGE("failed to set permissions on %s: %s", path, strerror(errno));
        return false;
    }
    return true;
}

bool ensureProgram(const char* root, const char* name, bool set_system_permissions) {
    char path[PATH_MAX] = {};
    snprintf(path, sizeof(path), "%s/%s", root, name);

    const int existing = openPinnedObject(path);
    if (existing >= 0) {
        close(existing);
        return !set_system_permissions || setSystemObjectPermissions(path, 0440);
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
    const bool pinned = pinObject(path, program);
    if (!pinned) ALOGE("BPF_OBJ_PIN(%s) failed: %s", path, strerror(errno));
    close(program);
    return pinned && (!set_system_permissions || setSystemObjectPermissions(path, 0440));
}

bool ensureMap(const char* root, const MapSpec& spec, bool set_system_permissions) {
    char path[PATH_MAX] = {};
    snprintf(path, sizeof(path), "%s/%s", root, spec.name);

    const int existing = openPinnedObject(path);
    if (existing >= 0) {
        close(existing);
        return !set_system_permissions || setSystemObjectPermissions(path, 0660);
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
    const bool pinned = pinObject(path, map);
    if (!pinned) ALOGE("BPF_OBJ_PIN(%s) failed: %s", path, strerror(errno));
    close(map);
    return pinned && (!set_system_permissions || setSystemObjectPermissions(path, 0660));
}

bool ensureSharedDirectory(const char* path) {
    if (mkdir(path, 01777) && errno != EEXIST) {
        ALOGE("mkdir(%s) failed: %s", path, strerror(errno));
        return false;
    }
    if (chmod(path, 01777) || chown(path, 0, 0)) {
        ALOGE("failed to set permissions on %s: %s", path, strerror(errno));
        return false;
    }
    return true;
}

}  // namespace

int main() {
    if (!ensureSharedDirectory(kNetdBpfRoot)) return 1;
    for (const char* program : kPrograms) {
        if (!ensureProgram(kNetdBpfRoot, program, false)) return 1;
    }
    for (const MapSpec& map : kMaps) {
        if (!ensureMap(kNetdBpfRoot, map, true)) return 1;
    }
    if (!ensureSharedDirectory(kClatBpfRoot)) return 1;
    for (const char* program : kClatPrograms) {
        if (!ensureProgram(kClatBpfRoot, program, true)) return 1;
    }
    for (const MapSpec& map : kClatMaps) {
        if (!ensureMap(kClatBpfRoot, map, true)) return 1;
    }
    return 0;
}
