#define LOG_TAG "dualscreen-preload"
#include <hardware/hardware.h>
#include <hardware/hwcomposer2.h>
#include <utils/Log.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

static hwc2_device_t* g_hwc2_dev = nullptr;

static int (*real_hw_device_open)(const struct hw_module_t*,
    const char*, struct hw_device_t**) = nullptr;

static void* delayed_init(void*) {
    sleep(3);
    if (!g_hwc2_dev) {
        ALOGE("dualscreen-preload: no hwc2 device");
        return nullptr;
    }

    if (!g_hwc2_dev->getFunction) {
        ALOGE("dualscreen-preload: no getFunction hook");
        return nullptr;
    }

    // Try to set power mode ON for external display (id=1)
    auto setPowerMode = reinterpret_cast<HWC2_PFN_SET_POWER_MODE>(
        g_hwc2_dev->getFunction(g_hwc2_dev, HWC2_FUNCTION_SET_POWER_MODE));
    if (setPowerMode) {
        // First try to get display type to see if display 1 exists
        auto getDisplayType = reinterpret_cast<HWC2_PFN_GET_DISPLAY_TYPE>(
            g_hwc2_dev->getFunction(g_hwc2_dev, HWC2_FUNCTION_GET_DISPLAY_TYPE));
        if (getDisplayType) {
            int32_t type = -1;
            getDisplayType(g_hwc2_dev, 1, &type);
            ALOGI("dualscreen-preload: display 1 type=%d", type);
        }

        // Set power mode ON for display 1
        int32_t err = setPowerMode(g_hwc2_dev, 1, HWC2_POWER_MODE_ON);
        ALOGI("dualscreen-preload: setPowerMode(1, ON) = %d", err);

        // Also try display 2
        err = setPowerMode(g_hwc2_dev, 2, HWC2_POWER_MODE_ON);
        ALOGI("dualscreen-preload: setPowerMode(2, ON) = %d", err);
    } else {
        ALOGE("dualscreen-preload: no setPowerMode function");
    }

    return nullptr;
}

extern "C" int hw_device_open(const struct hw_module_t* module,
    const char* id, struct hw_device_t** device) {

    if (!real_hw_device_open) {
        real_hw_device_open = (int(*)(const struct hw_module_t*,
            const char*, struct hw_device_t**))
            dlsym(RTLD_NEXT, "hw_device_open");
        if (!real_hw_device_open) {
            ALOGE("dualscreen-preload: can't find real hw_device_open");
            return -1;
        }
    }

    int ret = real_hw_device_open(module, id, device);
    if (ret == 0 && device && *device) {
        if (strcmp(id, HWC_HARDWARE_COMPOSER) == 0) {
            g_hwc2_dev = (hwc2_device_t*)(*device);
            ALOGI("dualscreen-preload: captured hwc2 device %p", g_hwc2_dev);
            pthread_t t;
            pthread_create(&t, nullptr, delayed_init, nullptr);
        }
    }
    return ret;
}
