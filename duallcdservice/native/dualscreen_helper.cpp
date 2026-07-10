#include <vendor/display/config/1.0/IDisplayConfig.h>
#include <cutils/properties.h>
#include <utils/Log.h>
#include <unistd.h>

using android::sp;
using vendor::display::config::V1_0::IDisplayConfig;

int main() {
    ALOGI("dualscreen-helper: starting");

    char mode[PROPERTY_VALUE_MAX];
    property_get("persist.vendor.fujisan.display_mode", mode, "1");
    if (mode[0] != '2' && mode[0] != '4' && mode[0] != '8') {
        ALOGI("dualscreen-helper: display mode %s does not need secondary bring-up", mode);
        return 0;
    }

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
        auto online = static_cast<IDisplayConfig::DisplayExternalStatus>(1);
        auto resume = static_cast<IDisplayConfig::DisplayExternalStatus>(3);

        bool connected = false;
        cfg->isDisplayConnected(ext,
            [&](int err, bool conn) {
                connected = conn;
                ALOGI("dualscreen-helper: external connected=%d err=%d", conn, err);
            });

        auto status_ret = cfg->setSecondayDisplayStatus(ext, online);
        if (status_ret.isOk()) {
            int32_t err = status_ret;
            ALOGI("dualscreen-helper: setSecondaryDisplayStatus(EXTERNAL,ONLINE) err=%d",
                  err);
        } else {
            ALOGE("dualscreen-helper: setSecondaryDisplayStatus(EXTERNAL,ONLINE) failed");
        }

        usleep(200 * 1000);

        status_ret = cfg->setSecondayDisplayStatus(ext, resume);
        if (status_ret.isOk()) {
            int32_t err = status_ret;
            ALOGI("dualscreen-helper: setSecondaryDisplayStatus(EXTERNAL,RESUME) err=%d",
                  err);
        } else {
            ALOGE("dualscreen-helper: setSecondaryDisplayStatus(EXTERNAL,RESUME) failed");
        }

        auto ret = cfg->setActiveConfig(ext, 0);
        ALOGI("dualscreen-helper: setActiveConfig(EXTERNAL,0) %s",
              ret.isOk() ? "ok" : "failed");

        ret = cfg->setActiveConfig(pri, 0);
        ALOGI("dualscreen-helper: setActiveConfig(PRIMARY,0) %s",
              ret.isOk() ? "ok" : "failed");

        auto refresh_ret = cfg->refreshScreen();
        ALOGI("dualscreen-helper: refreshScreen %s",
              refresh_ret.isOk() ? "ok" : "failed");

        return 0;
    }

    ALOGE("dualscreen-helper: timed out");
    return 1;
}
