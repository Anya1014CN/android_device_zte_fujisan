#include <vendor/display/config/1.0/IDisplayConfig.h>
#include <utils/Log.h>
#include <unistd.h>

using android::sp;
using vendor::display::config::V1_0::IDisplayConfig;

int main() {
    ALOGI("dualscreen-helper: starting");

    for (int i = 0; i < 30; i++) {
        sp<IDisplayConfig> cfg = IDisplayConfig::getService();
        if (cfg == nullptr) {
            ALOGI("dualscreen-helper: waiting... (%d)", i);
            sleep(1);
            continue;
        }

        ALOGI("dualscreen-helper: got IDisplayConfig");

        // DisplayType enum values: 0=PRIMARY, 1=EXTERNAL/HDMI
        auto ext = static_cast<IDisplayConfig::DisplayType>(1);
        auto pri = static_cast<IDisplayConfig::DisplayType>(0);

        bool connected = false;
        cfg->isDisplayConnected(ext,
            [&](int err, bool conn) {
                connected = conn;
                ALOGI("dualscreen-helper: external connected=%d err=%d", conn, err);
            });

        auto ret = cfg->setActiveConfig(ext, 0);
        ALOGI("dualscreen-helper: setActiveConfig(EXTERNAL,0) %s",
              ret.isOk() ? "ok" : "failed");

        ret = cfg->setActiveConfig(pri, 0);
        ALOGI("dualscreen-helper: setActiveConfig(PRIMARY,0) %s",
              ret.isOk() ? "ok" : "failed");

        return 0;
    }

    ALOGE("dualscreen-helper: timed out");
    return 1;
}
