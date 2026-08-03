/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "LocSvc_GnssXtra"

#include "GnssXtra.h"
#include "Gnss.h"

#include <log/log.h>

namespace android {
namespace hardware {
namespace gnss {
namespace V1_0 {
namespace implementation {

Return<bool> GnssXtra::setCallback(const sp<IGnssXtraCallback>& callback) {
    mCallback = callback;
    return mCallback != nullptr;
}

Return<bool> GnssXtra::injectXtraData(const hidl_string& xtraData) {
    GnssInterface* gnssInterface = mGnss ? mGnss->getGnssInterface() : nullptr;
    if (gnssInterface == nullptr || xtraData.empty()) {
        ALOGE("Cannot inject PSDS data: GNSS interface unavailable or payload empty");
        return false;
    }

    ALOGI("Queueing %zu bytes of PSDS data from framework", xtraData.size());
    gnssInterface->injectXtraData(xtraData.c_str(), xtraData.size());
    return true;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace gnss
}  // namespace hardware
}  // namespace android
