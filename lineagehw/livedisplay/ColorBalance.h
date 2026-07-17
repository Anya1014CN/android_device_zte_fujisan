/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <vendor/lineage/livedisplay/2.0/IColorBalance.h>

namespace vendor {
namespace lineage {
namespace livedisplay {
namespace V2_0 {
namespace fujisan {

using ::android::hardware::Return;

class ColorBalance : public IColorBalance {
public:
    static constexpr const char* kPanelHue = "/proc/panel_hue_0_set";
    static constexpr int32_t kDefaultHue = 255;

    static bool isSupported();

    Return<Range> getColorBalanceRange() override;
    Return<int32_t> getColorBalance() override;
    Return<bool> setColorBalance(int32_t value) override;

private:
    static int32_t hueToBalance(int32_t hue);
    static int32_t balanceToHue(int32_t balance);
};

}  // namespace fujisan
}  // namespace V2_0
}  // namespace livedisplay
}  // namespace lineage
}  // namespace vendor
