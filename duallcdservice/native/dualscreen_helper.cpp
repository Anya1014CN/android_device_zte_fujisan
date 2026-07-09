#include <vendor/display/config/1.1/IDisplayConfig.h>
#include <utils/Log.h>
#include <unistd.h>

using android::sp;
using vendor::display::config::V1_1::IDisplayConfig;

int main() {
    ALOGI("dualscreen-helper: starting");

    for (int i = 0; i < 30; i++) {
        sp<IDisplayConfig> config = IDisplayConfig::getService();
        if (config == nullptr) {
            ALOGI("dualscreen-helper: waiting for IDisplayConfig... (%d)", i);
            sleep(1);
            continue;
        }

        ALOGI("dualscreen-helper: got IDisplayConfig service");

        // DisplayType: 0=PRIMARY, 1=EXTERNAL/HDMI
        // DisplayExternalStatus: 0=OFFLINE, 1=ONLINE
        auto extType = static_cast<IDisplayConfig::DisplayType>(1);
        auto status = static_cast<IDisplayConfig::DisplayExternalStatus>(1);

        auto ret = config->setSecondayDisplayStatus(extType, status);
        if (ret.isOk()) {
            ALOGI("dualscreen-helper: setSecondayDisplayStatus(1, 1) SUCCESS");
            return 0;
        }
        ALOGE("dualscreen-helper: setSecondayDisplayStatus failed");
        return 1;
    }

    ALOGE("dualscreen-helper: timed out");
    return 1;
}
