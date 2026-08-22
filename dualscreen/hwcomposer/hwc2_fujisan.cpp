/*
 * Fujisan HWC2 wrapper (Composer 2.4)
 *  One logical INTERNAL display exposes three physical topologies:
 *    A: 1080x1920 panel A, B: 1080x1920 panel B, C: 2160x1915 A+B.
 *  Every frame remains client-composed.  The Fujisan MDSS atomic extension
 *  owns pipe selection, the five-row B offset, and paired fence lifetime.
 */
#define LOG_TAG "HwcFujisan"

#include <cutils/native_handle.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer2.h>
#include <system/graphics.h>
#include <cutils/properties.h>
#include <hardware/gralloc.h>

/* From CAF gralloc_priv.h / gr_priv_handle.h (msm8996). */
#ifndef GRALLOC_MODULE_PERFORM_GET_CUSTOM_STRIDE_FROM_HANDLE
#define GRALLOC_MODULE_PERFORM_GET_CUSTOM_STRIDE_FROM_HANDLE 3
#endif
#ifndef PRIV_FLAGS_UBWC_ALIGNED
#define PRIV_FLAGS_UBWC_ALIGNED 0x08000000
#endif

struct FujisanPrivateHandle {
    native_handle_t base;
    int fd;
    int fd_metadata;
    int magic;
    int flags;
    int width;              /* aligned stride in pixels */
    int height;             /* aligned height */
    int unaligned_width;    /* client width */
    int unaligned_height;   /* client height */
    int format;
    int buffer_type;
    unsigned int size;
    unsigned int offset;
    unsigned int offset_metadata;
    uint64_t base_addr;
    uint64_t base_metadata;
    uint64_t gpuaddr;
    uint64_t id;
    uint64_t producer_usage;
    uint64_t consumer_usage;
    unsigned int layer_count;
};
static constexpr int kFujisanGrallocMagic = 'gmsm';
#include <log/log.h>
#include <sync/sync.h>

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/msm_mdp.h>
#include <linux/msm_mdp_ext.h>
#include <pthread.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/system_properties.h>
#include <time.h>
#include <unistd.h>

#include <atomic>
#include <algorithm>
#include <cstdint>
#include <map>
#include <mutex>
#include <vector>

/* The Android generated kernel headers are refreshed independently from the
 * boot image.  Keep the userspace value in lockstep with the committed UAPI
 * while those headers still predate the Fujisan extension. */
#ifndef MDP_COMMIT_FUJISAN_WIDE
#define MDP_COMMIT_FUJISAN_WIDE 0x40000000
#endif
#ifndef MDP_COMMIT_FUJISAN_SINGLE
#define MDP_COMMIT_FUJISAN_SINGLE 0x20000000
#endif
#ifndef MDP_COMMIT_FUJISAN_SINGLE_B
#define MDP_COMMIT_FUJISAN_SINGLE_B 0x10000000
#endif

#ifndef FUJISAN_SEC_WIDTH
#define FUJISAN_SEC_WIDTH 1080
#endif
#ifndef FUJISAN_SEC_HEIGHT
#define FUJISAN_SEC_HEIGHT 1920
#endif
#ifndef FUJISAN_SEC_DPI_X
#define FUJISAN_SEC_DPI_X 428625
#endif
#ifndef FUJISAN_SEC_DPI_Y
#define FUJISAN_SEC_DPI_Y 427789
#endif
#ifndef FUJISAN_SEC_VSYNC_NS
#define FUJISAN_SEC_VSYNC_NS 16666667
#endif

namespace {

/*
 * Two stable framework-facing internal displays.  Small is always the first
 * hotplugged display so boot animation retains its 1080x1920 contract.  Wide
 * is an independent HWC endpoint backed by the same fb0 atomic interface;
 * DeviceState display layouts make the endpoints mutually exclusive.
 */
constexpr hwc2_display_t kSmallDisplay = 0;
constexpr hwc2_display_t kWideDisplay = 0xF001ULL;
constexpr hwc2_display_t kPrimaryDisplay = kSmallDisplay;  // CAF owns fb0 as display 0.
constexpr hwc2_config_t kPanelAConfig = 0;
constexpr hwc2_config_t kWideConfig = 1;
constexpr int kZoomWidth = 2160;
constexpr int kZoomHeight = 1915;
/* A leaves its bottom five rows unscanned; B starts five rows down because
 * its physical panel is five rows higher than A. */
constexpr char kFb0Path[] = "/dev/graphics/fb0";

struct RealFns {
    HWC2_PFN_ACCEPT_DISPLAY_CHANGES acceptDisplayChanges = nullptr;
    HWC2_PFN_CREATE_LAYER createLayer = nullptr;
    HWC2_PFN_CREATE_VIRTUAL_DISPLAY createVirtualDisplay = nullptr;
    HWC2_PFN_DESTROY_LAYER destroyLayer = nullptr;
    HWC2_PFN_DESTROY_VIRTUAL_DISPLAY destroyVirtualDisplay = nullptr;
    HWC2_PFN_DUMP dump = nullptr;
    HWC2_PFN_GET_ACTIVE_CONFIG getActiveConfig = nullptr;
    HWC2_PFN_GET_CHANGED_COMPOSITION_TYPES getChangedCompositionTypes = nullptr;
    HWC2_PFN_GET_CLIENT_TARGET_SUPPORT getClientTargetSupport = nullptr;
    HWC2_PFN_GET_COLOR_MODES getColorModes = nullptr;
    HWC2_PFN_GET_DISPLAY_ATTRIBUTE getDisplayAttribute = nullptr;
    HWC2_PFN_GET_DISPLAY_CONFIGS getDisplayConfigs = nullptr;
    HWC2_PFN_GET_DISPLAY_NAME getDisplayName = nullptr;
    HWC2_PFN_GET_DISPLAY_REQUESTS getDisplayRequests = nullptr;
    HWC2_PFN_GET_DISPLAY_TYPE getDisplayType = nullptr;
    HWC2_PFN_GET_DOZE_SUPPORT getDozeSupport = nullptr;
    HWC2_PFN_GET_HDR_CAPABILITIES getHdrCapabilities = nullptr;
    HWC2_PFN_GET_MAX_VIRTUAL_DISPLAY_COUNT getMaxVirtualDisplayCount = nullptr;
    HWC2_PFN_GET_RELEASE_FENCES getReleaseFences = nullptr;
    HWC2_PFN_PRESENT_DISPLAY presentDisplay = nullptr;
    HWC2_PFN_REGISTER_CALLBACK registerCallback = nullptr;
    HWC2_PFN_SET_ACTIVE_CONFIG setActiveConfig = nullptr;
    HWC2_PFN_SET_CLIENT_TARGET setClientTarget = nullptr;
    HWC2_PFN_SET_COLOR_MODE setColorMode = nullptr;
    HWC2_PFN_SET_COLOR_TRANSFORM setColorTransform = nullptr;
    HWC2_PFN_SET_CURSOR_POSITION setCursorPosition = nullptr;
    HWC2_PFN_SET_LAYER_BLEND_MODE setLayerBlendMode = nullptr;
    HWC2_PFN_SET_LAYER_BUFFER setLayerBuffer = nullptr;
    HWC2_PFN_SET_LAYER_COLOR setLayerColor = nullptr;
    HWC2_PFN_SET_LAYER_COMPOSITION_TYPE setLayerCompositionType = nullptr;
    HWC2_PFN_SET_LAYER_DATASPACE setLayerDataspace = nullptr;
    HWC2_PFN_SET_LAYER_DISPLAY_FRAME setLayerDisplayFrame = nullptr;
    HWC2_PFN_SET_LAYER_PLANE_ALPHA setLayerPlaneAlpha = nullptr;
    HWC2_PFN_SET_LAYER_SIDEBAND_STREAM setLayerSidebandStream = nullptr;
    HWC2_PFN_SET_LAYER_SOURCE_CROP setLayerSourceCrop = nullptr;
    HWC2_PFN_SET_LAYER_SURFACE_DAMAGE setLayerSurfaceDamage = nullptr;
    HWC2_PFN_SET_LAYER_TRANSFORM setLayerTransform = nullptr;
    HWC2_PFN_SET_LAYER_VISIBLE_REGION setLayerVisibleRegion = nullptr;
    HWC2_PFN_SET_LAYER_Z_ORDER setLayerZOrder = nullptr;
    HWC2_PFN_SET_OUTPUT_BUFFER setOutputBuffer = nullptr;
    HWC2_PFN_SET_POWER_MODE setPowerMode = nullptr;
    HWC2_PFN_SET_VSYNC_ENABLED setVsyncEnabled = nullptr;
    HWC2_PFN_VALIDATE_DISPLAY validateDisplay = nullptr;
    /* Composer 2.3/2.4 (optional on stock msm8996). */
    HWC2_PFN_GET_DISPLAY_CAPABILITIES getDisplayCapabilities = nullptr;
    HWC2_PFN_SET_DISPLAY_BRIGHTNESS setDisplayBrightness = nullptr;
    HWC2_PFN_GET_DISPLAY_CONNECTION_TYPE getDisplayConnectionType = nullptr;
    HWC2_PFN_GET_DISPLAY_VSYNC_PERIOD getDisplayVsyncPeriod = nullptr;
    HWC2_PFN_SET_ACTIVE_CONFIG_WITH_CONSTRAINTS setActiveConfigWithConstraints = nullptr;
};

struct ZoomLayer {
    /* HWC2 starts a new layer as DEVICE.  Validation must explicitly return
     * CLIENT before SurfaceFlinger renders the sole primary client target. */
    int32_t requested = HWC2_COMPOSITION_DEVICE;
    int32_t validated = HWC2_COMPOSITION_CLIENT;
    bool changed = false;
    buffer_handle_t buffer = nullptr;
    int acquire_fence = -1;
    hwc_rect_t frame{};
    hwc_frect_t crop{};
    int32_t transform = 0;
    int32_t blend = HWC2_BLEND_MODE_NONE;
    float alpha = 1.0f;
    int32_t dataspace = HAL_DATASPACE_UNKNOWN;
    uint32_t z = 0;
    hwc_color_t color{};
    bool has_color = false;
    const native_handle_t* sideband = nullptr;
    bool has_sideband = false;
    bool device_candidate = false;
    hwc2_display_t owner = kSmallDisplay;
};

struct EndpointState {
    buffer_handle_t client_target = nullptr;
    int32_t client_acquire_fence = -1;
    hwc2_config_t active_config = kPanelAConfig;
};

struct Device {
    hwc2_device_t base{};
    hwc2_device_t* real = nullptr;
    void* real_so = nullptr;
    RealFns fns{};

    hwc2_callback_data_t hotplug_data = nullptr;
    HWC2_PFN_HOTPLUG hotplug_fn = nullptr;
    hwc2_callback_data_t vsync_data = nullptr;
    HWC2_PFN_VSYNC vsync_fn = nullptr;
    hwc2_callback_data_t vsync24_data = nullptr;
    HWC2_PFN_VSYNC_2_4 vsync24_fn = nullptr;
    hwc2_callback_data_t refresh_data = nullptr;
    HWC2_PFN_REFRESH refresh_fn = nullptr;

    std::mutex cb_lock;
    std::atomic<bool> physical_displays_announced{false};
    std::atomic<bool> small_vsync_enabled{false};
    std::atomic<bool> wide_vsync_enabled{false};
    std::atomic<bool> small_power_on{true};
    std::atomic<bool> wide_power_on{false};
    /* Last non-zero framework brightness for each logical endpoint, in the
     * legacy 0..255 LED scale.  A/B share one physical backlight path. */
    std::atomic<int> small_brightness{-1};
    std::atomic<int> wide_brightness{-1};
    std::atomic<bool> shared_power_off_thread_run{false};
    std::atomic<uint64_t> shared_power_generation{0};
    pthread_t shared_power_off_thread{};
    pthread_t primary_panel_thread{};
    std::atomic<bool> primary_panel_thread_run{false};

    /* The DeviceState layout keeps these endpoints mutually exclusive, but
     * transitions can overlap one frame.  Keep client targets per endpoint. */
    std::mutex zoom_lock;
    EndpointState small;
    EndpointState wide;
    /* Wide is submitted through the standard atomic ABI on fb0.  Do not use
     * the legacy fb1 overlay route: the Fujisan kernel expands this one C
     * target into both physical CTLs. */
    int wide_fd = -1;
    bool wide_route_active = false;
    /* SurfaceFlinger owns each returned fence.  Retain a duplicate of the
     * newest C completion fence so WIDE -> SINGLE can drain both CTLs before
     * the wrapped CAF composer receives another primary frame. */
    int wide_drain_fence = -1;
    uint32_t wide_submit_count = 0;
    uint32_t single_submit_count = 0;

    std::map<hwc2_layer_t, ZoomLayer> zoom_layers;
    hwc2_layer_t next_primary_layer = 1;
    bool zoom_layers_validated = false;
    bool zoom_force_client = false;

    /* Bring-up telemetry only.  The client target's allocation geometry is
     * the authoritative indication of whether SurfaceFlinger has rotated the
     * virtual wide framebuffer; HWC2 does not pass a display rotation to
     * setClientTarget(). */
    int zoom_target_width = 0;
    int zoom_target_height = 0;
    int zoom_target_stride = 0;
    int64_t zoom_present_window_ns = 0;
    uint32_t zoom_present_count = 0;

};

static bool IsBootCompleted() {
    char boot_completed[PROPERTY_VALUE_MAX] = {};
    property_get("sys.boot_completed", boot_completed, "0");
    return boot_completed[0] == '1';
}

static bool WantSingleBPanel() {
    if (!IsBootCompleted()) {
        /* BootAnimation must always use physical A.  The user's folded A/B
         * preference is applied only after boot completion, when halld is
         * restarted and publishes active_primary. */
        return false;
    }
    char primary[PROPERTY_VALUE_MAX] = {};
    property_get("vendor.fujisan.active_primary", primary, "a");
    return primary[0] == 'b';
}

static bool IsFujisanDisplay(hwc2_display_t display) {
    return display == kSmallDisplay || display == kWideDisplay;
}

static bool IsWideDisplay(hwc2_display_t display) {
    return display == kWideDisplay;
}

/* The inherited CAF composer understands only fb0/display 0.  Wide is a
 * wrapper-owned endpoint whose present path chooses MDP_COMMIT_FUJISAN_WIDE. */
static hwc2_display_t RealDisplayFor(hwc2_display_t display) {
    return IsFujisanDisplay(display) ? kPrimaryDisplay : display;
}

static hwc2_config_t ConfigForDisplay(hwc2_display_t display) {
    return IsWideDisplay(display) ? kWideConfig : kPanelAConfig;
}

static bool IsConfigForDisplay(hwc2_display_t display, hwc2_config_t config) {
    return config == ConfigForDisplay(display);
}

static EndpointState& EndpointFor(Device* d, hwc2_display_t display) {
    return IsWideDisplay(display) ? d->wide : d->small;
}

static int64_t MonotonicNs() {
    struct timespec ts {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
}

static void NoteZoomPresent(Device* d, int width, int height, int stride) {
    const int64_t now = MonotonicNs();
    if (d->zoom_present_window_ns == 0)
        d->zoom_present_window_ns = now;
    ++d->zoom_present_count;
    const int64_t elapsed = now - d->zoom_present_window_ns;
    if (elapsed < 1000000000LL)
        return;
    const float fps = static_cast<float>(d->zoom_present_count) * 1000000000.0f /
                      static_cast<float>(elapsed);
    ALOGI("zoom target=%dx%d stride=%d present=%.1ffps", width, height, stride, fps);
    d->zoom_present_window_ns = now;
    d->zoom_present_count = 0;
}

static Device* ToDev(hwc2_device_t* d) {
    return reinterpret_cast<Device*>(d);
}

static bool OpenWideFramebuffer(Device* d) {
    if (d->wide_fd >= 0)
        return true;
    d->wide_fd = open(kFb0Path, O_RDWR | O_CLOEXEC);
    if (d->wide_fd < 0) {
        ALOGE("open %s for wide atomic submit failed: %s", kFb0Path, strerror(errno));
        return false;
    }
    return true;
}

static void CloseWideFramebuffer(Device* d) {
    int stale_drain_fence = -1;
    {
        std::lock_guard<std::mutex> zl(d->zoom_lock);
        stale_drain_fence = d->wide_drain_fence;
        d->wide_drain_fence = -1;
        d->wide_route_active = false;
        d->wide_submit_count = 0;
    }
    if (stale_drain_fence >= 0)
        close(stale_drain_fence);
    if (d->wide_fd >= 0) {
        close(d->wide_fd);
        d->wide_fd = -1;
    }
}

/* The C release fence is signaled only after the second native command-mode
 * pingpong completes.  CAF must not reclaim fb0 until that transaction ends. */
static bool DrainWideRoute(Device* d, const char* reason) {
    if (!d)
        return false;

    {
        std::lock_guard<std::mutex> zl(d->zoom_lock);
        if (!d->wide_route_active)
            return true;
        if (d->wide_drain_fence < 0) {
            ALOGE("wide drain (%s) has no merged completion fence", reason);
            return false;
        }
        if (sync_wait(d->wide_drain_fence, 1000) != 0) {
            ALOGE("wide drain (%s) timed out: %s", reason, strerror(errno));
            return false;
        }
        close(d->wide_drain_fence);
        d->wide_drain_fence = -1;
        d->wide_route_active = false;
    }

    ALOGI("wide drain complete (%s)", reason);
    CloseWideFramebuffer(d);
    return true;
}

static hwc2_vsync_period_t PrimaryVsyncPeriodNs(Device* d, hwc2_display_t display) {
    hwc2_config_t cfg = 0;
    if (d->fns.getActiveConfig)
        d->fns.getActiveConfig(d->real, RealDisplayFor(display), &cfg);
    int32_t period = 0;
    if (d->fns.getDisplayAttribute) {
        d->fns.getDisplayAttribute(d->real, RealDisplayFor(display), cfg,
                                    HWC2_ATTRIBUTE_VSYNC_PERIOD, &period);
    }
    if (period <= 0)
        period = FUJISAN_SEC_VSYNC_NS;
    return static_cast<hwc2_vsync_period_t>(period);
}

/* CAF emits only fb0's legacy VSYNC.  Fan it out to whichever stable
 * endpoint DeviceState has enabled; layouts keep Small and Wide exclusive. */
static void PrimaryVsyncTrampoline(hwc2_callback_data_t cb_data, hwc2_display_t /*display*/,
                                   int64_t timestamp) {
    auto* dev = reinterpret_cast<Device*>(cb_data);
    HWC2_PFN_VSYNC_2_4 fn24 = nullptr;
    HWC2_PFN_VSYNC fn = nullptr;
    hwc2_callback_data_t data24 = nullptr;
    hwc2_callback_data_t data = nullptr;
    {
        std::lock_guard<std::mutex> cl(dev->cb_lock);
        fn24 = dev->vsync24_fn;
        data24 = dev->vsync24_data;
        fn = dev->vsync_fn;
        data = dev->vsync_data;
    }
    const auto send = [&](hwc2_display_t target) {
        if (fn24)
            fn24(data24, target, timestamp, PrimaryVsyncPeriodNs(dev, target));
        if (fn)
            fn(data, target, timestamp);
    };
    if (dev->small_vsync_enabled.load())
        send(kSmallDisplay);
    if (dev->wide_vsync_enabled.load())
        send(kWideDisplay);
}

static int32_t WirePrimaryVsync(Device* d) {
    if (!d->fns.registerCallback)
        return HWC2_ERROR_UNSUPPORTED;
    return d->fns.registerCallback(d->real, HWC2_CALLBACK_VSYNC, d,
                                   reinterpret_cast<hwc2_function_pointer_t>(PrimaryVsyncTrampoline));
}

static hwc2_function_pointer_t RealGet(hwc2_device_t* real, int32_t desc) {
    return real->getFunction(real, desc);
}

static void LoadRealFns(Device* d) {
    auto g = [&](int32_t desc) { return RealGet(d->real, desc); };
#define LOAD(name, DESC) d->fns.name = reinterpret_cast<HWC2_PFN_##DESC>(g(HWC2_FUNCTION_##DESC))
    LOAD(acceptDisplayChanges, ACCEPT_DISPLAY_CHANGES);
    LOAD(createLayer, CREATE_LAYER);
    LOAD(createVirtualDisplay, CREATE_VIRTUAL_DISPLAY);
    LOAD(destroyLayer, DESTROY_LAYER);
    LOAD(destroyVirtualDisplay, DESTROY_VIRTUAL_DISPLAY);
    LOAD(dump, DUMP);
    LOAD(getActiveConfig, GET_ACTIVE_CONFIG);
    LOAD(getChangedCompositionTypes, GET_CHANGED_COMPOSITION_TYPES);
    LOAD(getClientTargetSupport, GET_CLIENT_TARGET_SUPPORT);
    LOAD(getColorModes, GET_COLOR_MODES);
    LOAD(getDisplayAttribute, GET_DISPLAY_ATTRIBUTE);
    LOAD(getDisplayConfigs, GET_DISPLAY_CONFIGS);
    LOAD(getDisplayName, GET_DISPLAY_NAME);
    LOAD(getDisplayRequests, GET_DISPLAY_REQUESTS);
    LOAD(getDisplayType, GET_DISPLAY_TYPE);
    LOAD(getDozeSupport, GET_DOZE_SUPPORT);
    LOAD(getHdrCapabilities, GET_HDR_CAPABILITIES);
    LOAD(getMaxVirtualDisplayCount, GET_MAX_VIRTUAL_DISPLAY_COUNT);
    LOAD(getReleaseFences, GET_RELEASE_FENCES);
    LOAD(presentDisplay, PRESENT_DISPLAY);
    LOAD(registerCallback, REGISTER_CALLBACK);
    LOAD(setActiveConfig, SET_ACTIVE_CONFIG);
    LOAD(setClientTarget, SET_CLIENT_TARGET);
    LOAD(setColorMode, SET_COLOR_MODE);
    LOAD(setColorTransform, SET_COLOR_TRANSFORM);
    LOAD(setCursorPosition, SET_CURSOR_POSITION);
    LOAD(setLayerBlendMode, SET_LAYER_BLEND_MODE);
    LOAD(setLayerBuffer, SET_LAYER_BUFFER);
    LOAD(setLayerColor, SET_LAYER_COLOR);
    LOAD(setLayerCompositionType, SET_LAYER_COMPOSITION_TYPE);
    LOAD(setLayerDataspace, SET_LAYER_DATASPACE);
    LOAD(setLayerDisplayFrame, SET_LAYER_DISPLAY_FRAME);
    LOAD(setLayerPlaneAlpha, SET_LAYER_PLANE_ALPHA);
    LOAD(setLayerSidebandStream, SET_LAYER_SIDEBAND_STREAM);
    LOAD(setLayerSourceCrop, SET_LAYER_SOURCE_CROP);
    LOAD(setLayerSurfaceDamage, SET_LAYER_SURFACE_DAMAGE);
    LOAD(setLayerTransform, SET_LAYER_TRANSFORM);
    LOAD(setLayerVisibleRegion, SET_LAYER_VISIBLE_REGION);
    LOAD(setLayerZOrder, SET_LAYER_Z_ORDER);
    LOAD(setOutputBuffer, SET_OUTPUT_BUFFER);
    LOAD(setPowerMode, SET_POWER_MODE);
    LOAD(setVsyncEnabled, SET_VSYNC_ENABLED);
    LOAD(validateDisplay, VALIDATE_DISPLAY);
    LOAD(getDisplayCapabilities, GET_DISPLAY_CAPABILITIES);
    LOAD(setDisplayBrightness, SET_DISPLAY_BRIGHTNESS);
    LOAD(getDisplayConnectionType, GET_DISPLAY_CONNECTION_TYPE);
    LOAD(getDisplayVsyncPeriod, GET_DISPLAY_VSYNC_PERIOD);
    LOAD(setActiveConfigWithConstraints, SET_ACTIVE_CONFIG_WITH_CONSTRAINTS);
#undef LOAD
}

static bool GetGrallocStridePx(buffer_handle_t handle, int* out_stride_px, int* out_w, int* out_h,
                              int* out_format, int* out_flags) {
    if (!handle || !out_stride_px)
        return false;
    *out_stride_px = FUJISAN_SEC_WIDTH;
    if (out_w)
        *out_w = FUJISAN_SEC_WIDTH;
    if (out_h)
        *out_h = FUJISAN_SEC_HEIGHT;
    if (out_format)
        *out_format = HAL_PIXEL_FORMAT_RGBA_8888;
    if (out_flags)
        *out_flags = 0;

    const auto* hnd = reinterpret_cast<const FujisanPrivateHandle*>(handle);
    if (hnd && hnd->magic == kFujisanGrallocMagic) {
        if (out_w && hnd->unaligned_width > 0)
            *out_w = hnd->unaligned_width;
        if (out_h && hnd->unaligned_height > 0)
            *out_h = hnd->unaligned_height;
        if (hnd->width > 0)
            *out_stride_px = hnd->width; /* CAF: width == aligned stride */
        if (out_format)
            *out_format = hnd->format;
        if (out_flags)
            *out_flags = hnd->flags;
        return true;
    }

    /* Fallback: gralloc perform */
    const hw_module_t* module = nullptr;
    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module) == 0 && module) {
        const auto* g = reinterpret_cast<const gralloc_module_t*>(module);
        if (g->perform) {
            int stride = 0;
            if (g->perform(const_cast<gralloc_module_t*>(g),
                           GRALLOC_MODULE_PERFORM_GET_CUSTOM_STRIDE_FROM_HANDLE, handle,
                           &stride) == 0 &&
                stride > 0) {
                *out_stride_px = stride;
                return true;
            }
        }
    }
    return false;
}

/*
 * Temporary wide-primary bridge.  SurfaceFlinger has already composed the
 * whole 2160x1915 scene into one linear client target.  Submit that target to
 * fb0 with the normal MDSS atomic ABI; MDP_COMMIT_FUJISAN_WIDE is consumed in
 * the kernel and expanded into the A/B CTL transaction.  There is deliberately
 * no fb1 open, overlay, copy, or fence wait in this route.
 */
static bool SubmitWideClientTarget(Device* d, buffer_handle_t handle, int acquire_fence,
                                   int32_t* out_retire_fence) {
    if (out_retire_fence)
        *out_retire_fence = -1;

    const auto close_acquire = [&]() {
        if (acquire_fence >= 0) {
            close(acquire_fence);
            acquire_fence = -1;
        }
    };

    if (!d || !handle) {
        ALOGE("wide atomic submit has no client target");
        close_acquire();
        return false;
    }

    const auto* gralloc = reinterpret_cast<const FujisanPrivateHandle*>(handle);
    int stride_px = 0;
    int width = 0;
    int height = 0;
    int format = 0;
    int flags = 0;
    if (gralloc->magic != kFujisanGrallocMagic || gralloc->fd < 0 ||
        !GetGrallocStridePx(handle, &stride_px, &width, &height, &format, &flags)) {
        ALOGE("wide atomic submit received an invalid gralloc handle");
        close_acquire();
        return false;
    }

    const bool rgba = format == HAL_PIXEL_FORMAT_RGBA_8888;
    const bool rgbx = format == HAL_PIXEL_FORMAT_RGBX_8888;
    const bool linear = (flags & PRIV_FLAGS_UBWC_ALIGNED) == 0;
    if (!linear || (!rgba && !rgbx) || width != kZoomWidth ||
        height != kZoomHeight || stride_px < kZoomWidth) {
        ALOGE("wide atomic contract mismatch: %dx%d stride=%d format=%d flags=0x%x",
              width, height, stride_px, format, flags);
        close_acquire();
        return false;
    }
    if (gralloc->size < static_cast<unsigned int>(stride_px * height * 4)) {
        ALOGE("wide atomic target too small: size=%u need=%zu", gralloc->size,
              static_cast<size_t>(stride_px) * height * 4);
        close_acquire();
        return false;
    }
    if (!OpenWideFramebuffer(d)) {
        close_acquire();
        return false;
    }

    mdp_input_layer layer {};
    layer.alpha = 0xff;
    layer.transp_mask = MDP_TRANSP_NOP;
    layer.blend_op = BLEND_OP_OPAQUE;
    layer.src_rect = {0, 0, kZoomWidth, kZoomHeight};
    layer.dst_rect = {0, 0, kZoomWidth, kZoomHeight};
    layer.buffer.width = static_cast<uint32_t>(stride_px);
    layer.buffer.height = static_cast<uint32_t>(height);
    layer.buffer.format = rgba ? MDP_RGBA_8888 : MDP_RGBX_8888;
    layer.buffer.planes[0].fd = gralloc->fd;
    layer.buffer.planes[0].offset = gralloc->offset;
    layer.buffer.planes[0].stride = static_cast<uint32_t>(stride_px * 4);
    layer.buffer.plane_count = 1;
    layer.buffer.comp_ratio.numer = 1000;
    layer.buffer.comp_ratio.denom = 1000;
    layer.buffer.fence = acquire_fence;

    mdp_layer_commit commit {};
    commit.version = MDP_COMMIT_VERSION_1_0;
    commit.commit_v1.flags = MDP_COMMIT_FUJISAN_WIDE;
    commit.commit_v1.release_fence = -1;
    commit.commit_v1.retire_fence = -1;
    commit.commit_v1.input_layers = &layer;
    commit.commit_v1.input_layer_cnt = 1;

    const int ret = ioctl(d->wide_fd, MSMFB_ATOMIC_COMMIT, &commit);
    close_acquire();
    if (ret != 0) {
        ALOGE("wide atomic submit failed: %s (layer=%d, release=%d, retire=%d)",
              strerror(errno), layer.error_code, commit.commit_v1.release_fence,
              commit.commit_v1.retire_fence);
        if (commit.commit_v1.release_fence >= 0)
            close(commit.commit_v1.release_fence);
        if (commit.commit_v1.retire_fence >= 0)
            close(commit.commit_v1.retire_fence);
        return false;
    }

    /* HWC2's present result is the display retire fence.  The atomic release
     * fence remains an internal A+B completion fence used to drain this
     * route, but must not be exposed as out_retire_fence: it advances only on
     * the second command pingpong and throttles SurfaceFlinger to ~30 Hz. */
    const int merged_fence = commit.commit_v1.release_fence;
    const int retire_fence = commit.commit_v1.retire_fence;
    if (merged_fence < 0 || retire_fence < 0) {
        ALOGE("wide atomic submit returned incomplete fences: release=%d retire=%d",
              merged_fence, retire_fence);
        if (merged_fence >= 0)
            close(merged_fence);
        if (retire_fence >= 0)
            close(retire_fence);
        return false;
    }
    const int drain_fence = dup(merged_fence);
    if (drain_fence < 0) {
        ALOGE("wide atomic submit could not retain merged fence: %s", strerror(errno));
    } else {
        std::lock_guard<std::mutex> zl(d->zoom_lock);
        /* MDP completion fences are timeline ordered, so the newest one
         * drains every earlier C transaction as well. */
        if (d->wide_drain_fence >= 0)
            close(d->wide_drain_fence);
        d->wide_drain_fence = drain_fence;
        d->wide_route_active = true;
    }
    close(merged_fence);
    if (out_retire_fence)
        *out_retire_fence = retire_fence;
    else
        close(retire_fence);

    ++d->wide_submit_count;
    if (d->wide_submit_count <= 5 || (d->wide_submit_count % 120) == 0) {
        ALOGI("wide atomic submit #%u: %dx%d stride=%d format=%d acquire=%d merged=%d",
              d->wide_submit_count, width, height, stride_px, format, layer.buffer.fence,
              retire_fence);
    }
    return true;
}

/* fb0 remains a native dual-CTL endpoint even while Android is in config 0.
 * CAF consequently expands a 1080 client buffer to a 2160 source rect and
 * MDSS rejects it.  Submit the complete 1080 client target through the
 * selected single-panel backend; panel power is coordinated by fujisan_halld. */
static bool SubmitSingleClientTarget(Device* d, buffer_handle_t handle, int acquire_fence,
                                     int32_t* out_retire_fence) {
    if (out_retire_fence)
        *out_retire_fence = -1;

    const auto close_acquire = [&]() {
        if (acquire_fence >= 0) {
            close(acquire_fence);
            acquire_fence = -1;
        }
    };

    if (!d || !handle) {
        ALOGE("single atomic submit has no client target");
        close_acquire();
        return false;
    }

    const auto* gralloc = reinterpret_cast<const FujisanPrivateHandle*>(handle);
    int stride_px = 0;
    int width = 0;
    int height = 0;
    int format = 0;
    int flags = 0;
    if (gralloc->magic != kFujisanGrallocMagic || gralloc->fd < 0 ||
        !GetGrallocStridePx(handle, &stride_px, &width, &height, &format, &flags)) {
        ALOGE("single atomic submit received an invalid gralloc handle");
        close_acquire();
        return false;
    }

    const bool rgba = format == HAL_PIXEL_FORMAT_RGBA_8888;
    const bool rgbx = format == HAL_PIXEL_FORMAT_RGBX_8888;
    const bool linear = (flags & PRIV_FLAGS_UBWC_ALIGNED) == 0;
    if (!linear || (!rgba && !rgbx) || width != FUJISAN_SEC_WIDTH ||
        height != FUJISAN_SEC_HEIGHT || stride_px < FUJISAN_SEC_WIDTH ||
        gralloc->size < static_cast<unsigned int>(stride_px * height * 4)) {
        ALOGE("single atomic contract mismatch: %dx%d stride=%d format=%d flags=0x%x size=%u",
              width, height, stride_px, format, flags, gralloc->size);
        close_acquire();
        return false;
    }
    if (!OpenWideFramebuffer(d)) {
        close_acquire();
        return false;
    }

    mdp_input_layer layer {};
    layer.alpha = 0xff;
    layer.transp_mask = MDP_TRANSP_NOP;
    layer.blend_op = BLEND_OP_OPAQUE;
    /* The kernel expands this client target into the native A+B command
     * transaction.  It pins A to VIG0 and retains a paired dark B layer so
     * one logical single frame still arms both CTLs. */
    layer.pipe_ndx = 1U;
    layer.src_rect = {0, 0, FUJISAN_SEC_WIDTH, FUJISAN_SEC_HEIGHT};
    layer.dst_rect = {0, 0, FUJISAN_SEC_WIDTH, FUJISAN_SEC_HEIGHT};
    layer.buffer.width = static_cast<uint32_t>(stride_px);
    layer.buffer.height = static_cast<uint32_t>(height);
    layer.buffer.format = rgba ? MDP_RGBA_8888 : MDP_RGBX_8888;
    layer.buffer.planes[0].fd = gralloc->fd;
    layer.buffer.planes[0].offset = gralloc->offset;
    layer.buffer.planes[0].stride = static_cast<uint32_t>(stride_px * 4);
    layer.buffer.plane_count = 1;
    layer.buffer.comp_ratio.numer = 1000;
    layer.buffer.comp_ratio.denom = 1000;
    layer.buffer.fence = acquire_fence;

    mdp_layer_commit commit {};
    commit.version = MDP_COMMIT_VERSION_1_0;
    commit.commit_v1.flags = MDP_COMMIT_FUJISAN_SINGLE |
                            (WantSingleBPanel() ? MDP_COMMIT_FUJISAN_SINGLE_B : 0);
    commit.commit_v1.release_fence = -1;
    commit.commit_v1.retire_fence = -1;
    commit.commit_v1.input_layers = &layer;
    commit.commit_v1.input_layer_cnt = 1;

    const int ret = ioctl(d->wide_fd, MSMFB_ATOMIC_COMMIT, &commit);
    close_acquire();
    if (ret != 0) {
        ALOGE("single atomic submit failed: %s (layer=%d, release=%d, retire=%d)",
              strerror(errno), layer.error_code, commit.commit_v1.release_fence,
              commit.commit_v1.retire_fence);
        if (commit.commit_v1.release_fence >= 0)
            close(commit.commit_v1.release_fence);
        if (commit.commit_v1.retire_fence >= 0)
            close(commit.commit_v1.retire_fence);
        return false;
    }

    /* This is still a paired command-mode transaction even in single mode.
     * The release fence advances after the second pingpong, so exposing it as
     * HWC's retire fence makes SurfaceFlinger wait an extra vsync and caps
     * the panel near 30 Hz.  Report the first-frame retire fence instead. */
    const int completion_fence = commit.commit_v1.release_fence;
    const int retire_fence = commit.commit_v1.retire_fence;
    if (completion_fence < 0 || retire_fence < 0) {
        ALOGE("single atomic submit returned incomplete fences: release=%d retire=%d",
              completion_fence, retire_fence);
        if (completion_fence >= 0)
            close(completion_fence);
        if (retire_fence >= 0)
            close(retire_fence);
        return false;
    }
    close(completion_fence);
    if (out_retire_fence)
        *out_retire_fence = retire_fence;
    else
        close(retire_fence);

    ++d->single_submit_count;
    if (d->single_submit_count <= 5 || (d->single_submit_count % 120) == 0) {
        ALOGI("single atomic submit #%u: %dx%d stride=%d format=%d acquire=%d retire=%d",
              d->single_submit_count, width, height, stride_px, format, layer.buffer.fence,
              retire_fence);
    }
    return true;
}


static void RequestPrimaryFrame(Device* d) {
    HWC2_PFN_REFRESH fn = nullptr;
    hwc2_callback_data_t data = nullptr;
    {
        std::lock_guard<std::mutex> cl(d->cb_lock);
        fn = d->refresh_fn;
        data = d->refresh_data;
    }
    if (fn)
        fn(data, kPrimaryDisplay);
}

struct DisplayPropertySnapshot {
    uint32_t serial = 0;
    char value[PROPERTY_VALUE_MAX] = {};
};

static void ReadDisplayProperty(void* cookie, const char*, const char* value, uint32_t serial) {
    auto* snapshot = static_cast<DisplayPropertySnapshot*>(cookie);
    snapshot->serial = serial;
    snprintf(snapshot->value, sizeof(snapshot->value), "%s", value ? value : "");
}

/* A/B has one unchanged HWC config.  It therefore needs its own property
 * wakeup rather than reusing display_mode, whose value correctly remains
 * "single" across the switch. */
static void* PrimaryPanelWatchThreadMain(void* arg) {
    auto* d = reinterpret_cast<Device*>(arg);
    const prop_info* primary_prop = __system_property_find("vendor.fujisan.active_primary");
    if (!primary_prop) {
        ALOGW("active_primary property missing; single-panel wake unavailable");
        d->primary_panel_thread_run.store(false);
        return nullptr;
    }

    DisplayPropertySnapshot snapshot;
    __system_property_read_callback(primary_prop, ReadDisplayProperty, &snapshot);
    char last_primary[sizeof(snapshot.value)];
    snprintf(last_primary, sizeof(last_primary), "%s", snapshot.value);
    while (d->primary_panel_thread_run.load()) {
        uint32_t changed_serial = snapshot.serial;
        const struct timespec timeout = {30, 0};
        if (!__system_property_wait(primary_prop, snapshot.serial, &changed_serial, &timeout))
            continue;
        __system_property_read_callback(primary_prop, ReadDisplayProperty, &snapshot);
        if (!d->primary_panel_thread_run.load())
            break;
        if (strcmp(last_primary, snapshot.value) == 0)
            continue;
        snprintf(last_primary, sizeof(last_primary), "%s", snapshot.value);
        /* A/B share geometry.  A refresh produces the next client target and
         * lets the kernel consume its A/B atomic flag; a connected callback
         * would needlessly make SurfaceFlinger rebuild the display. */
        ALOGI("active_primary -> %s; refreshing primary frame", snapshot.value);
        RequestPrimaryFrame(d);
    }
    d->primary_panel_thread_run.store(false);
    return nullptr;
}

static void EnsurePrimaryPanelWatchThread(Device* d) {
    bool expected = false;
    if (!d->primary_panel_thread_run.compare_exchange_strong(expected, true))
        return;
    if (pthread_create(&d->primary_panel_thread, nullptr, PrimaryPanelWatchThreadMain, d) != 0) {
        d->primary_panel_thread_run.store(false);
        ALOGE("active_primary watcher create failed");
    }
}

static void WrapperGetCapabilities(struct hwc2_device* device, uint32_t* out_count,
                                   int32_t* out_capabilities) {
    auto* d = ToDev(device);
    /* The CAF composer supports SKIP_VALIDATE for its native 1080-wide
     * device-composition path.  Wide C always requires a validation pass:
     * that pass changes every visible layer to CLIENT before SurfaceFlinger
     * renders the one 2160-wide target submitted below.  Advertising the
     * CAF capability lets the composer call Present directly after the first
     * frame, bypassing that change and producing an unrendered target.
     *
     * The underlying SDM also advertises SKIP_CLIENT_COLOR_TRANSFORM, but
     * rejects the corresponding setColorTransform request.  Do not expose
     * that broken capability: SurfaceFlinger will then apply Night Display
     * and other standard color transforms to the client target itself. */
    uint32_t real_count = 0;
    d->real->getCapabilities(d->real, &real_count, nullptr);
    std::vector<int32_t> real_caps(real_count);
    if (real_count)
        d->real->getCapabilities(d->real, &real_count, real_caps.data());

    std::vector<int32_t> caps;
    caps.reserve(real_count);
    for (uint32_t i = 0; i < real_count; ++i) {
        if (real_caps[i] != HWC2_CAPABILITY_SKIP_VALIDATE &&
            real_caps[i] != HWC2_CAPABILITY_SKIP_CLIENT_COLOR_TRANSFORM)
            caps.push_back(real_caps[i]);
    }
    if (!out_count)
        return;
    if (!out_capabilities) {
        *out_count = static_cast<uint32_t>(caps.size());
        return;
    }
    const uint32_t copy = std::min(*out_count, static_cast<uint32_t>(caps.size()));
    for (uint32_t i = 0; i < copy; ++i)
        out_capabilities[i] = caps[i];
    *out_count = static_cast<uint32_t>(caps.size());
}

static void AnnounceWideDisplay(Device* d) {
    if (d->physical_displays_announced.exchange(true))
        return;
    HWC2_PFN_HOTPLUG fn = nullptr;
    hwc2_callback_data_t data = nullptr;
    {
        std::lock_guard<std::mutex> cl(d->cb_lock);
        fn = d->hotplug_fn;
        data = d->hotplug_data;
    }
    if (fn) {
        ALOGI("announce stable Wide internal display");
        fn(data, kWideDisplay, HWC2_CONNECTION_CONNECTED);
    }
}

static void HotplugTrampoline(hwc2_callback_data_t cb_data, hwc2_display_t display,
                              int32_t connected) {
    auto* dev = reinterpret_cast<Device*>(cb_data);
    /* CAF's only local display becomes the stable Small endpoint.  Suppress
     * any legacy secondary event: Wide is wrapper-owned and is announced once
     * with a fixed identity. */
    if (display != kPrimaryDisplay)
        return;
    HWC2_PFN_HOTPLUG fn = nullptr;
    hwc2_callback_data_t user = nullptr;
    {
        std::lock_guard<std::mutex> cl(dev->cb_lock);
        fn = dev->hotplug_fn;
        user = dev->hotplug_data;
    }
    if (fn)
        fn(user, kSmallDisplay, connected);
    if (connected == HWC2_CONNECTION_CONNECTED) {
        AnnounceWideDisplay(dev);
    } else if (dev->physical_displays_announced.exchange(false) && fn) {
        /* This is a real CAF/service disconnect, not a hinge transition. Keep
         * framework physical-display lifecycles symmetric in that failure path. */
        fn(user, kWideDisplay, HWC2_CONNECTION_DISCONNECTED);
    }
}

static int32_t RegisterCallback(hwc2_device_t* device, int32_t descriptor,
                                hwc2_callback_data_t data, hwc2_function_pointer_t pointer) {
    auto* d = ToDev(device);
    {
        std::lock_guard<std::mutex> cl(d->cb_lock);
        switch (descriptor) {
            case HWC2_CALLBACK_HOTPLUG:
                d->hotplug_data = data;
                d->hotplug_fn = reinterpret_cast<HWC2_PFN_HOTPLUG>(pointer);
                break;
            case HWC2_CALLBACK_VSYNC:
                d->vsync_data = data;
                d->vsync_fn = reinterpret_cast<HWC2_PFN_VSYNC>(pointer);
                break;
            case HWC2_CALLBACK_VSYNC_2_4:
                d->vsync24_data = data;
                d->vsync24_fn = reinterpret_cast<HWC2_PFN_VSYNC_2_4>(pointer);
                break;
            case HWC2_CALLBACK_REFRESH:
                d->refresh_data = data;
                d->refresh_fn = reinterpret_cast<HWC2_PFN_REFRESH>(pointer);
                break;
            case HWC2_CALLBACK_VSYNC_PERIOD_TIMING_CHANGED:
            case HWC2_CALLBACK_SEAMLESS_POSSIBLE:
                return HWC2_ERROR_NONE;
            default:
                break;
        }
    }

    if (descriptor == HWC2_CALLBACK_HOTPLUG) {
        return d->fns.registerCallback
                ? d->fns.registerCallback(d->real, descriptor, d,
                        reinterpret_cast<hwc2_function_pointer_t>(HotplugTrampoline))
                : HWC2_ERROR_UNSUPPORTED;
    }
    if (descriptor == HWC2_CALLBACK_REFRESH)
        EnsurePrimaryPanelWatchThread(d);
    if (descriptor == HWC2_CALLBACK_VSYNC || descriptor == HWC2_CALLBACK_VSYNC_2_4)
        return WirePrimaryVsync(d);
    if (descriptor == HWC2_CALLBACK_REFRESH) {
        return d->fns.registerCallback
                ? d->fns.registerCallback(d->real, descriptor, data, pointer)
                : HWC2_ERROR_UNSUPPORTED;
    }
    return d->fns.registerCallback ? d->fns.registerCallback(d->real, descriptor, data, pointer)
                                   : HWC2_ERROR_UNSUPPORTED;
}

static int32_t AcceptDisplayChanges(hwc2_device_t* device, hwc2_display_t display) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display)) {
        std::lock_guard<std::mutex> zl(d->zoom_lock);
        for (auto& kv : d->zoom_layers) {
            if (kv.second.owner != display)
                continue;
            kv.second.requested = kv.second.validated;
            kv.second.changed = false;
        }
        return HWC2_ERROR_NONE;
    }
    return d->fns.acceptDisplayChanges
            ? d->fns.acceptDisplayChanges(d->real, RealDisplayFor(display))
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t CreateLayer(hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t* out) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display)) {
        if (!out)
            return HWC2_ERROR_BAD_PARAMETER;
        std::lock_guard<std::mutex> zl(d->zoom_lock);
        const hwc2_layer_t layer = d->next_primary_layer++;
        ZoomLayer state{};
        state.owner = display;
        d->zoom_layers[layer] = state;
        d->zoom_layers_validated = false;
        *out = layer;
        return HWC2_ERROR_NONE;
    }
    return d->fns.createLayer ? d->fns.createLayer(d->real, RealDisplayFor(display), out)
                               : HWC2_ERROR_UNSUPPORTED;
}

static int32_t DestroyLayer(hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display)) {
        std::lock_guard<std::mutex> zl(d->zoom_lock);
        auto it = d->zoom_layers.find(layer);
        if (it == d->zoom_layers.end() || it->second.owner != display)
            return HWC2_ERROR_BAD_LAYER;
        if (it->second.acquire_fence >= 0)
            close(it->second.acquire_fence);
        d->zoom_layers.erase(it);
        d->zoom_layers_validated = false;
        return HWC2_ERROR_NONE;
    }
    return d->fns.destroyLayer
            ? d->fns.destroyLayer(d->real, RealDisplayFor(display), layer)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t GetActiveConfig(hwc2_device_t* device, hwc2_display_t display, hwc2_config_t* out) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display)) {
        if (!out)
            return HWC2_ERROR_BAD_PARAMETER;
        std::lock_guard<std::mutex> zl(d->zoom_lock);
        EndpointFor(d, display).active_config = ConfigForDisplay(display);
        *out = ConfigForDisplay(display);
        return HWC2_ERROR_NONE;
    }
    return d->fns.getActiveConfig
            ? d->fns.getActiveConfig(d->real, RealDisplayFor(display), out)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t GetChangedCompositionTypes(hwc2_device_t* device, hwc2_display_t display,
                                          uint32_t* out_count, hwc2_layer_t* out_layers,
                                          int32_t* out_types) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display)) {
        std::lock_guard<std::mutex> zl(d->zoom_lock);
        uint32_t need = 0;
        for (const auto& kv : d->zoom_layers)
            if (kv.second.owner == display && kv.second.changed)
                ++need;
        if (!out_layers || !out_types) {
            if (out_count)
                *out_count = need;
            return HWC2_ERROR_NONE;
        }
        if (*out_count < need) {
            *out_count = need;
            return HWC2_ERROR_NONE;
        }
        uint32_t index = 0;
        for (const auto& kv : d->zoom_layers) {
            if (kv.second.owner != display || !kv.second.changed)
                continue;
            out_layers[index] = kv.first;
            out_types[index++] = kv.second.validated;
        }
        *out_count = index;
        return HWC2_ERROR_NONE;
    }
    return d->fns.getChangedCompositionTypes
            ? d->fns.getChangedCompositionTypes(d->real, RealDisplayFor(display), out_count,
                                                out_layers, out_types)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t GetClientTargetSupport(hwc2_device_t* device, hwc2_display_t display, uint32_t width,
                                      uint32_t height, int32_t format, int32_t dataspace) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display)) {
        const bool rgba = format == HAL_PIXEL_FORMAT_RGBA_8888 ||
                          format == HAL_PIXEL_FORMAT_RGBX_8888;
        const bool geometry = IsWideDisplay(display)
                ? width == kZoomWidth && height == kZoomHeight
                : width == FUJISAN_SEC_WIDTH && height == FUJISAN_SEC_HEIGHT;
        return rgba && geometry ? HWC2_ERROR_NONE : HWC2_ERROR_UNSUPPORTED;
    }
    return d->fns.getClientTargetSupport
            ? d->fns.getClientTargetSupport(d->real, RealDisplayFor(display), width, height,
                                             format, dataspace)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t GetColorModes(hwc2_device_t* device, hwc2_display_t display, uint32_t* out_count,
                             int32_t* out_modes) {
    auto* d = ToDev(device);
    return d->fns.getColorModes
            ? d->fns.getColorModes(d->real, RealDisplayFor(display), out_count, out_modes)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t GetDisplayAttribute(hwc2_device_t* device, hwc2_display_t display,
                                   hwc2_config_t config, int32_t attribute, int32_t* out) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display)) {
        if (!out || !IsConfigForDisplay(display, config))
            return HWC2_ERROR_BAD_CONFIG;
        switch (attribute) {
            case HWC2_ATTRIBUTE_WIDTH:
                *out = IsWideDisplay(display) ? kZoomWidth : FUJISAN_SEC_WIDTH;
                return HWC2_ERROR_NONE;
            case HWC2_ATTRIBUTE_HEIGHT:
                *out = IsWideDisplay(display) ? kZoomHeight : FUJISAN_SEC_HEIGHT;
                return HWC2_ERROR_NONE;
            case HWC2_ATTRIBUTE_VSYNC_PERIOD:
                *out = FUJISAN_SEC_VSYNC_NS;
                return HWC2_ERROR_NONE;
            case HWC2_ATTRIBUTE_DPI_X:
                *out = FUJISAN_SEC_DPI_X;
                return HWC2_ERROR_NONE;
            case HWC2_ATTRIBUTE_DPI_Y:
                *out = FUJISAN_SEC_DPI_Y;
                return HWC2_ERROR_NONE;
            default:
                *out = -1;
                return HWC2_ERROR_NONE;
        }
    }
    return d->fns.getDisplayAttribute
            ? d->fns.getDisplayAttribute(d->real, RealDisplayFor(display), config, attribute, out)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t GetDisplayConfigs(hwc2_device_t* device, hwc2_display_t display, uint32_t* out_count,
                                 hwc2_config_t* out_configs) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display)) {
        if (!out_count)
            return HWC2_ERROR_BAD_PARAMETER;
        if (!out_configs) {
            *out_count = 1;
            return HWC2_ERROR_NONE;
        }
        if (*out_count < 1) {
            *out_count = 1;
            return HWC2_ERROR_NONE;
        }
        out_configs[0] = ConfigForDisplay(display);
        *out_count = 1;
        return HWC2_ERROR_NONE;
    }
    return d->fns.getDisplayConfigs
            ? d->fns.getDisplayConfigs(d->real, RealDisplayFor(display), out_count, out_configs)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t GetDisplayName(hwc2_device_t* device, hwc2_display_t display, uint32_t* out_size,
                              char* out_name) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display)) {
        if (!out_size)
            return HWC2_ERROR_BAD_PARAMETER;
        const char* name = IsWideDisplay(display) ? "Fujisan Wide" : "Fujisan Small";
        const uint32_t size = static_cast<uint32_t>(strlen(name) + 1);
        if (!out_name) {
            *out_size = size;
            return HWC2_ERROR_NONE;
        }
        const uint32_t copy = std::min(*out_size, size);
        if (copy)
            memcpy(out_name, name, copy - 1);
        if (copy)
            out_name[copy - 1] = '\0';
        *out_size = size;
        return HWC2_ERROR_NONE;
    }
    return d->fns.getDisplayName
            ? d->fns.getDisplayName(d->real, RealDisplayFor(display), out_size, out_name)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t GetDisplayRequests(hwc2_device_t* device, hwc2_display_t display,
                                  int32_t* out_display_requests, uint32_t* out_num_elements,
                                  hwc2_layer_t* out_layers, int32_t* out_layer_requests) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display)) {
        if (out_display_requests)
            *out_display_requests = 0;
        if (out_num_elements)
            *out_num_elements = 0;
        return HWC2_ERROR_NONE;
    }
    return d->fns.getDisplayRequests
            ? d->fns.getDisplayRequests(d->real, RealDisplayFor(display), out_display_requests,
                                         out_num_elements, out_layers, out_layer_requests)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t GetDisplayCapabilities(hwc2_device_t* device, hwc2_display_t display,
                                      uint32_t* out_num, uint32_t* out_caps) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display)) {
        if (!out_num)
            return HWC2_ERROR_BAD_PARAMETER;
        if (!out_caps) {
            *out_num = 1;
            return HWC2_ERROR_NONE;
        }
        if (*out_num < 1) {
            *out_num = 1;
            return HWC2_ERROR_NONE;
        }
        /* Brightness must be advertised explicitly.  Otherwise Android 16
         * keeps using the disabled legacy Lights route for the wrapper-owned
         * Wide display and never invokes SetDisplayBrightness(). */
        out_caps[0] = HWC2_DISPLAY_CAPABILITY_BRIGHTNESS;
        *out_num = 1;
        return HWC2_ERROR_NONE;
    }
    if (d->fns.getDisplayCapabilities) {
        return d->fns.getDisplayCapabilities(d->real, RealDisplayFor(display), out_num, out_caps);
    }
    if (out_num)
        *out_num = 0;
    return HWC2_ERROR_NONE;
}

static int BrightnessToLevel(float brightness) {
    if (!(brightness >= 0.0f))
        return -1;
    if (brightness > 1.0f)
        brightness = 1.0f;
    return static_cast<int>(brightness * 255.0f + 0.5f);
}

static bool WritePrimaryBacklightLevel(int level) {
    if (level < 0)
        return true;
    if (level > 255)
        level = 255;
    char value[16];
    snprintf(value, sizeof(value), "%d", level);
    const int fd = open("/sys/class/leds/lcd-backlight/brightness", O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        ALOGE("open primary backlight failed: %s", strerror(errno));
        return false;
    }
    const ssize_t written = write(fd, value, strlen(value));
    close(fd);
    if (written != static_cast<ssize_t>(strlen(value))) {
        ALOGE("write primary backlight=%d failed: %s", level, strerror(errno));
        return false;
    }
    return true;
}

static std::atomic<int>& BrightnessFor(Device* d, hwc2_display_t display) {
    return IsWideDisplay(display) ? d->wide_brightness : d->small_brightness;
}

static bool IsEndpointPowered(Device* d, hwc2_display_t display) {
    return IsWideDisplay(display) ? d->wide_power_on.load() : d->small_power_on.load();
}

static int32_t SetDisplayBrightness(hwc2_device_t* device, hwc2_display_t display, float brightness) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display)) {
        const int level = BrightnessToLevel(brightness);
        if (level < 0)
            return HWC2_ERROR_BAD_PARAMETER;
        /* Android sends 0 to the outgoing logical display during a layout
         * handoff.  Do not let that inactive endpoint blank the shared A+B
         * panel; real screen-off is handled by SetPowerMode/kernel power-off. */
        if (level == 0)
            return HWC2_ERROR_NONE;
        BrightnessFor(d, display).store(level);
        if (!IsEndpointPowered(d, display))
            return HWC2_ERROR_NONE;
        return WritePrimaryBacklightLevel(level) ? HWC2_ERROR_NONE
                                                 : HWC2_ERROR_NO_RESOURCES;
    }
    return d->fns.setDisplayBrightness
            ? d->fns.setDisplayBrightness(d->real, RealDisplayFor(display), brightness)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t GetDisplayConnectionType(hwc2_device_t* device, hwc2_display_t display,
                                        uint32_t* out_type) {
    if (!out_type)
        return HWC2_ERROR_BAD_PARAMETER;
    if (IsFujisanDisplay(display)) {
        *out_type = HWC2_DISPLAY_CONNECTION_TYPE_INTERNAL;
        return HWC2_ERROR_NONE;
    }
    return HWC2_ERROR_BAD_DISPLAY;
}

static int32_t GetDisplayVsyncPeriod(hwc2_device_t* device, hwc2_display_t display,
                                     hwc2_vsync_period_t* out_period) {
    auto* d = ToDev(device);
    if (!out_period)
        return HWC2_ERROR_BAD_PARAMETER;
    *out_period = PrimaryVsyncPeriodNs(d, display);
    return HWC2_ERROR_NONE;
}

static int32_t SetActiveConfigWithConstraints(
        hwc2_device_t* device, hwc2_display_t display, hwc2_config_t config,
        hwc_vsync_period_change_constraints_t* constraints,
        hwc_vsync_period_change_timeline_t* out_timeline) {
    auto* d = ToDev(device);
    if (!out_timeline)
        return HWC2_ERROR_BAD_PARAMETER;
    if (!IsFujisanDisplay(display))
        return HWC2_ERROR_BAD_DISPLAY;
    if (!IsConfigForDisplay(display, config))
        return HWC2_ERROR_BAD_CONFIG;
    if (!IsWideDisplay(display) && !DrainWideRoute(d, "SetActiveConfigWithConstraints"))
        return HWC2_ERROR_NO_RESOURCES;
    {
        std::lock_guard<std::mutex> zl(d->zoom_lock);
        EndpointFor(d, display).active_config = ConfigForDisplay(display);
    }
    int32_t err = HWC2_ERROR_NONE;
    if (d->fns.setActiveConfigWithConstraints) {
        err = d->fns.setActiveConfigWithConstraints(d->real, kPrimaryDisplay, kPanelAConfig,
                                                     constraints, out_timeline);
    } else if (d->fns.setActiveConfig) {
        err = d->fns.setActiveConfig(d->real, kPrimaryDisplay, kPanelAConfig);
    }
    if (err != HWC2_ERROR_NONE)
        return err;
    if (!d->fns.setActiveConfigWithConstraints) {
        const int64_t now = MonotonicNs();
        int64_t desired = constraints ? constraints->desiredTimeNanos : now;
        if (desired < now)
            desired = now;
        out_timeline->newVsyncAppliedTimeNanos = desired;
        out_timeline->refreshRequired = false;
        out_timeline->refreshTimeNanos = 0;
    }
    ALOGI("SetActiveConfigWithConstraints endpoint=%s config=%llu",
          IsWideDisplay(display) ? "wide" : "small",
          static_cast<unsigned long long>(config));
    return HWC2_ERROR_NONE;
}

static int32_t GetDisplayType(hwc2_device_t* device, hwc2_display_t display, int32_t* out_type) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display)) {
        if (!out_type)
            return HWC2_ERROR_BAD_PARAMETER;
        *out_type = HWC2_DISPLAY_TYPE_PHYSICAL;
        return HWC2_ERROR_NONE;
    }
    return d->fns.getDisplayType
            ? d->fns.getDisplayType(d->real, RealDisplayFor(display), out_type)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t GetDozeSupport(hwc2_device_t* device, hwc2_display_t display, int32_t* out) {
    (void)device;
    (void)display;
    if (out)
        *out = 0;
    return HWC2_ERROR_NONE;
}

static int32_t GetHdrCapabilities(hwc2_device_t* device, hwc2_display_t display, uint32_t* out_num,
                                  int32_t* types, float* max_l, float* max_avg, float* min_l) {
    auto* d = ToDev(device);
    return d->fns.getHdrCapabilities
            ? d->fns.getHdrCapabilities(d->real, RealDisplayFor(display), out_num, types, max_l,
                                         max_avg, min_l)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t GetReleaseFences(hwc2_device_t* device, hwc2_display_t display, uint32_t* out_num,
                                hwc2_layer_t* layers, int32_t* fences) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display)) {
        if (out_num)
            *out_num = 0;
        return HWC2_ERROR_NONE;
    }
    return d->fns.getReleaseFences
            ? d->fns.getReleaseFences(d->real, RealDisplayFor(display), out_num, layers, fences)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t PresentDisplay(hwc2_device_t* device, hwc2_display_t display,
                              int32_t* out_retire_fence) {
    auto* d = ToDev(device);
    if (!IsFujisanDisplay(display)) {
        return d->fns.presentDisplay
                ? d->fns.presentDisplay(d->real, RealDisplayFor(display), out_retire_fence)
                : HWC2_ERROR_UNSUPPORTED;
    }
    buffer_handle_t target = nullptr;
    int fence = -1;
    {
        std::lock_guard<std::mutex> zl(d->zoom_lock);
        EndpointState& endpoint = EndpointFor(d, display);
        endpoint.active_config = ConfigForDisplay(display);
        target = endpoint.client_target;
        fence = endpoint.client_acquire_fence;
        endpoint.client_acquire_fence = -1;
    }
    const bool wide = IsWideDisplay(display);
    if (!wide && !DrainWideRoute(d, "PresentDisplay Small")) {
        if (fence >= 0)
            close(fence);
        if (out_retire_fence)
            *out_retire_fence = -1;
        return HWC2_ERROR_NO_RESOURCES;
    }
    if (!target) {
        if (fence >= 0)
            close(fence);
        if (out_retire_fence)
            *out_retire_fence = -1;
        return HWC2_ERROR_NONE;
    }
    int width = 0, height = 0, stride = 0, format = 0, flags = 0;
    GetGrallocStridePx(target, &stride, &width, &height, &format, &flags);
    const int expected_width = wide ? kZoomWidth : FUJISAN_SEC_WIDTH;
    const int expected_height = wide ? kZoomHeight : FUJISAN_SEC_HEIGHT;
    if (width != expected_width || height != expected_height) {
        if (fence >= 0)
            close(fence);
        if (out_retire_fence)
            *out_retire_fence = -1;
        ALOGW("%s: skip interim client target %dx%d stride=%d format=%d flags=0x%x",
              wide ? "wide" : "small", width, height, stride, format, flags);
        return HWC2_ERROR_NONE;
    }
    if (wide) {
        NoteZoomPresent(d, width, height, stride);
        return SubmitWideClientTarget(d, target, fence, out_retire_fence)
                ? HWC2_ERROR_NONE : HWC2_ERROR_NO_RESOURCES;
    }
    return SubmitSingleClientTarget(d, target, fence, out_retire_fence)
            ? HWC2_ERROR_NONE : HWC2_ERROR_NO_RESOURCES;
}

static int32_t SetActiveConfig(hwc2_device_t* device, hwc2_display_t display, hwc2_config_t config) {
    auto* d = ToDev(device);
    if (!IsFujisanDisplay(display)) {
        return d->fns.setActiveConfig
                ? d->fns.setActiveConfig(d->real, RealDisplayFor(display), config)
                : HWC2_ERROR_UNSUPPORTED;
    }
    if (!IsConfigForDisplay(display, config))
        return HWC2_ERROR_BAD_CONFIG;
    if (!IsWideDisplay(display) && !DrainWideRoute(d, "SetActiveConfig Small"))
        return HWC2_ERROR_NO_RESOURCES;
    {
        std::lock_guard<std::mutex> zl(d->zoom_lock);
        EndpointFor(d, display).active_config = ConfigForDisplay(display);
    }
    if (d->fns.setActiveConfig)
        (void)d->fns.setActiveConfig(d->real, kPrimaryDisplay, kPanelAConfig);
    return HWC2_ERROR_NONE;
}

static int32_t SetClientTarget(hwc2_device_t* device, hwc2_display_t display, buffer_handle_t target,
                               int32_t acquire_fence, int32_t dataspace, hwc_region_t damage) {
    auto* d = ToDev(device);
    if (!IsFujisanDisplay(display)) {
        return d->fns.setClientTarget
                ? d->fns.setClientTarget(d->real, RealDisplayFor(display), target, acquire_fence,
                                          dataspace, damage)
                : HWC2_ERROR_UNSUPPORTED;
    }
    if (!IsWideDisplay(display) && !DrainWideRoute(d, "SetClientTarget Small"))
        return HWC2_ERROR_NO_RESOURCES;
    {
        std::lock_guard<std::mutex> zl(d->zoom_lock);
        EndpointState& endpoint = EndpointFor(d, display);
        if (endpoint.client_acquire_fence >= 0)
            close(endpoint.client_acquire_fence);
        endpoint.client_target = target;
        endpoint.client_acquire_fence = acquire_fence;
        endpoint.active_config = ConfigForDisplay(display);
    }
    (void)dataspace;
    (void)damage;
    return HWC2_ERROR_NONE;
}

static int32_t SetColorMode(hwc2_device_t* device, hwc2_display_t display, int32_t mode) {
    auto* d = ToDev(device);
    return d->fns.setColorMode ? d->fns.setColorMode(d->real, RealDisplayFor(display), mode)
                               : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetColorTransform(hwc2_device_t* device, hwc2_display_t display, const float* m,
                                 int32_t hint) {
    auto* d = ToDev(device);
    return d->fns.setColorTransform
            ? d->fns.setColorTransform(d->real, RealDisplayFor(display), m, hint)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetCursorPosition(hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
                                 int32_t x, int32_t y) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display))
        return HWC2_ERROR_NONE;
    return d->fns.setCursorPosition
            ? d->fns.setCursorPosition(d->real, RealDisplayFor(display), layer, x, y)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetLayerBlendMode(hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
                                 int32_t mode) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display)) {
        std::lock_guard<std::mutex> zl(d->zoom_lock);
        auto it = d->zoom_layers.find(layer);
        if (it == d->zoom_layers.end() || it->second.owner != display)
            return HWC2_ERROR_BAD_LAYER;
        it->second.blend = mode;
        d->zoom_layers_validated = false;
        return HWC2_ERROR_NONE;
    }
    return d->fns.setLayerBlendMode
            ? d->fns.setLayerBlendMode(d->real, RealDisplayFor(display), layer, mode)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetLayerBuffer(hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
                              buffer_handle_t buffer, int32_t acquire_fence) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display)) {
        std::lock_guard<std::mutex> zl(d->zoom_lock);
        auto it = d->zoom_layers.find(layer);
        if (it == d->zoom_layers.end() || it->second.owner != display)
            return HWC2_ERROR_BAD_LAYER;
        it->second.buffer = buffer;
        if (it->second.acquire_fence >= 0)
            close(it->second.acquire_fence);
        it->second.acquire_fence = acquire_fence >= 0 ? dup(acquire_fence) : -1;
        if (acquire_fence >= 0)
            close(acquire_fence);
        d->zoom_layers_validated = false;
        return HWC2_ERROR_NONE;
    }
    return d->fns.setLayerBuffer
            ? d->fns.setLayerBuffer(d->real, RealDisplayFor(display), layer, buffer, acquire_fence)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetLayerColor(hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
                             hwc_color_t color) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display)) {
        std::lock_guard<std::mutex> zl(d->zoom_lock);
        auto it = d->zoom_layers.find(layer);
        if (it == d->zoom_layers.end() || it->second.owner != display)
            return HWC2_ERROR_BAD_LAYER;
        it->second.color = color;
        it->second.has_color = true;
        d->zoom_layers_validated = false;
        return HWC2_ERROR_NONE;
    }
    return d->fns.setLayerColor
            ? d->fns.setLayerColor(d->real, RealDisplayFor(display), layer, color)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetLayerCompositionType(hwc2_device_t* device, hwc2_display_t display,
                                       hwc2_layer_t layer, int32_t type) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display)) {
        std::lock_guard<std::mutex> zl(d->zoom_lock);
        auto it = d->zoom_layers.find(layer);
        if (it == d->zoom_layers.end() || it->second.owner != display)
            return HWC2_ERROR_BAD_LAYER;
        it->second.requested = type;
        d->zoom_layers_validated = false;
        return HWC2_ERROR_NONE;
    }
    return d->fns.setLayerCompositionType
            ? d->fns.setLayerCompositionType(d->real, RealDisplayFor(display), layer, type)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetLayerDataspace(hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
                                 int32_t dataspace) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display)) {
        std::lock_guard<std::mutex> zl(d->zoom_lock);
        auto it = d->zoom_layers.find(layer);
        if (it == d->zoom_layers.end() || it->second.owner != display)
            return HWC2_ERROR_BAD_LAYER;
        it->second.dataspace = dataspace;
        d->zoom_layers_validated = false;
        return HWC2_ERROR_NONE;
    }
    return d->fns.setLayerDataspace
            ? d->fns.setLayerDataspace(d->real, RealDisplayFor(display), layer, dataspace)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetLayerDisplayFrame(hwc2_device_t* device, hwc2_display_t display,
                                    hwc2_layer_t layer, hwc_rect_t frame) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display)) {
        std::lock_guard<std::mutex> zl(d->zoom_lock);
        auto it = d->zoom_layers.find(layer);
        if (it == d->zoom_layers.end() || it->second.owner != display)
            return HWC2_ERROR_BAD_LAYER;
        it->second.frame = frame;
        d->zoom_layers_validated = false;
        return HWC2_ERROR_NONE;
    }
    return d->fns.setLayerDisplayFrame
            ? d->fns.setLayerDisplayFrame(d->real, RealDisplayFor(display), layer, frame)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetLayerPlaneAlpha(hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
                                  float alpha) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display)) {
        std::lock_guard<std::mutex> zl(d->zoom_lock);
        auto it = d->zoom_layers.find(layer);
        if (it == d->zoom_layers.end() || it->second.owner != display)
            return HWC2_ERROR_BAD_LAYER;
        it->second.alpha = alpha;
        d->zoom_layers_validated = false;
        return HWC2_ERROR_NONE;
    }
    return d->fns.setLayerPlaneAlpha
            ? d->fns.setLayerPlaneAlpha(d->real, RealDisplayFor(display), layer, alpha)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetLayerSidebandStream(hwc2_device_t* device, hwc2_display_t display,
                                      hwc2_layer_t layer, const native_handle_t* stream) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display)) {
        std::lock_guard<std::mutex> zl(d->zoom_lock);
        auto it = d->zoom_layers.find(layer);
        if (it == d->zoom_layers.end() || it->second.owner != display)
            return HWC2_ERROR_BAD_LAYER;
        it->second.sideband = stream;
        it->second.has_sideband = stream != nullptr;
        d->zoom_layers_validated = false;
        return HWC2_ERROR_NONE;
    }
    return d->fns.setLayerSidebandStream
            ? d->fns.setLayerSidebandStream(d->real, RealDisplayFor(display), layer, stream)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetLayerSourceCrop(hwc2_device_t* device, hwc2_display_t display,
                                  hwc2_layer_t layer, hwc_frect_t crop) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display)) {
        std::lock_guard<std::mutex> zl(d->zoom_lock);
        auto it = d->zoom_layers.find(layer);
        if (it == d->zoom_layers.end() || it->second.owner != display)
            return HWC2_ERROR_BAD_LAYER;
        it->second.crop = crop;
        d->zoom_layers_validated = false;
        return HWC2_ERROR_NONE;
    }
    return d->fns.setLayerSourceCrop
            ? d->fns.setLayerSourceCrop(d->real, RealDisplayFor(display), layer, crop)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetLayerSurfaceDamage(hwc2_device_t* device, hwc2_display_t display,
                                     hwc2_layer_t layer, hwc_region_t damage) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display))
        return HWC2_ERROR_NONE;
    return d->fns.setLayerSurfaceDamage
            ? d->fns.setLayerSurfaceDamage(d->real, RealDisplayFor(display), layer, damage)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetLayerTransform(hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
                                 int32_t transform) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display)) {
        std::lock_guard<std::mutex> zl(d->zoom_lock);
        auto it = d->zoom_layers.find(layer);
        if (it == d->zoom_layers.end() || it->second.owner != display)
            return HWC2_ERROR_BAD_LAYER;
        it->second.transform = transform;
        d->zoom_layers_validated = false;
        return HWC2_ERROR_NONE;
    }
    return d->fns.setLayerTransform
            ? d->fns.setLayerTransform(d->real, RealDisplayFor(display), layer, transform)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetLayerVisibleRegion(hwc2_device_t* device, hwc2_display_t display,
                                     hwc2_layer_t layer, hwc_region_t visible) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display))
        return HWC2_ERROR_NONE;
    return d->fns.setLayerVisibleRegion
            ? d->fns.setLayerVisibleRegion(d->real, RealDisplayFor(display), layer, visible)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetLayerZOrder(hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
                              uint32_t z) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display)) {
        std::lock_guard<std::mutex> zl(d->zoom_lock);
        auto it = d->zoom_layers.find(layer);
        if (it == d->zoom_layers.end() || it->second.owner != display)
            return HWC2_ERROR_BAD_LAYER;
        it->second.z = z;
        d->zoom_layers_validated = false;
        return HWC2_ERROR_NONE;
    }
    return d->fns.setLayerZOrder
            ? d->fns.setLayerZOrder(d->real, RealDisplayFor(display), layer, z)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetOutputBuffer(hwc2_device_t* device, hwc2_display_t display, buffer_handle_t buffer,
                               int32_t release_fence) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display)) {
        if (release_fence >= 0)
            close(release_fence);
        return HWC2_ERROR_NONE;
    }
    return d->fns.setOutputBuffer
            ? d->fns.setOutputBuffer(d->real, RealDisplayFor(display), buffer, release_fence)
            : HWC2_ERROR_UNSUPPORTED;
}

/* Small and Wide are two framework displays but one physical fb0.  The
 * DeviceState layout powers the old endpoint off before it powers the new one
 * on.  Forwarding that first OFF to CAF zeros both panel backlights, and the
 * later Wide atomic commit can only re-enable B.  Defer a physical OFF long
 * enough for the paired endpoint to claim fb0; an actual screen-off remains
 * OFF after the grace interval. */
static void* SharedPowerOffThreadMain(void* arg) {
    auto* d = reinterpret_cast<Device*>(arg);
    for (;;) {
        const uint64_t generation = d->shared_power_generation.load();
        usleep(1000 * 1000);
        if (!d->shared_power_off_thread_run.load())
            break;
        if (generation != d->shared_power_generation.load())
            continue;
        if (!d->small_power_on.load() && !d->wide_power_on.load() &&
            d->fns.setPowerMode) {
            ALOGI("shared fb0 power off after endpoint handoff grace");
            (void)d->fns.setPowerMode(d->real, kPrimaryDisplay, HWC2_POWER_MODE_OFF);
        }
        break;
    }
    d->shared_power_off_thread_run.store(false);
    return nullptr;
}

static void ScheduleSharedPowerOff(Device* d) {
    d->shared_power_generation.fetch_add(1);
    bool expected = false;
    if (!d->shared_power_off_thread_run.compare_exchange_strong(expected, true))
        return;
    if (pthread_create(&d->shared_power_off_thread, nullptr,
                       SharedPowerOffThreadMain, d) != 0) {
        d->shared_power_off_thread_run.store(false);
        ALOGE("shared fb0 deferred power-off thread create failed");
    }
}

static int32_t SetPowerMode(hwc2_device_t* device, hwc2_display_t display, int32_t mode) {
    auto* d = ToDev(device);
    if (!IsFujisanDisplay(display)) {
        return d->fns.setPowerMode
                ? d->fns.setPowerMode(d->real, RealDisplayFor(display), mode)
                : HWC2_ERROR_UNSUPPORTED;
    }
    /* Both endpoints share fb0.  A layout can issue Wide=OFF while Small is
     * already ON (or vice versa), so forward OFF only when neither endpoint
     * remains powered. */
    const bool on = mode == HWC2_POWER_MODE_ON;
    if (IsWideDisplay(display))
        d->wide_power_on.store(on);
    else
        d->small_power_on.store(on);
    d->shared_power_generation.fetch_add(1);
    if (!on) {
        if (!d->small_power_on.load() && !d->wide_power_on.load())
            ScheduleSharedPowerOff(d);
        return HWC2_ERROR_NONE;
    }
    const int32_t ret = d->fns.setPowerMode
            ? d->fns.setPowerMode(d->real, kPrimaryDisplay, HWC2_POWER_MODE_ON)
            : HWC2_ERROR_UNSUPPORTED;
    if (ret != HWC2_ERROR_NONE)
        return ret;
    const int cached_level = BrightnessFor(d, display).load();
    if (cached_level > 0) {
        ALOGI("restore %s brightness=%d after power on",
              IsWideDisplay(display) ? "wide" : "small", cached_level);
        if (!WritePrimaryBacklightLevel(cached_level))
            return HWC2_ERROR_NO_RESOURCES;
    }
    return HWC2_ERROR_NONE;
}

static int32_t SetVsyncEnabled(hwc2_device_t* device, hwc2_display_t display, int32_t enabled) {
    auto* d = ToDev(device);
    if (!IsFujisanDisplay(display)) {
        return d->fns.setVsyncEnabled
                ? d->fns.setVsyncEnabled(d->real, RealDisplayFor(display), enabled)
                : HWC2_ERROR_UNSUPPORTED;
    }
    const bool is_enabled = enabled == HWC2_VSYNC_ENABLE;
    if (IsWideDisplay(display))
        d->wide_vsync_enabled.store(is_enabled);
    else
        d->small_vsync_enabled.store(is_enabled);
    const bool real_enabled = d->small_vsync_enabled.load() || d->wide_vsync_enabled.load();
    return d->fns.setVsyncEnabled
            ? d->fns.setVsyncEnabled(d->real, kPrimaryDisplay,
                                     real_enabled ? HWC2_VSYNC_ENABLE : HWC2_VSYNC_DISABLE)
            : HWC2_ERROR_UNSUPPORTED;
}

static int32_t ValidateDisplay(hwc2_device_t* device, hwc2_display_t display, uint32_t* out_types,
                               uint32_t* out_requests) {
    auto* d = ToDev(device);
    if (IsFujisanDisplay(display)) {
        std::lock_guard<std::mutex> zl(d->zoom_lock);
        uint32_t changes = 0;
        d->zoom_force_client = true;
        for (auto& kv : d->zoom_layers) {
            ZoomLayer& layer = kv.second;
            if (layer.owner != display)
                continue;
            layer.device_candidate = false;
            layer.changed = layer.requested != HWC2_COMPOSITION_CLIENT;
            layer.validated = HWC2_COMPOSITION_CLIENT;
            if (layer.changed)
                ++changes;
        }
        d->zoom_layers_validated = true;
        if (out_types)
            *out_types = changes;
        if (out_requests)
            *out_requests = 0;
        return changes ? HWC2_ERROR_HAS_CHANGES : HWC2_ERROR_NONE;
    }
    return d->fns.validateDisplay
            ? d->fns.validateDisplay(d->real, RealDisplayFor(display), out_types, out_requests)
            : HWC2_ERROR_UNSUPPORTED;
}


static void Dump(hwc2_device_t* device, uint32_t* out_size, char* out_buffer) {
    auto* d = ToDev(device);
    if (d->fns.dump)
        d->fns.dump(d->real, out_size, out_buffer);
}

static int32_t CreateVirtualDisplay(hwc2_device_t* device, uint32_t w, uint32_t h, int32_t* format,
                                    hwc2_display_t* out) {
    auto* d = ToDev(device);
    return d->fns.createVirtualDisplay ? d->fns.createVirtualDisplay(d->real, w, h, format, out)
                                      : HWC2_ERROR_UNSUPPORTED;
}

static int32_t DestroyVirtualDisplay(hwc2_device_t* device, hwc2_display_t display) {
    auto* d = ToDev(device);
    return d->fns.destroyVirtualDisplay ? d->fns.destroyVirtualDisplay(d->real, display)
                                       : HWC2_ERROR_UNSUPPORTED;
}

static uint32_t GetMaxVirtualDisplayCount(hwc2_device_t* device) {
    auto* d = ToDev(device);
    return d->fns.getMaxVirtualDisplayCount ? d->fns.getMaxVirtualDisplayCount(d->real) : 0;
}

static hwc2_function_pointer_t WrapperGetFunction(struct hwc2_device* /*device*/,
                                                  int32_t descriptor) {
    switch (descriptor) {
        case HWC2_FUNCTION_ACCEPT_DISPLAY_CHANGES:
            return reinterpret_cast<hwc2_function_pointer_t>(AcceptDisplayChanges);
        case HWC2_FUNCTION_CREATE_LAYER:
            return reinterpret_cast<hwc2_function_pointer_t>(CreateLayer);
        case HWC2_FUNCTION_CREATE_VIRTUAL_DISPLAY:
            return reinterpret_cast<hwc2_function_pointer_t>(CreateVirtualDisplay);
        case HWC2_FUNCTION_DESTROY_LAYER:
            return reinterpret_cast<hwc2_function_pointer_t>(DestroyLayer);
        case HWC2_FUNCTION_DESTROY_VIRTUAL_DISPLAY:
            return reinterpret_cast<hwc2_function_pointer_t>(DestroyVirtualDisplay);
        case HWC2_FUNCTION_DUMP:
            return reinterpret_cast<hwc2_function_pointer_t>(Dump);
        case HWC2_FUNCTION_GET_ACTIVE_CONFIG:
            return reinterpret_cast<hwc2_function_pointer_t>(GetActiveConfig);
        case HWC2_FUNCTION_GET_CHANGED_COMPOSITION_TYPES:
            return reinterpret_cast<hwc2_function_pointer_t>(GetChangedCompositionTypes);
        case HWC2_FUNCTION_GET_CLIENT_TARGET_SUPPORT:
            return reinterpret_cast<hwc2_function_pointer_t>(GetClientTargetSupport);
        case HWC2_FUNCTION_GET_COLOR_MODES:
            return reinterpret_cast<hwc2_function_pointer_t>(GetColorModes);
        case HWC2_FUNCTION_GET_DISPLAY_ATTRIBUTE:
            return reinterpret_cast<hwc2_function_pointer_t>(GetDisplayAttribute);
        case HWC2_FUNCTION_GET_DISPLAY_CONFIGS:
            return reinterpret_cast<hwc2_function_pointer_t>(GetDisplayConfigs);
        case HWC2_FUNCTION_GET_DISPLAY_NAME:
            return reinterpret_cast<hwc2_function_pointer_t>(GetDisplayName);
        case HWC2_FUNCTION_GET_DISPLAY_REQUESTS:
            return reinterpret_cast<hwc2_function_pointer_t>(GetDisplayRequests);
        case HWC2_FUNCTION_GET_DISPLAY_TYPE:
            return reinterpret_cast<hwc2_function_pointer_t>(GetDisplayType);
        case HWC2_FUNCTION_GET_DISPLAY_CAPABILITIES:
            return reinterpret_cast<hwc2_function_pointer_t>(GetDisplayCapabilities);
        case HWC2_FUNCTION_SET_DISPLAY_BRIGHTNESS:
            return reinterpret_cast<hwc2_function_pointer_t>(SetDisplayBrightness);
        case HWC2_FUNCTION_GET_DISPLAY_CONNECTION_TYPE:
            return reinterpret_cast<hwc2_function_pointer_t>(GetDisplayConnectionType);
        case HWC2_FUNCTION_GET_DISPLAY_VSYNC_PERIOD:
            return reinterpret_cast<hwc2_function_pointer_t>(GetDisplayVsyncPeriod);
        case HWC2_FUNCTION_SET_ACTIVE_CONFIG_WITH_CONSTRAINTS:
            return reinterpret_cast<hwc2_function_pointer_t>(SetActiveConfigWithConstraints);
        case HWC2_FUNCTION_GET_DOZE_SUPPORT:
            return reinterpret_cast<hwc2_function_pointer_t>(GetDozeSupport);
        case HWC2_FUNCTION_GET_HDR_CAPABILITIES:
            return reinterpret_cast<hwc2_function_pointer_t>(GetHdrCapabilities);
        case HWC2_FUNCTION_GET_MAX_VIRTUAL_DISPLAY_COUNT:
            return reinterpret_cast<hwc2_function_pointer_t>(GetMaxVirtualDisplayCount);
        case HWC2_FUNCTION_GET_RELEASE_FENCES:
            return reinterpret_cast<hwc2_function_pointer_t>(GetReleaseFences);
        case HWC2_FUNCTION_PRESENT_DISPLAY:
            return reinterpret_cast<hwc2_function_pointer_t>(PresentDisplay);
        case HWC2_FUNCTION_REGISTER_CALLBACK:
            return reinterpret_cast<hwc2_function_pointer_t>(RegisterCallback);
        case HWC2_FUNCTION_SET_ACTIVE_CONFIG:
            return reinterpret_cast<hwc2_function_pointer_t>(SetActiveConfig);
        case HWC2_FUNCTION_SET_CLIENT_TARGET:
            return reinterpret_cast<hwc2_function_pointer_t>(SetClientTarget);
        case HWC2_FUNCTION_SET_COLOR_MODE:
            return reinterpret_cast<hwc2_function_pointer_t>(SetColorMode);
        case HWC2_FUNCTION_SET_COLOR_TRANSFORM:
            return reinterpret_cast<hwc2_function_pointer_t>(SetColorTransform);
        case HWC2_FUNCTION_SET_CURSOR_POSITION:
            return reinterpret_cast<hwc2_function_pointer_t>(SetCursorPosition);
        case HWC2_FUNCTION_SET_LAYER_BLEND_MODE:
            return reinterpret_cast<hwc2_function_pointer_t>(SetLayerBlendMode);
        case HWC2_FUNCTION_SET_LAYER_BUFFER:
            return reinterpret_cast<hwc2_function_pointer_t>(SetLayerBuffer);
        case HWC2_FUNCTION_SET_LAYER_COLOR:
            return reinterpret_cast<hwc2_function_pointer_t>(SetLayerColor);
        case HWC2_FUNCTION_SET_LAYER_COMPOSITION_TYPE:
            return reinterpret_cast<hwc2_function_pointer_t>(SetLayerCompositionType);
        case HWC2_FUNCTION_SET_LAYER_DATASPACE:
            return reinterpret_cast<hwc2_function_pointer_t>(SetLayerDataspace);
        case HWC2_FUNCTION_SET_LAYER_DISPLAY_FRAME:
            return reinterpret_cast<hwc2_function_pointer_t>(SetLayerDisplayFrame);
        case HWC2_FUNCTION_SET_LAYER_PLANE_ALPHA:
            return reinterpret_cast<hwc2_function_pointer_t>(SetLayerPlaneAlpha);
        case HWC2_FUNCTION_SET_LAYER_SIDEBAND_STREAM:
            return reinterpret_cast<hwc2_function_pointer_t>(SetLayerSidebandStream);
        case HWC2_FUNCTION_SET_LAYER_SOURCE_CROP:
            return reinterpret_cast<hwc2_function_pointer_t>(SetLayerSourceCrop);
        case HWC2_FUNCTION_SET_LAYER_SURFACE_DAMAGE:
            return reinterpret_cast<hwc2_function_pointer_t>(SetLayerSurfaceDamage);
        case HWC2_FUNCTION_SET_LAYER_TRANSFORM:
            return reinterpret_cast<hwc2_function_pointer_t>(SetLayerTransform);
        case HWC2_FUNCTION_SET_LAYER_VISIBLE_REGION:
            return reinterpret_cast<hwc2_function_pointer_t>(SetLayerVisibleRegion);
        case HWC2_FUNCTION_SET_LAYER_Z_ORDER:
            return reinterpret_cast<hwc2_function_pointer_t>(SetLayerZOrder);
        case HWC2_FUNCTION_SET_OUTPUT_BUFFER:
            return reinterpret_cast<hwc2_function_pointer_t>(SetOutputBuffer);
        case HWC2_FUNCTION_SET_POWER_MODE:
            return reinterpret_cast<hwc2_function_pointer_t>(SetPowerMode);
        case HWC2_FUNCTION_SET_VSYNC_ENABLED:
            return reinterpret_cast<hwc2_function_pointer_t>(SetVsyncEnabled);
        case HWC2_FUNCTION_VALIDATE_DISPLAY:
            return reinterpret_cast<hwc2_function_pointer_t>(ValidateDisplay);
        default:
            return nullptr;
    }
}

static int OpenRealComposer(hwc2_device_t** out_real, void** out_so) {
    const char* paths[] = {
        "/vendor/lib64/hw/hwcomposer.msm8996.so",
        "/vendor/lib/hw/hwcomposer.msm8996.so",
        "hwcomposer.msm8996.so",
    };
    void* so = nullptr;
    for (const char* p : paths) {
        so = dlopen(p, RTLD_NOW | RTLD_LOCAL);
        if (so)
            break;
    }
    if (!so) {
        ALOGE("dlopen hwcomposer.msm8996 failed: %s", dlerror());
        return -ENOENT;
    }
    auto* hmi = reinterpret_cast<hw_module_t*>(dlsym(so, HAL_MODULE_INFO_SYM_AS_STR));
    if (!hmi || !hmi->methods || !hmi->methods->open) {
        ALOGE("HMI missing in hwcomposer.msm8996");
        dlclose(so);
        return -EINVAL;
    }
    hw_device_t* dev = nullptr;
    int err = hmi->methods->open(hmi, HWC_HARDWARE_COMPOSER, &dev);
    if (err || !dev) {
        ALOGE("open real HWC failed: %d", err);
        dlclose(so);
        return err ? err : -EIO;
    }
    *out_real = reinterpret_cast<hwc2_device_t*>(dev);
    *out_so = so;
    return 0;
}

static int HwcClose(hw_device_t* dev) {
    auto* d = reinterpret_cast<Device*>(dev);
    d->shared_power_off_thread_run.store(false);
    d->shared_power_generation.fetch_add(1);
    if (d->shared_power_off_thread)
        pthread_join(d->shared_power_off_thread, nullptr);
    d->primary_panel_thread_run.store(false);
    if (d->primary_panel_thread)
        pthread_join(d->primary_panel_thread, nullptr);
    CloseWideFramebuffer(d);
    if (d->small.client_acquire_fence >= 0)
        close(d->small.client_acquire_fence);
    if (d->wide.client_acquire_fence >= 0)
        close(d->wide.client_acquire_fence);
    if (d->real && d->real->common.close)
        d->real->common.close(reinterpret_cast<hw_device_t*>(d->real));
    if (d->real_so)
        dlclose(d->real_so);
    delete d;
    return 0;
}

static int HwcOpen(const struct hw_module_t* module, const char* name, struct hw_device_t** device) {
    if (!name || strcmp(name, HWC_HARDWARE_COMPOSER) != 0)
        return -EINVAL;

    /* Stage 2 accepts the explicit linear RGBA/RGBX contract only.  CAF
     * gralloc reads these vendor properties when its module is initialized;
     * the old debug.gralloc.* spelling is not consumed by this source tree. */
    property_set("vendor.gralloc.disable_ubwc", "1");
    property_set("vendor.gralloc.enable_fb_ubwc", "0");

    auto* d = new Device();
    int err = OpenRealComposer(&d->real, &d->real_so);
    if (err) {
        delete d;
        return err;
    }
    LoadRealFns(d);

    d->base.common.tag = HARDWARE_DEVICE_TAG;
    d->base.common.version = HWC_DEVICE_API_VERSION_2_0;
    d->base.common.module = const_cast<hw_module_t*>(module);
    d->base.common.close = HwcClose;
    d->base.getCapabilities = WrapperGetCapabilities;
    d->base.getFunction = WrapperGetFunction;

    *device = &d->base.common;
    ALOGI("Fujisan HWC2 wrapper open (stable Small/Wide endpoints)");
    return 0;
}

static struct hw_module_methods_t g_methods = {
    .open = HwcOpen,
};

}  // namespace

// HWC2 is selected by device API version (HWC_DEVICE_API_VERSION_2_0).
// Module API remains 0.1 (there is no HWC_MODULE_API_VERSION_2_0).
hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .module_api_version = HWC_MODULE_API_VERSION_0_1,
    .hal_api_version = HARDWARE_HAL_API_VERSION,
    .id = HWC_HARDWARE_MODULE_ID,
    .name = "Fujisan dual-panel HWC2 wrapper",
    .author = "Fujisan bring-up",
    .methods = &g_methods,
};
