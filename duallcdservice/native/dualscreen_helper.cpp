#include <vendor/display/config/1.0/IDisplayConfig.h>
#include <vendor/display/config/1.1/IDisplayConfig.h>
#include <utils/Log.h>
#include <unistd.h>

using android::sp;

int main() {
    ALOGI("dualscreen-helper: starting");

    for (int i = 0; i < 30; i++) {
        auto configV1_0 = vendor::display::config::V1_0::IDisplayConfig::getService();
        if (configV1_0 == nullptr) {
            ALOGI("dualscreen-helper: waiting for IDisplayConfig@1.0... (%d)", i);
            sleep(1);
            continue;
        }

        ALOGI("dualscreen-helper: got IDisplayConfig@1.0, casting to @1.1");

        auto config = vendor::display::config::V1_1::IDisplayConfig::castFrom(configV1_0);
        if (config == nullptr) {
            ALOGE("dualscreen-helper: cast to @1.1 failed");
            return 1;
        }

        auto extType = static_cast<
            vendor::display::config::V1_0::IDisplayConfig::DisplayType>(1);

        auto status = static_cast<
            vendor::display::config::V1_0::IDisplayConfig::DisplayExternalStatus>(1);

        auto ret = config->setSecondayDisplayStatus(extType, status);
        if (ret.isOk()) {
            ALOGI("dualscreen-helper: setSecondayDisplayStatus(1,1) SUCCESS");
            return 0;
        }
        ALOGE("dualscreen-helper: setSecondayDisplayStatus failed");
        return 1;
    }

    ALOGE("dualscreen-helper: timed out");
    return 1;
}
