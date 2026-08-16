/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Bridge the constructor map ABI used by Android 8 generated HIDL blobs to
 * the accessor ABI used by current libhidlbase.  The global definitions match
 * LineageOS's libhidlbase_shim, but are deliberately co-located with the
 * accessors to guarantee static-initialization order in the HWC process.
 */

#include <hidl/Static.h>

#include <cstdio>
#include <string>

namespace {

std::string gnssUnknownValue(uint32_t value) {
    char text[11];
    std::snprintf(text, sizeof(text), "0x%x", value);
    return text;
}

}  // namespace

// Android 8's generated GNSS interface emitted these formatters as exported
// functions. Current HIDL generates them inline, but the OEM QTI interface
// still links against their old ABI. Their values and strings come directly
// from hardware/interfaces/gnss/1.0/IGnssNiCallback.h.
std::string gnssNiNotifyFlagsToString(uint32_t value)
        __asm__("_ZN7android8hardware4gnss4V1_08toStringINS2_15IGnssNiCallback17GnssNiNotifyFlagsEEENSt3__112basic_stringIcNS6_11char_traitsIcEENS6_9allocatorIcEEEEj");
std::string gnssNiNotifyFlagsToString(uint32_t value) {
    std::string text;
    uint32_t known = 0;
    if (value & 0x1) { text += "NEED_NOTIFY"; known |= 0x1; }
    if (value & 0x2) { text += (text.empty() ? "" : " | "); text += "NEED_VERIFY"; known |= 0x2; }
    if (value & 0x4) { text += (text.empty() ? "" : " | "); text += "PRIVACY_OVERRIDE"; known |= 0x4; }
    if (value != known) { text += (text.empty() ? "" : " | "); text += gnssUnknownValue(value & ~known); }
    text += " (" + gnssUnknownValue(value) + ")";
    return text;
}

std::string gnssUserResponseTypeToString(uint8_t value)
        __asm__("_ZN7android8hardware4gnss4V1_08toStringENS2_15IGnssNiCallback20GnssUserResponseTypeE");
std::string gnssUserResponseTypeToString(uint8_t value) {
    if (value == 1) return "RESPONSE_ACCEPT";
    if (value == 2) return "RESPONSE_DENY";
    if (value == 3) return "RESPONSE_NORESP";
    return gnssUnknownValue(value);
}

std::string gnssNiEncodingTypeToString(int32_t value)
        __asm__("_ZN7android8hardware4gnss4V1_08toStringENS2_15IGnssNiCallback18GnssNiEncodingTypeE");
std::string gnssNiEncodingTypeToString(int32_t value) {
    if (value == 0) return "ENC_NONE";
    if (value == 1) return "ENC_SUPL_GSM_DEFAULT";
    if (value == 2) return "ENC_SUPL_UTF8";
    if (value == 3) return "ENC_SUPL_UCS2";
    if (value == -1) return "ENC_UNKNOWN";
    return gnssUnknownValue(static_cast<uint32_t>(value));
}

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
