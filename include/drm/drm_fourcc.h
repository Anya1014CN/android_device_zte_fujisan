#pragma once

#include_next <drm/drm_fourcc.h>

// Legacy fourcc_mod_code() expects the numeric vendor ID directly.
#ifndef QCOM
#define QCOM 0x05
#endif
