#include <vendor/display/config/1.0/IDisplayConfig.h>
#include <cutils/properties.h>
#include <utils/Log.h>
#include <stdlib.h>
#include <unistd.h>

using android::sp;
using vendor::display::config::V1_0::IDisplayConfig;

static void launchSecondaryHomeIfDocked(const char *mode) {
    if (mode[0] != '4') {
        return;
    }

    system("/system/bin/am startservice "
           "-n org.lineageos.fujisan.secondarysysui/"
           ".SecondarySystemUiService >/dev/null 2>&1");

    sleep(2);
    ALOGI("dualscreen-helper: launching secondary launcher on display 1");
    int ret = system("/system/bin/am start --display 1 "
                     "-n org.lineageos.fujisan.secondarysysui/"
                     ".SecondaryLauncherActivity "
                     ">/dev/null 2>&1");
    if (ret != 0) {
        ALOGW("dualscreen-helper: secondary launcher failed (%d), trying Trebuchet", ret);
        ret = system("/system/bin/am start --display 1 "
                     "-n org.lineageos.trebuchet/"
                     "com.android.launcher3.searchlauncher.SearchLauncher "
                     ">/dev/null 2>&1");
    }
    if (ret != 0) {
        ALOGW("dualscreen-helper: Trebuchet launch failed (%d), trying default HOME", ret);
        ret = system("/system/bin/am start --display 1 "
                     "-a android.intent.action.MAIN "
                     "-c android.intent.category.HOME >/dev/null 2>&1");
    }
    ALOGI("dualscreen-helper: secondary HOME launch result=%d", ret);

    int primary_ret = system("/system/bin/am start --display 0 "
                             "-n org.lineageos.trebuchet/"
                             "com.android.launcher3.searchlauncher.SearchLauncher "
                             ">/dev/null 2>&1");
    if (primary_ret != 0) {
        ALOGW("dualscreen-helper: primary Trebuchet launch failed (%d), trying default HOME",
              primary_ret);
        primary_ret = system("/system/bin/am start --display 0 "
                             "-a android.intent.action.MAIN "
                             "-c android.intent.category.HOME >/dev/null 2>&1");
    }
    ALOGI("dualscreen-helper: primary HOME launch result=%d", primary_ret);
}

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

        launchSecondaryHomeIfDocked(mode);
        return 0;
    }

    ALOGE("dualscreen-helper: timed out");
    return 1;
}
