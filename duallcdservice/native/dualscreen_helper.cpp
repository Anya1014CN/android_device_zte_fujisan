#include <android/hardware/graphics/composer/2.1/IComposer.h>
#include <android/hardware/graphics/composer/2.1/IComposerClient.h>
#include <android/hardware/graphics/composer/2.1/types.h>
#include <utils/Log.h>
#include <cutils/properties.h>
#include <unistd.h>

using namespace android::hardware::graphics::composer::V2_1;
using android::sp;

int main() {
    ALOGI("dualscreen-helper: starting");

    for (int i = 0; i < 30; i++) {
        sp<IComposer> composer = IComposer::getService();
        if (composer == nullptr) {
            ALOGI("dualscreen-helper: waiting for composer service... (%d)", i);
            sleep(1);
            continue;
        }

        ALOGI("dualscreen-helper: got composer service");

        // Create a client
        sp<IComposerClient> client;
        composer->createClient([&](const auto& err, const auto& c) {
            if (err == Error::NONE) {
                client = c;
                ALOGI("dualscreen-helper: composer client created");
            } else {
                ALOGE("dualscreen-helper: createClient failed, err=%d",
                      static_cast<int>(err));
            }
        });

        if (client == nullptr) {
            ALOGE("dualscreen-helper: null client");
            return 1;
        }

        // Set power mode ON for external display (HIDL Display enum:
        // INVALID=0, DISPLAY_PRIMARY=1, DISPLAY_EXTERNAL=2)
        const Display kExternalDisplay = static_cast<Display>(2);
        Error err = client->setPowerMode(kExternalDisplay,
            IComposerClient::PowerMode::ON);
        if (err == Error::NONE) {
            ALOGI("dualscreen-helper: setPowerMode(ON) for display 2 SUCCESS");
        } else {
            ALOGE("dualscreen-helper: setPowerMode failed, err=%d",
                  static_cast<int>(err));
        }

        // Also try to register a callback (this forces SurfaceFlinger
        // to know about the display)
        composer->registerCallback(nullptr, [](const auto&, const auto&, const auto&) {});

        return (err == Error::NONE) ? 0 : 1;
    }

    ALOGE("dualscreen-helper: timed out waiting for composer");
    return 1;
}
