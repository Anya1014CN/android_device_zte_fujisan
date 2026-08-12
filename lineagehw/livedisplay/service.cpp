/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "vendor.lineage.livedisplay-service.fujisan"

#include "ColorBalance.h"

#include <cstdlib>

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

using ::aidl::vendor::lineage::livedisplay::fujisan::ColorBalance;

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(0);

    auto colorBalance = ndk::SharedRefBase::make<ColorBalance>();
    const std::string instance = std::string(ColorBalance::descriptor) + "/default";
    CHECK_EQ(AServiceManager_addService(colorBalance->asBinder().get(), instance.c_str()), STATUS_OK);

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;
}
