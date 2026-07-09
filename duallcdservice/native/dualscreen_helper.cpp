#include <vendor/display/config/1.0/IDisplayConfig.h>
#include <vendor/display/config/1.1/IDisplayConfig.h>
#include <utils/Log.h>
#include <unistd.h>

using android::sp;

namespace V1_0 = vendor::display::config::V1_0;
namespace V1_1 = vendor::display::config::V1_1;

int main() {
    ALOGI("dualscreen-helper: starting");

    for (int i = 0; i < 30; i++) {
        sp<V1_0::IDisplayConfig> base = V1_0::IDisplayConfig::getService();
        if (base == nullptr) {
            ALOGI("dualscreen-helper: waiting for IDisplayConfig... (%d)", i);
            sleep(1);
            continue;
        }

        ALOGI("dualscreen-helper: got IDisplayConfig@1.0, casting to @1.1");

        auto ret = V1_1::IDisplayConfig::castFrom(base);
        if (!ret.isOk()) {
            ALOGE("dualscreen-helper: cast to @1.1 failed");
            return 1;
        }
        sp<V1_1::IDisplayConfig> config = ret;
        if (config == nullptr) {
            ALOGE("dualscreen-helper: cast returned null");
            return 1;
        }

        ALOGI("dualscreen-helper: calling setSecondayDisplayStatus");
        auto extType = static_cast<V1_0::IDisplayConfig::DisplayType>(1);
        auto status = static_cast<V1_0::IDisplayConfig::DisplayExternalStatus>(1);

        auto callRet = config->setSecondayDisplayStatus(extType, status);
        if (callRet.isOk()) {
            ALOGI("dualscreen-helper: setSecondayDisplayStatus SUCCESS");
            return 0;
        }
        ALOGE("dualscreen-helper: setSecondayDisplayStatus failed");
        return 1;
    }

    ALOGE("dualscreen-helper: timed out");
    return 1;
}
