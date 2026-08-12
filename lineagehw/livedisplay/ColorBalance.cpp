/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "ColorBalanceService"

#include "ColorBalance.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>

namespace {
constexpr int32_t kHueMin = 0;
constexpr int32_t kHueMax = 512;
constexpr int32_t kBalanceMin = -100;
constexpr int32_t kBalanceMax = 100;
}  // namespace

using ::android::base::ReadFileToString;
using ::android::base::Trim;
using ::android::base::WriteStringToFile;
namespace aidl {
namespace vendor {
namespace lineage {
namespace livedisplay {
namespace fujisan {

ndk::ScopedAStatus ColorBalance::getColorBalanceRange(Range* _aidl_return) {
    _aidl_return->max = kBalanceMax;
    _aidl_return->min = kBalanceMin;
    _aidl_return->step = 1;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ColorBalance::getColorBalance(int32_t* _aidl_return) {
    std::string value;
    if (!ReadFileToString(kPanelHue, &value)) {
        LOG(ERROR) << "Failed to read " << kPanelHue;
        return ndk::ScopedAStatus::fromExceptionCode(EX_SERVICE_SPECIFIC);
    }

    char* end = nullptr;
    const std::string trimmed = Trim(value);
    const long hue = strtol(trimmed.c_str(), &end, 10);
    if (end == nullptr || *end != '\0') {
        LOG(ERROR) << "Invalid hue value: " << value;
        return ndk::ScopedAStatus::fromExceptionCode(EX_SERVICE_SPECIFIC);
    }

    *_aidl_return = hueToBalance(static_cast<int32_t>(hue));
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ColorBalance::setColorBalance(int32_t value) {
    value = std::max(kBalanceMin, std::min(kBalanceMax, value));
    const int32_t hue = balanceToHue(value);

    const std::string encoded = std::to_string(hue);
    if (!WriteStringToFile(encoded, kPanelHue, true)) {
        LOG(ERROR) << "Failed to write " << hue << " to " << kPanelHue;
        return ndk::ScopedAStatus::fromExceptionCode(EX_SERVICE_SPECIFIC);
    }

    /* The second panel is absent while folded.  Its kernel endpoint retains
     * the requested value and applies it on the next panel-on, so synchronize
     * it when available without making a folded device report a false error. */
    if (access(kSecondaryPanelHue, W_OK) == 0 &&
            !WriteStringToFile(encoded, kSecondaryPanelHue, true)) {
        LOG(ERROR) << "Failed to write " << hue << " to " << kSecondaryPanelHue;
        return ndk::ScopedAStatus::fromExceptionCode(EX_SERVICE_SPECIFIC);
    }

    return ndk::ScopedAStatus::ok();
}

int32_t ColorBalance::hueToBalance(int32_t hue) {
    hue = std::max(kHueMin, std::min(kHueMax, hue));
    if (hue >= kDefaultHue) {
        return -((hue - kDefaultHue) * kBalanceMax) /
                (kHueMax - kDefaultHue);
    }
    return ((kDefaultHue - hue) * kBalanceMax) / kDefaultHue;
}

int32_t ColorBalance::balanceToHue(int32_t balance) {
    balance = std::max(kBalanceMin, std::min(kBalanceMax, balance));
    if (balance < 0) {
        return kDefaultHue +
                ((-balance) * (kHueMax - kDefaultHue)) / kBalanceMax;
    }
    return kDefaultHue - (balance * kDefaultHue) / kBalanceMax;
}

}  // namespace fujisan
}  // namespace livedisplay
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
