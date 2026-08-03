/*
 * ZTE's proprietary location clients use the older msm8996 loc_core ABI.
 * Keep libloc_core proprietary and adapt the three source GNSS calls here.
 */
#include <stdint.h>
#include <dlfcn.h>

extern "C" bool sourceEventConnectionStatus(void* self, bool connected, int8_t type)
        __asm__("_ZN8loc_core12SystemStatus21eventConnectionStatusEba");

extern "C" void sourceLocAdapterBaseCtor(void* self, uint64_t mask, void* context, void* proxy)
        __asm__("_ZN8loc_core14LocAdapterBaseC2EmPNS_11ContextBaseEPNS_19LocAdapterProxyBaseE");

extern "C" bool sourceSetDefaultGnssEngineStates(void* self)
        __asm__("_ZN8loc_core12SystemStatus26setDefaultGnssEngineStatesEv");

extern "C" int zteSetGpsLock(void* self, int lock)
{
    using SetGpsLock = int (*)(void*, int);
    static SetGpsLock setter = []() -> SetGpsLock {
        void* handle = dlopen("libloc_api_v02.so", RTLD_NOW);
        return handle == nullptr ? nullptr : reinterpret_cast<SetGpsLock>(
                dlsym(handle, "_ZN9LocApiV0210setGpsLockE17GnssConfigGpsLock"));
    }();

    return setter == nullptr ? 1 : setter(self, lock);
}

extern "C" bool sourceEventConnectionStatus(void* self, bool connected, int8_t type)
{
    using EventCallback = bool (*)(void*, bool, uint8_t);
    static auto callback = reinterpret_cast<EventCallback>(dlsym(
            RTLD_DEFAULT, "_ZN8loc_core12SystemStatus21eventConnectionStatusEbh"));
    return callback && callback(self, connected, static_cast<uint8_t>(type));
}

extern "C" void sourceLocAdapterBaseCtor(void* self, uint64_t mask, void* context, void* proxy)
{
    using Constructor = void (*)(void*, uint32_t, void*, void*);
    static auto constructor = reinterpret_cast<Constructor>(dlsym(
            RTLD_DEFAULT, "_ZN8loc_core14LocAdapterBaseC1EjPNS_11ContextBaseEPNS_19LocAdapterProxyBaseE"));
    if (constructor) {
        constructor(self, static_cast<uint32_t>(mask), context, proxy);
    }
}

extern "C" bool sourceSetDefaultGnssEngineStates(void* /*self*/)
{
    return true;
}
