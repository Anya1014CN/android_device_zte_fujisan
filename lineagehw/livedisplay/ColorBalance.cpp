/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "vendor.lineage.livedisplay@2.0-impl-fujisan"

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
using ::android::hardware::Void;

namespace vendor {
namespace lineage {
namespace livedisplay {
namespace V2_0 {
namespace fujisan {

bool ColorBalance::isSupported() {
    return access(kPanelHue, R_OK | W_OK) == 0;
}

Return<void> ColorBalance::getColorBalanceRange(getColorBalanceRange_cb _hidl_cb) {
    _hidl_cb(Range{kBalanceMax, kBalanceMin, 1});
    return Void();
}

Return<int32_t> ColorBalance::getColorBalance() {
    std::string value;
    if (!ReadFileToString(kPanelHue, &value)) {
        LOG(ERROR) << "Failed to read " << kPanelHue;
        return 0;
    }

    char* end = nullptr;
    const std::string trimmed = Trim(value);
    const long hue = strtol(trimmed.c_str(), &end, 10);
    if (end == nullptr || *end != '\0') {
        LOG(ERROR) << "Invalid hue value: " << value;
        return 0;
    }

    return hueToBalance(static_cast<int32_t>(hue));
}

Return<bool> ColorBalance::setColorBalance(int32_t value) {
    value = std::max(kBalanceMin, std::min(kBalanceMax, value));
    const int32_t hue = balanceToHue(value);

    if (!WriteStringToFile(std::to_string(hue), kPanelHue, true)) {
        LOG(ERROR) << "Failed to write " << hue << " to " << kPanelHue;
        return false;
    }

    return true;
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
}  // namespace V2_0
}  // namespace livedisplay
}  // namespace lineage
}  // namespace vendor
