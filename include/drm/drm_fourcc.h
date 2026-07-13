#pragma once

#include_next <drm/drm_fourcc.h>

// Qualcomm modifier encoding used by the msm8996 display HAL.
#ifndef DRM_FORMAT_MOD_QCOM_COMPRESSED
#define DRM_FORMAT_MOD_QCOM_COMPRESSED ((0x05ULL << 56) | 0x1ULL)
#endif

#ifndef DRM_FORMAT_MOD_QCOM_DX
#define DRM_FORMAT_MOD_QCOM_DX ((0x05ULL << 56) | 0x2ULL)
#endif

#ifndef DRM_FORMAT_MOD_QCOM_TIGHT
#define DRM_FORMAT_MOD_QCOM_TIGHT ((0x05ULL << 56) | 0x4ULL)
#endif
