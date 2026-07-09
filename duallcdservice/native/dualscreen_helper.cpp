#include <vendor/display/config/1.0/IDisplayConfig.h>
#include <utils/Log.h>
#include <unistd.h>

using android::sp;
using V1_0 = vendor::display::config::V1_0;

int main() {
    ALOGI("dualscreen-helper: starting");

    for (int i = 0; i < 30; i++) {
        auto cfg = V1_0::IDisplayConfig::getService();
        if (cfg == nullptr) {
            ALOGI("dualscreen-helper: waiting... (%d)", i);
            sleep(1);
            continue;
        }

        ALOGI("dualscreen-helper: got IDisplayConfig@1.0");

        // Check if external display is connected
        bool connected = false;
        cfg->isDisplayConnected(V1_0::IDisplayConfig::DisplayType::EXTERNAL,
            [&](bool conn) {
                connected = conn;
                ALOGI("dualscreen-helper: external connected=%d", conn);
            });

        // Try setting active config on external display
        // This may trigger HWC to initialize/connect the display
        auto ret = cfg->setActiveConfig(
            V1_0::IDisplayConfig::DisplayType::EXTERNAL, 0);
        ALOGI("dualscreen-helper: setActiveConfig(EXTERNAL,0) %s",
              ret.isOk() ? "ok" : "failed");

        // Also try setting active config on primary
        ret = cfg->setActiveConfig(
            V1_0::IDisplayConfig::DisplayType::PRIMARY, 0);
        ALOGI("dualscreen-helper: setActiveConfig(PRIMARY,0) %s",
              ret.isOk() ? "ok" : "failed");

        return 0;
    }

    ALOGE("dualscreen-helper: timed out");
    return 1;
}
