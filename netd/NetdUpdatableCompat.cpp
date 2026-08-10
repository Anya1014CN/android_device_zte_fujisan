/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Fujisan's 4.4 kernel does not implement the eBPF ABI required by Android
 * 16's Tethering APEX. These are the complete public libnetd_updatable ABI
 * entry points. Preloading this library lets the unmodified platform netd
 * retain its non-BPF network management functionality.
 */

#include <stdint.h>
#include <sys/types.h>

extern "C" int libnetd_updatable_init(const char*) {
    return 0;
}

extern "C" int libnetd_updatable_tagSocket(int, uint32_t, uid_t, uid_t) {
    return 0;
}

extern "C" int libnetd_updatable_untagSocket(int) {
    return 0;
}
