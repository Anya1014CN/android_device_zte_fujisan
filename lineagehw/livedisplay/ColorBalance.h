/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/vendor/lineage/livedisplay/BnColorBalance.h>

namespace aidl {
namespace vendor {
namespace lineage {
namespace livedisplay {
namespace fujisan {

class ColorBalance final : public BnColorBalance {
public:
    static constexpr const char* kPanelHue = "/proc/panel_hue_0_set";
    static constexpr const char* kSecondaryPanelHue = "/proc/panel_hue_1_set";
    static constexpr int32_t kDefaultHue = 255;

    ndk::ScopedAStatus getColorBalanceRange(Range* _aidl_return) override;
    ndk::ScopedAStatus getColorBalance(int32_t* _aidl_return) override;
    ndk::ScopedAStatus setColorBalance(int32_t value) override;

private:
    static int32_t hueToBalance(int32_t hue);
    static int32_t balanceToHue(int32_t balance);
};

}  // namespace fujisan
}  // namespace livedisplay
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
