/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "vendor.lineage.livedisplay@2.0-service.fujisan"

#include "ColorBalance.h"

#include <android-base/logging.h>
#include <hidl/HidlTransportSupport.h>

using ::android::OK;
using ::android::sp;
using ::android::hardware::configureRpcThreadpool;
using ::android::hardware::joinRpcThreadpool;
using ::vendor::lineage::livedisplay::V2_0::fujisan::ColorBalance;

int main() {
    configureRpcThreadpool(1, true);

    if (!ColorBalance::isSupported()) {
        LOG(ERROR) << "Fujisan panel hue interface is unavailable";
        return 1;
    }

    sp<ColorBalance> colorBalance = new ColorBalance();
    if (colorBalance->registerAsService() != OK) {
        LOG(ERROR) << "Could not register IColorBalance";
        return 1;
    }

    joinRpcThreadpool();
    return 0;
}
