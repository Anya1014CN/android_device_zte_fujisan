/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Bridge the constructor map ABI used by Android 8 generated HIDL blobs to
 * the accessor ABI used by current libhidlbase.  The global definitions match
 * LineageOS's libhidlbase_shim, but are deliberately co-located with the
 * accessors to guarantee static-initialization order in the HWC process.
 */

#include <hidl/Static.h>

namespace android {
namespace hardware {
namespace details {

// Deprecated Android 8 ABI; retained for proprietary generated interfaces.
DoNotDestruct<BnConstructorMap> gBnConstructorMap{};
DoNotDestruct<BsConstructorMap> gBsConstructorMap{};

BnConstructorMap& getBnConstructorMap() {
    return gBnConstructorMap.get();
}

BsConstructorMap& getBsConstructorMap() {
    return gBsConstructorMap.get();
}

}  // namespace details
}  // namespace hardware
}  // namespace android
