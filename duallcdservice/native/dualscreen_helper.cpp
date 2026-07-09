#include <vendor/display/config/1.1/IDisplayConfig.h>
#include <utils/Log.h>
#include <unistd.h>

using android::sp;
using vendor::display::config::V1_1::IDisplayConfig;
using vendor::display::config::V1_0::IDisplayConfig as IDisplayConfigV1_0;

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

        // DisplayType::EXTERNAL = 1
        // DisplayExternalStatus: EXTERNAL_OFFLINE=0, EXTERNAL_ONLINE=1
        IDisplayConfigV1_0::DisplayType extType =
            IDisplayConfigV1_0::DisplayType::EXTERNAL;
        IDisplayConfig::DisplayExternalStatus status =
            IDisplayConfig::DisplayExternalStatus::EXTERNAL_ONLINE;

        auto ret = config->setSecondayDisplayStatus(extType, status);
        if (ret.isOk()) {
            ALOGI("dualscreen-helper: setSecondayDisplayStatus(EXTERNAL, ONLINE) SUCCESS");
            return 0;
        } else {
            ALOGE("dualscreen-helper: setSecondayDisplayStatus failed");
        }

        return 1;
    }

    ALOGE("dualscreen-helper: timed out waiting for IDisplayConfig");
    return 1;
}
