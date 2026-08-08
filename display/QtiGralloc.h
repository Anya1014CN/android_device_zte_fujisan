/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <hardware/gralloc.h>
#include <stdint.h>

// Matches CAF msm8996 gralloc's private WFD consumer usage bit.
#ifndef GRALLOC_USAGE_PRIVATE_WFD
#define GRALLOC_USAGE_PRIVATE_WFD (UINT32_C(1) << 21)
#endif
