/*
 * Fujisan HWC2 wrapper (Composer 2.4)
 *  Closed (single): passthrough hwcomposer.msm8996 on panel A or B (primary_panel).
 *  Open (zoom): one logical 2160x1920 INTERNAL, client target split to fb0|fb1 with 1px hinge.
 *  Secondary hotplug disabled — stock dual-LCD ZOOM path, not dual INTERNAL.
 */
#define LOG_TAG "HwcFujisan"

#include <android/hardware/graphics/common/1.0/types.h>
#include <android/hardware/graphics/mapper/2.0/IMapper.h>
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
#ifndef GRALLOC_MODULE_PERFORM_GET_RGB_DATA_ADDRESS
#define GRALLOC_MODULE_PERFORM_GET_RGB_DATA_ADDRESS 10
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
#include <pthread.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <time.h>
#include <unistd.h>

#include <atomic>
#include <algorithm>
#include <cstdint>
#include <map>
#include <mutex>
#include <vector>

using android::hardware::hidl_handle;
using android::hardware::graphics::common::V1_0::BufferUsage;
using android::hardware::graphics::mapper::V2_0::Error;
using android::hardware::graphics::mapper::V2_0::IMapper;
using android::sp;

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

constexpr hwc2_display_t kPrimaryDisplay = 0;
constexpr hwc2_display_t kSecondaryDisplay = 1;
constexpr hwc2_config_t kSecondaryConfig = 0;
constexpr hwc2_config_t kSingleConfig = 0;
constexpr hwc2_config_t kZoomConfig = 1;
constexpr int kZoomWidth = 2160;  /* 1080 + 1 hinge + 1079 usable right? 1080+1080 */
/* Open virtual size is 2160x1920: left 1080 (A) + right 1080 (B). Hinge is 1px at x=1080
 * declared to WM via device_state/fold overlays; pixels at x=1080 may be skipped when splitting.
 */
constexpr char kFb1Path[] = "/dev/graphics/fb1";
constexpr char kBl2Path[] = "/sys/class/leds/lcd-backlight-2/brightness";

#ifndef MSMFB_DISPLAY_COMMIT
#define MSMFB_IOCTL_MAGIC 'm'
/* ioctl numbers include the full argument size; void* is not interchangeable
 * with mdp_display_commit here. */
#define MSMFB_DISPLAY_COMMIT _IOW(MSMFB_IOCTL_MAGIC, 164, struct MdpDisplayCommit)
#endif

struct MdpDisplayCommit {
    uint32_t flags;
    uint32_t wait_for_finish;
    struct fb_var_screeninfo var;
    struct {
        int32_t x, y, w, h;
    } l_roi, r_roi;
};

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

struct SecLayer {
    int32_t requested = HWC2_COMPOSITION_CLIENT;
    int32_t validated = HWC2_COMPOSITION_CLIENT;
    bool changed = false;
};

struct SecondaryState {
    std::mutex lock;
    std::map<hwc2_layer_t, SecLayer> layers;
    hwc2_layer_t next_layer = 1000;
    bool validated = false;
    bool power_on = true;
    bool vsync_on = false;
    bool hotplugged = false;
    buffer_handle_t client_target = nullptr;
    int32_t client_acquire_fence = -1;
};

struct Device {
    hwc2_device_t base{};
    hwc2_device_t* real = nullptr;
    void* real_so = nullptr;
    RealFns fns{};
    SecondaryState sec{};

    hwc2_callback_data_t hotplug_data = nullptr;
    HWC2_PFN_HOTPLUG hotplug_fn = nullptr;
    hwc2_callback_data_t vsync_data = nullptr;
    HWC2_PFN_VSYNC vsync_fn = nullptr;
    hwc2_callback_data_t vsync24_data = nullptr;
    HWC2_PFN_VSYNC_2_4 vsync24_fn = nullptr;
    hwc2_callback_data_t refresh_data = nullptr;
    HWC2_PFN_REFRESH refresh_fn = nullptr;

    std::mutex cb_lock;
    pthread_t vsync_thread{};
    std::atomic<bool> vsync_thread_run{false};
    std::atomic<bool> secondary_attached{false};
    std::atomic<bool> secondary_topology_pending{false};
    std::atomic<bool> secondary_overlay_reset_pending{false};
    std::atomic<bool> primary_refresh_after_topology{false};
    std::atomic<bool> primary_reprobe_pending{false};

    int fb_fd = -1;
    void* fb_map = MAP_FAILED;
    size_t fb_map_size = 0;
    struct fb_var_screeninfo vinfo {};
    struct fb_fix_screeninfo finfo {};
    uint32_t sec_overlay_id = MSMFB_NEW_REQUEST;
    uint32_t pri_overlay_id = MSMFB_NEW_REQUEST;
    /* The legacy MDP overlay stores its source geometry and pixel format at
     * OVERLAY_SET time.  SurfaceFlinger may first submit a 1080-wide target
     * while changing to zoom, then replace it with the 2160-wide target. */
    uint32_t sec_overlay_src_width = 0;
    uint32_t sec_overlay_src_height = 0;
    uint32_t sec_overlay_src_format = 0;
    int sec_overlay_crop_x = -1;
    uint32_t sec_overlay_crop_width = 0;
    sp<IMapper> mapper;

    /* ZOOM (open) virtual large screen on primary display id 0. */
    std::mutex zoom_lock;
    bool zoom_active = false;
    hwc2_config_t active_config = kSingleConfig;
    buffer_handle_t client_target = nullptr;
    int32_t client_acquire_fence = -1;
    buffer_handle_t zoom_client_target = nullptr; /* alias while zoom */
    int32_t zoom_acquire_fence = -1;
    bool disable_secondary = true; /* dual INTERNAL off for hinge/zoom path */
    int fb0_fd = -1;
    void* fb0_map = MAP_FAILED;
    size_t fb0_map_size = 0;
    struct fb_var_screeninfo vinfo0 {};
    struct fb_fix_screeninfo finfo0 {};
    char last_mode[16] = "single";
    bool mode_seen = false;
    bool topology_seen = false;
    bool dual_internal_active = false;

    /* Physical B power is owned by fujisan_halld.  These track its hinge
     * availability so the independent logical display gets one redraw only
     * after the panel has settled following an open. */
    bool secondary_panel_available = false;
    bool secondary_panel_refresh_pending = false;
    int64_t secondary_panel_available_since_ns = 0;
};

static bool DualInternalEnabled() {
    /* The product supports only the virtual A+B topology.  Ignore old
     * persisted dual-mode properties so an OTA from an earlier build cannot
     * resurrect the retired independent INTERNAL display. */
    return false;
}

static bool WantZoomMode() {
    if (DualInternalEnabled())
        return false;
    char buf[PROPERTY_VALUE_MAX] = {};
    property_get("vendor.fujisan.display_mode", buf, "single");
    return strcmp(buf, "zoom") == 0;
}

static bool WantPrimaryB() {
    char buf[PROPERTY_VALUE_MAX] = {};
    property_get("vendor.fujisan.active_primary", buf, "a");
    return buf[0] == 'b' || buf[0] == 'B';
}

static bool SecondaryPanelAvailable() {
    if (!DualInternalEnabled())
        return false;

    char force_b[PROPERTY_VALUE_MAX] = {};
    property_get("persist.vendor.fujisan.force_b_on", force_b, "0");
    if (force_b[0] == '1')
        return true;

    char hall[PROPERTY_VALUE_MAX] = {};
    property_get("vendor.fujisan.hall_status", hall, "1");
    /* A(1) is folded; B(2) is mid-open and C(3) is fully open. */
    return hall[0] == '2' || hall[0] == '3';
}

static int64_t MonotonicNs() {
    struct timespec ts {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
}

static void SetDisplayPowerProp(bool on) {
    property_set("vendor.fujisan.display_power", on ? "1" : "0");
}


static Device* ToDev(hwc2_device_t* d) {
    return reinterpret_cast<Device*>(d);
}

static int WriteSysfs(const char* path, const char* value) {
    int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0)
        return -errno;
    ssize_t n = write(fd, value, strlen(value));
    close(fd);
    return n < 0 ? -errno : 0;
}

static int KickFb(int fd, struct fb_var_screeninfo* vinfo) {
    /* Non-blocking: wait_for_finish deadlocks composer when posting fb1 from Present. */
    MdpDisplayCommit commit;
    memset(&commit, 0, sizeof(commit));
    commit.wait_for_finish = 0;
    commit.var = *vinfo;
    commit.var.activate = FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;
    if (ioctl(fd, MSMFB_DISPLAY_COMMIT, &commit) == 0)
        return 0;
    vinfo->activate = FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;
    return ioctl(fd, FBIOPAN_DISPLAY, vinfo);
}

static bool OpenFb1(Device* d) {
    if (d->fb_fd >= 0)
        return true;
    d->fb_fd = open(kFb1Path, O_RDWR | O_CLOEXEC);
    if (d->fb_fd < 0) {
        ALOGE("open %s failed: %s", kFb1Path, strerror(errno));
        return false;
    }
    if (ioctl(d->fb_fd, FBIOGET_VSCREENINFO, &d->vinfo) < 0 ||
        ioctl(d->fb_fd, FBIOGET_FSCREENINFO, &d->finfo) < 0) {
        ALOGE("fb1 get screeninfo failed: %s", strerror(errno));
        close(d->fb_fd);
        d->fb_fd = -1;
        return false;
    }
    if (d->vinfo.xres == 0)
        d->vinfo.xres = FUJISAN_SEC_WIDTH;
    if (d->vinfo.yres == 0)
        d->vinfo.yres = FUJISAN_SEC_HEIGHT;
    if (d->vinfo.bits_per_pixel == 0)
        d->vinfo.bits_per_pixel = 32;
    if (d->vinfo.xres_virtual < d->vinfo.xres)
        d->vinfo.xres_virtual = d->vinfo.xres;
    if (d->vinfo.yres_virtual < d->vinfo.yres * 2)
        d->vinfo.yres_virtual = d->vinfo.yres * 2;
    d->vinfo.xoffset = 0;
    d->vinfo.yoffset = 0;
    d->vinfo.activate = FB_ACTIVATE_NOW;
    ioctl(d->fb_fd, FBIOPUT_VSCREENINFO, &d->vinfo);
    ioctl(d->fb_fd, FBIOGET_FSCREENINFO, &d->finfo);
    ioctl(d->fb_fd, FBIOGET_VSCREENINFO, &d->vinfo);

    d->fb_map_size = d->finfo.smem_len;
    if (d->fb_map_size == 0) {
        d->fb_map_size = (size_t)d->vinfo.xres_virtual * d->vinfo.yres_virtual *
                         (d->vinfo.bits_per_pixel / 8);
    }
    d->fb_map = mmap(nullptr, d->fb_map_size, PROT_READ | PROT_WRITE, MAP_SHARED, d->fb_fd, 0);
    if (d->fb_map == MAP_FAILED) {
        ALOGE("fb1 mmap failed: %s", strerror(errno));
        close(d->fb_fd);
        d->fb_fd = -1;
        return false;
    }
    /* Map only — do not force unblank/backlight (halld owns B power in single mode). */
    ALOGI("fb1 mapped %ux%u bpp=%u line=%u smem=%zu", d->vinfo.xres, d->vinfo.yres,
          d->vinfo.bits_per_pixel, d->finfo.line_length, d->fb_map_size);
    return true;
}

static void CloseFb1(Device* d) {
    if (d->fb_fd >= 0 && d->sec_overlay_id != MSMFB_NEW_REQUEST) {
        uint32_t id = d->sec_overlay_id;
        (void)ioctl(d->fb_fd, MSMFB_OVERLAY_UNSET, &id);
        d->sec_overlay_id = MSMFB_NEW_REQUEST;
    }
    d->sec_overlay_src_width = 0;
    d->sec_overlay_src_height = 0;
    d->sec_overlay_src_format = 0;
    d->sec_overlay_crop_x = -1;
    d->sec_overlay_crop_width = 0;
    if (d->fb_map != MAP_FAILED) {
        munmap(d->fb_map, d->fb_map_size);
        d->fb_map = MAP_FAILED;
        d->fb_map_size = 0;
    }
    if (d->fb_fd >= 0) {
        close(d->fb_fd);
        d->fb_fd = -1;
    }
}

static bool OpenFb0(Device* d) {
    if (d->fb0_fd >= 0)
        return true;
    d->fb0_fd = open("/dev/graphics/fb0", O_RDWR | O_CLOEXEC);
    if (d->fb0_fd < 0) {
        ALOGE("open fb0 failed: %s", strerror(errno));
        return false;
    }
    if (ioctl(d->fb0_fd, FBIOGET_VSCREENINFO, &d->vinfo0) < 0 ||
        ioctl(d->fb0_fd, FBIOGET_FSCREENINFO, &d->finfo0) < 0) {
        ALOGE("fb0 get screeninfo failed: %s", strerror(errno));
        close(d->fb0_fd);
        d->fb0_fd = -1;
        return false;
    }
    d->fb0_map_size = d->finfo0.smem_len;
    if (d->fb0_map_size == 0) {
        d->fb0_map_size = (size_t)d->vinfo0.xres_virtual * d->vinfo0.yres_virtual *
                          (d->vinfo0.bits_per_pixel / 8);
    }
    d->fb0_map = mmap(nullptr, d->fb0_map_size, PROT_READ | PROT_WRITE, MAP_SHARED, d->fb0_fd, 0);
    if (d->fb0_map == MAP_FAILED) {
        ALOGE("fb0 mmap failed: %s", strerror(errno));
        close(d->fb0_fd);
        d->fb0_fd = -1;
        return false;
    }
    ALOGI("fb0 mapped %ux%u line=%u smem=%zu", d->vinfo0.xres, d->vinfo0.yres,
          d->finfo0.line_length, d->fb0_map_size);
    return true;
}

static hwc2_vsync_period_t PrimaryVsyncPeriodNs(Device* d, hwc2_display_t display) {
    hwc2_config_t cfg = 0;
    if (d->fns.getActiveConfig)
        d->fns.getActiveConfig(d->real, display, &cfg);
    int32_t period = 0;
    if (d->fns.getDisplayAttribute) {
        d->fns.getDisplayAttribute(d->real, display, cfg, HWC2_ATTRIBUTE_VSYNC_PERIOD, &period);
    }
    if (period <= 0)
        period = FUJISAN_SEC_VSYNC_NS;
    return static_cast<hwc2_vsync_period_t>(period);
}

/* Stock msm8996 only emits legacy VSYNC. Composer 2.4 needs VSYNC_2_4. */
static void PrimaryVsyncTrampoline(hwc2_callback_data_t cb_data, hwc2_display_t display,
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
    if (fn24) {
        fn24(data24, display, timestamp, PrimaryVsyncPeriodNs(dev, display));
    }
    if (fn) {
        fn(data, display, timestamp);
    }
}

static int32_t WirePrimaryVsync(Device* d) {
    if (!d->fns.registerCallback)
        return HWC2_ERROR_UNSUPPORTED;
    return d->fns.registerCallback(d->real, HWC2_CALLBACK_VSYNC, d,
                                   reinterpret_cast<hwc2_function_pointer_t>(PrimaryVsyncTrampoline));
}

static void* VsyncThreadMain(void* arg) {
    auto* d = static_cast<Device*>(arg);
    prctl(PR_SET_NAME, "fujisan-sec-vsync", 0, 0, 0);
    while (d->vsync_thread_run.load()) {
        bool fire = false;
        {
            std::lock_guard<std::mutex> sc(d->sec.lock);
            fire = d->sec.vsync_on && d->sec.power_on && d->sec.hotplugged;
        }
        HWC2_PFN_VSYNC_2_4 fn24 = nullptr;
        HWC2_PFN_VSYNC fn = nullptr;
        hwc2_callback_data_t data24 = nullptr;
        hwc2_callback_data_t data = nullptr;
        {
            std::lock_guard<std::mutex> cl(d->cb_lock);
            fn24 = d->vsync24_fn;
            data24 = d->vsync24_data;
            fn = d->vsync_fn;
            data = d->vsync_data;
        }
        if (fire && (fn24 || fn)) {
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            int64_t t = int64_t(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
            if (fn24)
                fn24(data24, kSecondaryDisplay, t, FUJISAN_SEC_VSYNC_NS);
            if (fn)
                fn(data, kSecondaryDisplay, t);
        }
        usleep(FUJISAN_SEC_VSYNC_NS / 1000);
    }
    return nullptr;
}

static void EnsureVsyncThread(Device* d) {
    bool expected = false;
    if (d->vsync_thread_run.compare_exchange_strong(expected, true)) {
        if (pthread_create(&d->vsync_thread, nullptr, VsyncThreadMain, d) != 0) {
            d->vsync_thread_run.store(false);
            ALOGE("secondary vsync thread create failed");
        }
    }
}

static void ReconcileSecondaryTopology(Device* d) {
    /* Never call a SurfaceFlinger callback from PresentDisplay. The callback
     * can synchronously query HWC, so topology changes are serialized by a
     * detached worker. */
    HWC2_PFN_HOTPLUG fn = nullptr;
    hwc2_callback_data_t data = nullptr;
    {
        std::lock_guard<std::mutex> cl(d->cb_lock);
        fn = d->hotplug_fn;
        data = d->hotplug_data;
    }
    if (!fn)
        return;

    const bool want_dual = DualInternalEnabled();
    const bool attached = d->secondary_attached.load();
    if (want_dual == attached)
        return;

    /* An independent B target and a zoom right-half target have different
     * source crops. Retire the old MDSS overlay before the next present path
     * allocates the replacement. */
    d->secondary_overlay_reset_pending.store(true);

    if (want_dual) {
        {
            std::lock_guard<std::mutex> sc(d->sec.lock);
            d->sec.hotplugged = true;
            d->sec.power_on = true;
            d->sec.validated = false;
        }
        d->secondary_attached.store(true);
        EnsureVsyncThread(d);
        ALOGI("attach independent INTERNAL display 1");
        fn(data, kSecondaryDisplay, HWC2_CONNECTION_CONNECTED);
        return;
    }

    {
        std::lock_guard<std::mutex> sc(d->sec.lock);
        d->sec.hotplugged = false;
        d->sec.power_on = false;
        d->sec.validated = false;
        d->sec.layers.clear();
        d->sec.client_target = nullptr;
        if (d->sec.client_acquire_fence >= 0) {
            close(d->sec.client_acquire_fence);
            d->sec.client_acquire_fence = -1;
        }
    }
    d->secondary_attached.store(false);
    ALOGI("detach independent INTERNAL display 1 for zoom topology");
    fn(data, kSecondaryDisplay, HWC2_CONNECTION_DISCONNECTED);
}

static void ScheduleSecondaryTopology(Device* d);
static void SchedulePrimaryReprobe(Device* d);

static void* SecondaryTopologyThreadMain(void* arg) {
    auto* d = reinterpret_cast<Device*>(arg);
    ReconcileSecondaryTopology(d);
    if (d->primary_refresh_after_topology.exchange(false))
        SchedulePrimaryReprobe(d);
    d->secondary_topology_pending.store(false);
    /* A user can tap twice while this callback is in flight. Schedule exactly
     * one follow-up only if the final property differs from the state we just
     * published; there is no periodic work in the steady state. */
    if (DualInternalEnabled() != d->secondary_attached.load())
        ScheduleSecondaryTopology(d);
    return nullptr;
}

static void ScheduleSecondaryTopology(Device* d) {
    if (d->secondary_topology_pending.exchange(true))
        return;
    pthread_t thread {};
    if (pthread_create(&thread, nullptr, SecondaryTopologyThreadMain, d) != 0) {
        d->secondary_topology_pending.store(false);
        ALOGE("secondary topology worker create failed");
        return;
    }
    pthread_detach(thread);
}

static void ResetSecondaryOverlayIfNeeded(Device* d) {
    if (!d->secondary_overlay_reset_pending.exchange(false))
        return;
    if (d->fb_fd >= 0 && d->sec_overlay_id != MSMFB_NEW_REQUEST) {
        uint32_t id = d->sec_overlay_id;
        (void)ioctl(d->fb_fd, MSMFB_OVERLAY_UNSET, &id);
    }
    d->sec_overlay_id = MSMFB_NEW_REQUEST;
    d->sec_overlay_src_width = 0;
    d->sec_overlay_src_height = 0;
    d->sec_overlay_src_format = 0;
    d->sec_overlay_crop_x = -1;
    d->sec_overlay_crop_width = 0;
}

/* SurfaceFlinger reloads a physical display's supported modes after a connected
 * callback for an already-known HWC display.  Do that off the present path to
 * avoid re-entering SurfaceFlinger.  Deliberately do not send a preceding
 * disconnect: that tears down the logical display, briefly invalidates every
 * input viewport and makes SystemUI look like it has rebooted. */
static void* PrimaryReprobeThreadMain(void* arg) {
    auto* d = reinterpret_cast<Device*>(arg);
    HWC2_PFN_HOTPLUG fn = nullptr;
    hwc2_callback_data_t data = nullptr;
    {
        std::lock_guard<std::mutex> cl(d->cb_lock);
        fn = d->hotplug_fn;
        data = d->hotplug_data;
    }
    if (fn) {
        ALOGI("reprobe primary display: in-place mode refresh");
        fn(data, kPrimaryDisplay, HWC2_CONNECTION_CONNECTED);
    }
    d->primary_reprobe_pending.store(false);
    return nullptr;
}

static void SchedulePrimaryReprobe(Device* d) {
    if (d->primary_reprobe_pending.exchange(true))
        return;
    pthread_t thread {};
    if (pthread_create(&thread, nullptr, PrimaryReprobeThreadMain, d) != 0) {
        d->primary_reprobe_pending.store(false);
        ALOGE("primary reprobe thread create failed");
        return;
    }
    pthread_detach(thread);
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

/* Read-only probe: determine whether the Qualcomm composer owns fb1 itself. */
static void ProbeRealSecondary(Device* d) {
    if (!d->fns.getDisplayConfigs)
        return;

    uint32_t count = 0;
    int32_t err = d->fns.getDisplayConfigs(d->real, kSecondaryDisplay, &count, nullptr);
    ALOGI("real display 1 probe: getConfigs err=%d count=%u", err, count);
    if (err != HWC2_ERROR_NONE || count == 0)
        return;

    std::vector<hwc2_config_t> configs(count);
    uint32_t returned = count;
    err = d->fns.getDisplayConfigs(d->real, kSecondaryDisplay, &returned, configs.data());
    ALOGI("real display 1 probe: fill err=%d count=%u first=%u", err, returned,
          returned ? configs[0] : 0);
    if (err != HWC2_ERROR_NONE || returned == 0)
        return;

    if (d->fns.getDisplayType) {
        int32_t type = -1;
        int32_t type_err = d->fns.getDisplayType(d->real, kSecondaryDisplay, &type);
        ALOGI("real display 1 probe: type err=%d type=%d", type_err, type);
    }
    if (d->fns.getActiveConfig) {
        hwc2_config_t active = 0;
        int32_t active_err = d->fns.getActiveConfig(d->real, kSecondaryDisplay, &active);
        ALOGI("real display 1 probe: active err=%d config=%u", active_err, active);
    }
}

static int32_t SecCreateLayer(Device* d, hwc2_layer_t* out_layer) {
    std::lock_guard<std::mutex> sc(d->sec.lock);
    hwc2_layer_t id = d->sec.next_layer++;
    d->sec.layers[id] = SecLayer{};
    d->sec.validated = false;
    *out_layer = id;
    return HWC2_ERROR_NONE;
}

static int32_t SecDestroyLayer(Device* d, hwc2_layer_t layer) {
    std::lock_guard<std::mutex> sc(d->sec.lock);
    d->sec.layers.erase(layer);
    d->sec.validated = false;
    return HWC2_ERROR_NONE;
}

static int32_t SecValidate(Device* d, uint32_t* out_num_types, uint32_t* out_num_requests) {
    std::lock_guard<std::mutex> sc(d->sec.lock);
    uint32_t changes = 0;
    for (auto& kv : d->sec.layers) {
        auto& L = kv.second;
        const int32_t want = HWC2_COMPOSITION_CLIENT;
        L.changed = (L.requested != want);
        L.validated = want;
        if (L.changed)
            changes++;
    }
    d->sec.validated = true;
    if (out_num_types)
        *out_num_types = changes;
    if (out_num_requests)
        *out_num_requests = 0;
    return changes ? HWC2_ERROR_HAS_CHANGES : HWC2_ERROR_NONE;
}

static int32_t SecGetChanged(Device* d, uint32_t* out_count, hwc2_layer_t* out_layers,
                             int32_t* out_types) {
    std::lock_guard<std::mutex> sc(d->sec.lock);
    uint32_t need = 0;
    for (auto& kv : d->sec.layers) {
        if (kv.second.changed)
            need++;
    }
    if (!out_layers || !out_types) {
        if (out_count)
            *out_count = need;
        return HWC2_ERROR_NONE;
    }
    if (*out_count < need) {
        *out_count = need;
        return HWC2_ERROR_NONE;
    }
    uint32_t i = 0;
    for (auto& kv : d->sec.layers) {
        if (!kv.second.changed)
            continue;
        out_layers[i] = kv.first;
        out_types[i] = kv.second.validated;
        i++;
    }
    *out_count = i;
    return HWC2_ERROR_NONE;
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

static void* TryGrallocRgbDataAddress(buffer_handle_t handle) {
    const hw_module_t* module = nullptr;
    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module) != 0 || !module)
        return nullptr;
    const auto* g = reinterpret_cast<const gralloc_module_t*>(module);
    if (!g->perform)
        return nullptr;
    void* rgb = nullptr;
    if (g->perform(const_cast<gralloc_module_t*>(g), GRALLOC_MODULE_PERFORM_GET_RGB_DATA_ADDRESS,
                   handle, &rgb) == 0 &&
        rgb != nullptr)
        return rgb;
    return nullptr;
}

static bool MapperLockCpu(Device* d, buffer_handle_t handle, int w, int h, void** out_vaddr) {
    if (d->mapper == nullptr)
        d->mapper = IMapper::getService();
    if (d->mapper == nullptr)
        return false;

    const uint64_t usages[] = {
            static_cast<uint64_t>(BufferUsage::CPU_READ_OFTEN),
            static_cast<uint64_t>(BufferUsage::CPU_READ_OFTEN) |
                    static_cast<uint64_t>(BufferUsage::GPU_TEXTURE),
            static_cast<uint64_t>(BufferUsage::CPU_READ_RARELY),
    };
    const IMapper::Rect rects[] = {
            IMapper::Rect{0, 0, w, h},
    };

    for (uint64_t usage : usages) {
        for (const auto& rect : rects) {
            void* vaddr = nullptr;
            Error err = Error::NONE;
            d->mapper->lock(const_cast<native_handle_t*>(handle), usage, rect, hidl_handle(),
                            [&](const auto e, void* ptr) {
                                err = e;
                                vaddr = ptr;
                            });
            if (err == Error::NONE && vaddr != nullptr) {
                *out_vaddr = vaddr;
                return true;
            }
        }
    }
    return false;
}

static void __attribute__((unused)) CopyRgbaToFb(Device* d, const uint8_t* src, int stride_px,
                                                 int w, int h, int format) {
    const size_t src_stride_bytes = static_cast<size_t>(stride_px) * 4;
    const size_t dst_stride_bytes =
            d->finfo.line_length ? d->finfo.line_length : static_cast<size_t>(FUJISAN_SEC_WIDTH) * 4;
    const size_t copy_w = static_cast<size_t>(w) * 4;
    auto* dst = static_cast<uint8_t*>(d->fb_map);

    /* mdss fb often scans out as BGRA8888 (red offset 16). SF client target is RGBA. */
    const bool fb_bgra = (d->vinfo.bits_per_pixel == 32 && d->vinfo.red.offset == 16);
    const bool src_rgba = (format == HAL_PIXEL_FORMAT_RGBA_8888 ||
                           format == HAL_PIXEL_FORMAT_RGBX_8888 || format == 1);
    const bool src_bgra = (format == HAL_PIXEL_FORMAT_BGRA_8888 || format == 5);
    const bool swizzle = fb_bgra ? src_rgba : src_bgra;

    for (int y = 0; y < h; y++) {
        const uint8_t* srow = src + static_cast<size_t>(y) * src_stride_bytes;
        uint8_t* drow = dst + static_cast<size_t>(y) * dst_stride_bytes;
        if (!swizzle) {
            memcpy(drow, srow, copy_w);
        } else {
            for (int x = 0; x < w; x++) {
                const uint8_t* sp = srow + static_cast<size_t>(x) * 4;
                uint8_t* dp = drow + static_cast<size_t>(x) * 4;
                dp[0] = sp[2];
                dp[1] = sp[1];
                dp[2] = sp[0];
                dp[3] = sp[3];
            }
        }
        if (dst_stride_bytes > copy_w)
            memset(drow + copy_w, 0, dst_stride_bytes - copy_w);
    }
    msync(d->fb_map, d->fb_map_size, MS_SYNC);
}

/*
 * fb1 is a real MDSS panel.  Its contents must be submitted as an MDP overlay
 * using the client target dma-buf.  Writing its fbdev mmap then pan_display
 * allocates transient base pipes, which collides with the primary HWC and
 * eventually exhausts all SSPPs.
 */
static bool PostHandleOverlay(Device* d, int fb_fd, uint32_t* overlay_id,
                              buffer_handle_t handle, int src_x, const char* panel) {
    if (!handle || fb_fd < 0 || !overlay_id)
        return false;

    int stride_px = FUJISAN_SEC_WIDTH;
    int w = FUJISAN_SEC_WIDTH;
    int h = FUJISAN_SEC_HEIGHT;
    int format = HAL_PIXEL_FORMAT_RGBA_8888;
    int flags = 0;
    GetGrallocStridePx(handle, &stride_px, &w, &h, &format, &flags);

    if (src_x < 0 || src_x >= w)
        return false;
    w = std::min(FUJISAN_SEC_WIDTH, w - src_x);
    if (h > FUJISAN_SEC_HEIGHT)
        h = FUJISAN_SEC_HEIGHT;
    if (stride_px < src_x + w)
        stride_px = src_x + w;

    const auto* gralloc = reinterpret_cast<const FujisanPrivateHandle*>(handle);
    if (gralloc->magic != kFujisanGrallocMagic || gralloc->fd < 0) {
        ALOGE("invalid fb1 client target handle magic=0x%x fd=%d", gralloc->magic, gralloc->fd);
        return false;
    }

    const bool ubwc = (flags & PRIV_FLAGS_UBWC_ALIGNED) != 0;
    /* The msm8996 gralloc used by the vendor composer can retain the
     * RGBA_8888 format field when SurfaceFlinger resizes a folded 1080-wide
     * client target to 2160-wide.  Its allocation is nevertheless a 16-bpp
     * RGB565 buffer (about half the RGBA size).  Passing the stale format to
     * MDSS makes the driver demand twice the dma-buf size and reject B's
     * overlay.  Trust the allocation extent in that transition. */
    const uint64_t rgba_bytes = static_cast<uint64_t>(stride_px) * h * 4;
    const bool compact_rgb565 =
        gralloc->size > 0 && static_cast<uint64_t>(gralloc->size) < rgba_bytes;
    const bool rgb565 = format == HAL_PIXEL_FORMAT_RGB_565 || compact_rgb565;
    const uint32_t mdp_format = rgb565 ? MDP_RGB_565
                                       : (ubwc ? MDP_RGBA_8888_UBWC : MDP_RGBA_8888);

    /* MDP's OVERLAY_PLAY only imports a dma-buf; it does not update the
     * geometry or format selected by the earlier OVERLAY_SET.  On a cold
     * boot while folded, SF first gives B a narrow client target and then a
     * compact RGB565 wide target on the first open.  Reuse of the narrow
     * RGBA overlay makes MDSS validate the latter as a 16.8 MiB buffer even
     * though the allocation is 8.4 MiB.  Retire just B's pipe when its
     * immutable overlay contract changes; this is internal to HWC and does
     * not disconnect/recreate the primary display. */
    const bool is_secondary = overlay_id == &d->sec_overlay_id;
    const bool secondary_contract_changed =
        is_secondary && *overlay_id != MSMFB_NEW_REQUEST &&
        (d->sec_overlay_src_width != static_cast<uint32_t>(stride_px) ||
         d->sec_overlay_src_height != static_cast<uint32_t>(h) ||
         d->sec_overlay_src_format != mdp_format ||
         d->sec_overlay_crop_x != src_x ||
         d->sec_overlay_crop_width != static_cast<uint32_t>(w));
    if (secondary_contract_changed) {
        uint32_t id = *overlay_id;
        if (ioctl(fb_fd, MSMFB_OVERLAY_UNSET, &id) != 0)
            ALOGW("%s overlay reset id=0x%x failed: %s", panel, id, strerror(errno));
        *overlay_id = MSMFB_NEW_REQUEST;
        d->sec_overlay_src_width = 0;
        d->sec_overlay_src_height = 0;
        d->sec_overlay_src_format = 0;
        d->sec_overlay_crop_x = -1;
        d->sec_overlay_crop_width = 0;
        ALOGI("%s overlay contract changed; recreating", panel);
    }

    if (*overlay_id == MSMFB_NEW_REQUEST) {
        mdp_overlay overlay {};
        overlay.src.width = static_cast<uint32_t>(stride_px);
        overlay.src.height = static_cast<uint32_t>(h);
        /* The MDSS legacy overlay API imports the complete gralloc dma-buf,
         * including UBWC metadata, so preserve the producer's real layout.
         * SurfaceFlinger may choose RGB_565 for the 2160-wide client target;
         * treating that 2-byte buffer as RGBA makes MDSS request twice the
         * available dma-buf size and leaves panel B without a valid frame. */
        overlay.src.format = mdp_format;
        overlay.src_rect = {static_cast<uint32_t>(src_x), 0,
                            static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
        overlay.dst_rect = {0, 0, static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
        overlay.z_order = 0;
        overlay.is_fg = 1;
        overlay.alpha = MDP_ALPHA_NOP;
        overlay.blend_op = BLEND_OP_OPAQUE;
        overlay.transp_mask = MDP_TRANSP_NOP;
        /* Let MDSS choose a pipe compatible with panel B and the imported
         * UBWC target.  fbdev's forced-DMA base-pipe path is exactly what
         * exhausted the SSPP pool in the previous implementation. */
        overlay.pipe_type = PIPE_TYPE_AUTO;
        overlay.id = MSMFB_NEW_REQUEST;
        if (ioctl(fb_fd, MSMFB_OVERLAY_SET, &overlay) != 0) {
            ALOGE("%s overlay set failed: %s", panel, strerror(errno));
            return false;
        }
        *overlay_id = overlay.id;
        if (is_secondary) {
            d->sec_overlay_src_width = static_cast<uint32_t>(stride_px);
            d->sec_overlay_src_height = static_cast<uint32_t>(h);
            d->sec_overlay_src_format = mdp_format;
            d->sec_overlay_crop_x = src_x;
            d->sec_overlay_crop_width = static_cast<uint32_t>(w);
        }
        ALOGI("%s overlay configured id=0x%x src=%dx%d crop_x=%d dst=%dx%d fmt=%d size=%u rgb565=%d ubwc=%d", panel,
              overlay.id, stride_px, h, src_x, w, h, format, gralloc->size,
              rgb565 ? 1 : 0, ubwc ? 1 : 0);
    }

    msmfb_overlay_data post {};
    post.id = *overlay_id;
    post.data.memory_id = gralloc->fd;
    post.data.offset = gralloc->offset;
    if (ioctl(fb_fd, MSMFB_OVERLAY_PLAY, &post) != 0) {
        const int err = errno;
        ALOGE("%s overlay play id=0x%x failed: %s", panel, post.id, strerror(err));
        /* fb1 can be blanked/re-enabled by the display power path after the
         * logical display is attached.  MDSS then drops its legacy pipes;
         * recreate this one on the next client-target frame. */
        if (err == ENODEV || err == EPERM)
            *overlay_id = MSMFB_NEW_REQUEST;
        return false;
    }

    /* Commit with the full mdp_display_commit ABI.  Do not confuse this with
     * the tiny pointer-sized ioctl used by the standalone fb fill probe: the
     * latter is rejected, while this HWC structure is the one accepted by
     * the legacy MDSS overlay path. */
    MdpDisplayCommit commit {};
    commit.flags = MDP_DISPLAY_COMMIT_OVERLAY;
    commit.wait_for_finish = 0;
    if (ioctl(fb_fd, MSMFB_DISPLAY_COMMIT, &commit) != 0) {
        ALOGE("%s overlay commit failed: %s", panel, strerror(errno));
        return false;
    }

    static int once = 0;
    if (once++ < 5) {
        ALOGI("%s overlay post id=0x%x %dx%d crop_x=%d stride=%d fmt=%d flags=0x%x ubwc=%d",
              panel, *overlay_id, w, h, src_x, stride_px, format, flags, ubwc ? 1 : 0);
    }
    return true;
}

static bool PostHandleOverlayToFb1(Device* d, buffer_handle_t handle) {
    if (!OpenFb1(d))
        return false;
    return PostHandleOverlay(d, d->fb_fd, &d->sec_overlay_id, handle, 0, "fb1");
}

/* The 2160-wide client target is one dma-buf.  Import it into both real
 * panels with cropped source rectangles rather than copying to fbdev and
 * calling PAN_DISPLAY: the latter collides with MDSS/HWC base-pipe ownership. */
static bool __attribute__((unused)) PostZoomOverlays(Device* d, buffer_handle_t handle, int fence) {
    if (!handle)
        return false;
    if (fence >= 0) {
        (void)sync_wait(fence, 1000);
        close(fence);
    }
    if (!OpenFb0(d) || !OpenFb1(d))
        return false;
    const bool a = PostHandleOverlay(d, d->fb0_fd, &d->pri_overlay_id, handle, 0, "fb0");
    const bool b = PostHandleOverlay(d, d->fb_fd, &d->sec_overlay_id, handle,
                                     FUJISAN_SEC_WIDTH, "fb1");
    return a && b;
}

/* A is always composed by the vendor Qualcomm HWC.  In zoom it consumes the
 * full client target through its native primary path; B receives only the
 * right 1080-pixel crop through the working legacy overlay path. */
static bool PostZoomSecondaryOverlay(Device* d, buffer_handle_t handle, int fence) {
    if (!handle)
        return false;
    if (fence >= 0) {
        (void)sync_wait(fence, 1000);
        close(fence);
    }
    if (!OpenFb1(d))
        return false;
    const bool posted = PostHandleOverlay(d, d->fb_fd, &d->sec_overlay_id, handle,
                                          FUJISAN_SEC_WIDTH, "fb1");
    return posted;
}


/* Post full 1080 client target to fb1 (closed_b primary or zoom interim). */
static bool PostClientToFb1(Device* d, buffer_handle_t handle, int fence) {
    if (!handle)
        return false;
    if (fence >= 0) {
        (void)sync_wait(fence, 1000);
        close(fence);
    }
    if (!PostHandleOverlayToFb1(d, handle))
        return false;
    /* Panel B power and brightness are owned by fujisan_halld / Lights.
     * In particular, never restore a hard-coded brightness while posting a
     * composition frame: a touch can cause a present at any time. */
    return true;
}

/* Split 2160x1920 client target: left -> fb0, right -> fb1 (1px hinge at x=1080 skipped into B col0). */
static bool __attribute__((unused)) CopyZoomSplit(Device* d, buffer_handle_t handle, int fence) {
    if (!handle)
        return false;
    if (fence >= 0) {
        (void)sync_wait(fence, 1000);
        close(fence);
    }
    if (!OpenFb0(d) || !OpenFb1(d) || d->fb0_map == MAP_FAILED || d->fb_map == MAP_FAILED)
        return false;

    int stride_px = kZoomWidth;
    int w = kZoomWidth;
    int h = FUJISAN_SEC_HEIGHT;
    int format = HAL_PIXEL_FORMAT_RGBA_8888;
    int flags = 0;
    GetGrallocStridePx(handle, &stride_px, &w, &h, &format, &flags);
    if (h > FUJISAN_SEC_HEIGHT)
        h = FUJISAN_SEC_HEIGHT;
    if (stride_px < w)
        stride_px = w;

    void* vaddr = TryGrallocRgbDataAddress(handle);
    bool locked = false;
    if (vaddr == nullptr) {
        locked = MapperLockCpu(d, handle, stride_px, h, &vaddr);
        if (!locked)
            locked = MapperLockCpu(d, handle, w, h, &vaddr);
    }
    if (vaddr == nullptr) {
        ALOGE("zoom split: no CPU base %dx%d stride=%d flags=0x%x", w, h, stride_px, flags);
        return false;
    }

    const uint8_t* src = static_cast<const uint8_t*>(vaddr);
    const size_t src_stride = static_cast<size_t>(stride_px) * 4;
    const size_t dst0_stride = d->finfo0.line_length ? d->finfo0.line_length
                                                     : static_cast<size_t>(FUJISAN_SEC_WIDTH) * 4;
    const size_t dst1_stride = d->finfo.line_length ? d->finfo.line_length
                                                    : static_cast<size_t>(FUJISAN_SEC_WIDTH) * 4;
    auto* dst0 = static_cast<uint8_t*>(d->fb0_map);
    auto* dst1 = static_cast<uint8_t*>(d->fb_map);
    const int left_w = FUJISAN_SEC_WIDTH;
    const int right_w = FUJISAN_SEC_WIDTH;
    const int src_right_x = (w >= kZoomWidth) ? FUJISAN_SEC_WIDTH : 0; /* if only 1080, mirror */

    for (int y = 0; y < h; y++) {
        const uint8_t* srow = src + static_cast<size_t>(y) * src_stride;
        uint8_t* d0 = dst0 + static_cast<size_t>(y) * dst0_stride;
        uint8_t* d1 = dst1 + static_cast<size_t>(y) * dst1_stride;
        memcpy(d0, srow, static_cast<size_t>(left_w) * 4);
        if (w >= kZoomWidth) {
            memcpy(d1, srow + static_cast<size_t>(src_right_x) * 4, static_cast<size_t>(right_w) * 4);
        } else {
            memcpy(d1, srow, static_cast<size_t>(right_w) * 4);
        }
    }
    msync(d->fb0_map, d->fb0_map_size, MS_SYNC);
    msync(d->fb_map, d->fb_map_size, MS_SYNC);

    if (locked && d->mapper != nullptr) {
        Error err = Error::NONE;
        d->mapper->unlock(const_cast<native_handle_t*>(handle),
                          [&](const auto e, const auto&) { err = e; });
        (void)err;
    }

    d->vinfo0.xoffset = 0;
    d->vinfo0.yoffset = 0;
    d->vinfo0.activate = FB_ACTIVATE_VBL;
    KickFb(d->fb0_fd, &d->vinfo0);
    d->vinfo.xoffset = 0;
    d->vinfo.yoffset = 0;
    d->vinfo.activate = FB_ACTIVATE_VBL;
    KickFb(d->fb_fd, &d->vinfo);
    ioctl(d->fb0_fd, FBIOBLANK, FB_BLANK_UNBLANK);
    ioctl(d->fb_fd, FBIOBLANK, FB_BLANK_UNBLANK);
    static int once = 0;
    if (once++ < 8)
        ALOGI("zoom split post src=%dx%d stride=%d", w, h, stride_px);
    return true;
}

static int32_t __attribute__((unused)) SecPresent(Device* d, int32_t* out_retire) {
    buffer_handle_t target = nullptr;
    int fence = -1;
    {
        std::lock_guard<std::mutex> sc(d->sec.lock);
        if (!d->sec.validated)
            return HWC2_ERROR_NOT_VALIDATED;
        if (!d->sec.power_on) {
            if (out_retire)
                *out_retire = -1;
            return HWC2_ERROR_NONE;
        }
        target = d->sec.client_target;
        fence = d->sec.client_acquire_fence;
        d->sec.client_acquire_fence = -1;
        for (auto& kv : d->sec.layers)
            kv.second.changed = false;
    }
    if (fence >= 0) {
        /* GPU client-target must finish before CPU post to fb1. */
        if (sync_wait(fence, 1000) != 0)
            ALOGW("client target fence wait failed/timeout");
        close(fence);
    }
    /* halld blanks fb1 while folded.  Do not create/play an overlay against a
     * powered-down MDSS panel; it fails with EPERM and leaves no valid pipe
     * to receive the first frame after the next hinge-open. */
    ResetSecondaryOverlayIfNeeded(d);
    if (target && SecondaryPanelAvailable())
        PostHandleOverlayToFb1(d, target);
    if (out_retire)
        *out_retire = -1;
    return HWC2_ERROR_NONE;
}

static void WrapperGetCapabilities(struct hwc2_device* device, uint32_t* out_count,
                                   int32_t* out_capabilities) {
    auto* d = ToDev(device);
    d->real->getCapabilities(d->real, out_count, out_capabilities);
}

static void HotplugTrampoline(hwc2_callback_data_t cb_data, hwc2_display_t display,
                              int32_t connected) {
    auto* dev = reinterpret_cast<Device*>(cb_data);
    HWC2_PFN_HOTPLUG fn = nullptr;
    hwc2_callback_data_t user = nullptr;
    {
        std::lock_guard<std::mutex> cl(dev->cb_lock);
        fn = dev->hotplug_fn;
        user = dev->hotplug_data;
    }
    if (fn && display != kSecondaryDisplay)
        fn(user, display, connected);
    if (display == kPrimaryDisplay && connected == HWC2_CONNECTION_CONNECTED)
        ScheduleSecondaryTopology(dev);
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
                EnsureVsyncThread(d);
                break;
            case HWC2_CALLBACK_VSYNC_2_4:
                d->vsync24_data = data;
                d->vsync24_fn = reinterpret_cast<HWC2_PFN_VSYNC_2_4>(pointer);
                EnsureVsyncThread(d);
                break;
            case HWC2_CALLBACK_REFRESH:
                d->refresh_data = data;
                d->refresh_fn = reinterpret_cast<HWC2_PFN_REFRESH>(pointer);
                break;
            case HWC2_CALLBACK_VSYNC_PERIOD_TIMING_CHANGED:
            case HWC2_CALLBACK_SEAMLESS_POSSIBLE:
                /* Optional 2.4 callbacks; accept no-op. */
                return HWC2_ERROR_NONE;
            default:
                break;
        }
    }

    if (descriptor == HWC2_CALLBACK_HOTPLUG) {
        int32_t err =
            d->fns.registerCallback
                ? d->fns.registerCallback(d->real, descriptor, d,
                                         reinterpret_cast<hwc2_function_pointer_t>(HotplugTrampoline))
                : HWC2_ERROR_UNSUPPORTED;
        // Also reconcile in case primary hotplug already fired.
        if (err == HWC2_ERROR_NONE)
            ScheduleSecondaryTopology(d);
        return err;
    }

    if (descriptor == HWC2_CALLBACK_VSYNC || descriptor == HWC2_CALLBACK_VSYNC_2_4) {
        /* Always bridge primary legacy VSYNC -> optional VSYNC / VSYNC_2_4. */
        return WirePrimaryVsync(d);
    }

    if (descriptor == HWC2_CALLBACK_REFRESH) {
        return d->fns.registerCallback ? d->fns.registerCallback(d->real, descriptor, data, pointer)
                                       : HWC2_ERROR_UNSUPPORTED;
    }

    return d->fns.registerCallback ? d->fns.registerCallback(d->real, descriptor, data, pointer)
                                  : HWC2_ERROR_UNSUPPORTED;
}

static int32_t AcceptDisplayChanges(hwc2_device_t* device, hwc2_display_t display) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay) {
        std::lock_guard<std::mutex> sc(d->sec.lock);
        for (auto& kv : d->sec.layers)
            kv.second.changed = false;
        return HWC2_ERROR_NONE;
    }
    return d->fns.acceptDisplayChanges ? d->fns.acceptDisplayChanges(d->real, display)
                                      : HWC2_ERROR_UNSUPPORTED;
}

static int32_t CreateLayer(hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t* out) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay)
        return SecCreateLayer(d, out);
    return d->fns.createLayer ? d->fns.createLayer(d->real, display, out) : HWC2_ERROR_UNSUPPORTED;
}

static int32_t DestroyLayer(hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay)
        return SecDestroyLayer(d, layer);
    return d->fns.destroyLayer ? d->fns.destroyLayer(d->real, display, layer)
                              : HWC2_ERROR_UNSUPPORTED;
}

static int32_t GetActiveConfig(hwc2_device_t* device, hwc2_display_t display, hwc2_config_t* out) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay) {
        if (!out)
            return HWC2_ERROR_BAD_PARAMETER;
        *out = kSecondaryConfig;
        return HWC2_ERROR_NONE;
    }
    if (display == kPrimaryDisplay) {
        std::lock_guard<std::mutex> zl(d->zoom_lock);
        if (WantZoomMode())
            d->active_config = kZoomConfig;
        *out = d->active_config;
        return HWC2_ERROR_NONE;
    }
    return d->fns.getActiveConfig ? d->fns.getActiveConfig(d->real, display, out)
                                 : HWC2_ERROR_UNSUPPORTED;
}

static int32_t GetChangedCompositionTypes(hwc2_device_t* device, hwc2_display_t display,
                                          uint32_t* out_count, hwc2_layer_t* out_layers,
                                          int32_t* out_types) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay)
        return SecGetChanged(d, out_count, out_layers, out_types);
    return d->fns.getChangedCompositionTypes
               ? d->fns.getChangedCompositionTypes(d->real, display, out_count, out_layers, out_types)
               : HWC2_ERROR_UNSUPPORTED;
}

static int32_t GetClientTargetSupport(hwc2_device_t* device, hwc2_display_t display, uint32_t width,
                                      uint32_t height, int32_t format, int32_t dataspace) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay) {
        /* Independent INTERNAL panel B is CPU-posted. Accept common 32bpp linear formats. */
        (void)width;
        (void)height;
        (void)dataspace;
        if (format == HAL_PIXEL_FORMAT_RGBA_8888 || format == HAL_PIXEL_FORMAT_RGBX_8888 ||
            format == HAL_PIXEL_FORMAT_BGRA_8888)
            return HWC2_ERROR_NONE;
        return HWC2_ERROR_UNSUPPORTED;
    }
    if (display == kPrimaryDisplay && WantZoomMode() && width == kZoomWidth &&
        height == FUJISAN_SEC_HEIGHT &&
        (format == HAL_PIXEL_FORMAT_RGBA_8888 || format == HAL_PIXEL_FORMAT_RGBX_8888 ||
         format == HAL_PIXEL_FORMAT_BGRA_8888)) {
        /* The real Xiaomi-derived composer advertises only its physical
         * 1080-wide limit.  Returning that answer for our virtual 2160-wide
         * config makes SurfaceFlinger allocate a 1080 target and scale the
         * whole desktop into it, leaving no distinct right half for panel B.
         * We have a working MDP crop path for a 2160 RGBA client target, so
         * advertise that capability at the wrapper boundary. */
        return HWC2_ERROR_NONE;
    }
    return d->fns.getClientTargetSupport
               ? d->fns.getClientTargetSupport(d->real, display, width, height, format, dataspace)
               : HWC2_ERROR_UNSUPPORTED;
}

static int32_t GetColorModes(hwc2_device_t* device, hwc2_display_t display, uint32_t* out_count,
                             int32_t* out_modes) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay) {
        if (!out_modes) {
            *out_count = 1;
            return HWC2_ERROR_NONE;
        }
        if (*out_count >= 1) {
            out_modes[0] = HAL_COLOR_MODE_NATIVE;
            *out_count = 1;
        }
        return HWC2_ERROR_NONE;
    }
    return d->fns.getColorModes ? d->fns.getColorModes(d->real, display, out_count, out_modes)
                               : HWC2_ERROR_UNSUPPORTED;
}

static int32_t GetDisplayAttribute(hwc2_device_t* device, hwc2_display_t display,
                                   hwc2_config_t config, int32_t attribute, int32_t* out) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay) {
        switch (attribute) {
            case HWC2_ATTRIBUTE_WIDTH:
                *out = FUJISAN_SEC_WIDTH;
                return HWC2_ERROR_NONE;
            case HWC2_ATTRIBUTE_HEIGHT:
                *out = FUJISAN_SEC_HEIGHT;
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
    if (display == kPrimaryDisplay && config == kZoomConfig) {
        switch (attribute) {
            case HWC2_ATTRIBUTE_WIDTH:
                *out = kZoomWidth; /* 2160 */
                return HWC2_ERROR_NONE;
            case HWC2_ATTRIBUTE_HEIGHT:
                *out = FUJISAN_SEC_HEIGHT;
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
               ? d->fns.getDisplayAttribute(d->real, display, config, attribute, out)
               : HWC2_ERROR_UNSUPPORTED;
}

static int32_t GetDisplayConfigs(hwc2_device_t* device, hwc2_display_t display, uint32_t* out_count,
                                 hwc2_config_t* out_configs) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay) {
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
        out_configs[0] = kSecondaryConfig;
        *out_count = 1;
        return HWC2_ERROR_NONE;
    }
    if (display == kPrimaryDisplay) {
        /* config0: 1080x1920 single; config1: 2160x1920 open/zoom virtual */
        if (!out_configs) {
            *out_count = 2;
            return HWC2_ERROR_NONE;
        }
        if (*out_count >= 1) out_configs[0] = kSingleConfig;
        if (*out_count >= 2) out_configs[1] = kZoomConfig;
        *out_count = (*out_count >= 2) ? 2 : *out_count;
        return HWC2_ERROR_NONE;
    }
    return d->fns.getDisplayConfigs
               ? d->fns.getDisplayConfigs(d->real, display, out_count, out_configs)
               : HWC2_ERROR_UNSUPPORTED;
}

static int32_t GetDisplayName(hwc2_device_t* device, hwc2_display_t display, uint32_t* out_size,
                              char* out_name) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay) {
        const char* name = "Fujisan Panel B";
        size_t len = strlen(name) + 1;
        if (!out_name) {
            *out_size = static_cast<uint32_t>(len);
            return HWC2_ERROR_NONE;
        }
        if (*out_size == 0)
            return HWC2_ERROR_NONE;
        strncpy(out_name, name, *out_size - 1);
        out_name[*out_size - 1] = 0;
        *out_size = static_cast<uint32_t>(strlen(out_name) + 1);
        return HWC2_ERROR_NONE;
    }
    return d->fns.getDisplayName ? d->fns.getDisplayName(d->real, display, out_size, out_name)
                                : HWC2_ERROR_UNSUPPORTED;
}

static int32_t GetDisplayRequests(hwc2_device_t* device, hwc2_display_t display,
                                  int32_t* out_display_requests, uint32_t* out_num_elements,
                                  hwc2_layer_t* out_layers, int32_t* out_layer_requests) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay) {
        if (out_display_requests)
            *out_display_requests = 0;
        if (out_num_elements)
            *out_num_elements = 0;
        return HWC2_ERROR_NONE;
    }
    return d->fns.getDisplayRequests
               ? d->fns.getDisplayRequests(d->real, display, out_display_requests, out_num_elements,
                                          out_layers, out_layer_requests)
               : HWC2_ERROR_UNSUPPORTED;
}

static int32_t GetDisplayCapabilities(hwc2_device_t* device, hwc2_display_t display,
                                        uint32_t* out_num, uint32_t* out_caps) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay) {
        if (out_num)
            *out_num = 0;
        return HWC2_ERROR_NONE;
    }
    if (d->fns.getDisplayCapabilities)
        return d->fns.getDisplayCapabilities(d->real, display, out_num, out_caps);
    if (out_num)
        *out_num = 0;
    return HWC2_ERROR_NONE;
}

static int32_t SetDisplayBrightness(hwc2_device_t* device, hwc2_display_t display, float brightness) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay)
        return HWC2_ERROR_UNSUPPORTED;
    /* msm8996's composer brightness values are stale after a display
     * reconfiguration and can arrive during arbitrary touch/composition
     * frames.  B is exclusively mirrored from A's actual Lights sysfs write
     * by fujisan_halld's inotify watch. */
    return d->fns.setDisplayBrightness
               ? d->fns.setDisplayBrightness(d->real, display, brightness)
               : HWC2_ERROR_UNSUPPORTED;
}

/* Both built-in panels are INTERNAL. Do not implement GET_DISPLAY_IDENTIFICATION_DATA:
 * SF stays in legacy multi-display mode and keeps secondary as local:1. */
static int32_t GetDisplayConnectionType(hwc2_device_t* device, hwc2_display_t display,
                                        uint32_t* out_type) {
    if (!out_type)
        return HWC2_ERROR_BAD_PARAMETER;
    if (display == kPrimaryDisplay || display == kSecondaryDisplay) {
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
    if (display == kSecondaryDisplay) {
        *out_period = FUJISAN_SEC_VSYNC_NS;
        return HWC2_ERROR_NONE;
    }
    if (d->fns.getDisplayVsyncPeriod)
        return d->fns.getDisplayVsyncPeriod(d->real, display, out_period);
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

    if (display == kPrimaryDisplay) {
        if (config != kSingleConfig && config != kZoomConfig)
            return HWC2_ERROR_BAD_CONFIG;

        /* Android 12 uses this HWC 2.4 entry point rather than the legacy
         * SetActiveConfig callback.  Do not pass virtual config 1 to the
         * Xiaomi-derived real composer: it only owns physical 1080 config 0
         * and quietly returns a narrow client target after a hotplug
         * reprobe.  Keep config 1 visible to SurfaceFlinger while applying
         * config 0 beneath it. */
        bool zoom = false;
        {
            std::lock_guard<std::mutex> zl(d->zoom_lock);
            zoom = (config == kZoomConfig) || WantZoomMode();
            d->zoom_active = zoom;
            d->active_config = zoom ? kZoomConfig : kSingleConfig;
        }

        int32_t err = HWC2_ERROR_UNSUPPORTED;
        if (d->fns.setActiveConfigWithConstraints) {
            err = d->fns.setActiveConfigWithConstraints(d->real, display, kSingleConfig,
                                                        constraints, out_timeline);
        } else if (d->fns.setActiveConfig) {
            err = d->fns.setActiveConfig(d->real, display, kSingleConfig);
        }
        if (err != HWC2_ERROR_NONE)
            return err;

        /* Old composers do not fill the HWC 2.4 timeline when reached
         * through their HWC 2.3 fallback. */
        if (!d->fns.setActiveConfigWithConstraints) {
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            int64_t now = int64_t(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
            int64_t desired = constraints ? constraints->desiredTimeNanos : now;
            if (desired < now)
                desired = now;
            out_timeline->newVsyncAppliedTimeNanos = desired;
            out_timeline->refreshRequired = false;
            out_timeline->refreshTimeNanos = 0;
        }
        ALOGI("SetActiveConfigWithConstraints primary request=%llu -> %s (real=0)",
              static_cast<unsigned long long>(config), zoom ? "ZOOM 2160" : "SINGLE 1080");
        return HWC2_ERROR_NONE;
    }

    int32_t err;
    if (display == kSecondaryDisplay) {
        if (config != kSecondaryConfig)
            return HWC2_ERROR_BAD_CONFIG;
        err = HWC2_ERROR_NONE;
    } else if (d->fns.setActiveConfig) {
        err = d->fns.setActiveConfig(d->real, display, config);
    } else {
        err = HWC2_ERROR_UNSUPPORTED;
    }
    if (err != HWC2_ERROR_NONE)
        return err;

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int64_t now = int64_t(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
    int64_t desired = constraints ? constraints->desiredTimeNanos : now;
    if (desired < now)
        desired = now;
    out_timeline->newVsyncAppliedTimeNanos = desired;
    out_timeline->refreshRequired = false;
    out_timeline->refreshTimeNanos = 0;
    return HWC2_ERROR_NONE;
}

static int32_t GetDisplayType(hwc2_device_t* device, hwc2_display_t display, int32_t* out_type) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay) {
        *out_type = HWC2_DISPLAY_TYPE_PHYSICAL;
        return HWC2_ERROR_NONE;
    }
    return d->fns.getDisplayType ? d->fns.getDisplayType(d->real, display, out_type)
                                : HWC2_ERROR_UNSUPPORTED;
}

static int32_t GetDozeSupport(hwc2_device_t* device, hwc2_display_t display, int32_t* out) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay) {
        *out = 0;
        return HWC2_ERROR_NONE;
    }
    return d->fns.getDozeSupport ? d->fns.getDozeSupport(d->real, display, out)
                                : HWC2_ERROR_UNSUPPORTED;
}

static int32_t GetHdrCapabilities(hwc2_device_t* device, hwc2_display_t display, uint32_t* out_num,
                                  int32_t* types, float* max_l, float* max_avg, float* min_l) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay) {
        if (out_num)
            *out_num = 0;
        return HWC2_ERROR_NONE;
    }
    return d->fns.getHdrCapabilities
               ? d->fns.getHdrCapabilities(d->real, display, out_num, types, max_l, max_avg, min_l)
               : HWC2_ERROR_UNSUPPORTED;
}

static int32_t GetReleaseFences(hwc2_device_t* device, hwc2_display_t display, uint32_t* out_num,
                                hwc2_layer_t* layers, int32_t* fences) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay) {
        if (out_num)
            *out_num = 0;
        return HWC2_ERROR_NONE;
    }
    return d->fns.getReleaseFences
               ? d->fns.getReleaseFences(d->real, display, out_num, layers, fences)
               : HWC2_ERROR_UNSUPPORTED;
}

static int32_t PresentDisplay(hwc2_device_t* device, hwc2_display_t display,
                              int32_t* out_retire_fence) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay)
        return SecPresent(d, out_retire_fence);
    if (display == kPrimaryDisplay) {
        bool zoom = WantZoomMode();
        const bool dual_internal = DualInternalEnabled();
        bool primary_b = WantPrimaryB();
        buffer_handle_t target = nullptr;
        int fence = -1;
        bool refresh_secondary = false;
        bool reprobe_primary = false;
        bool reconfigure_secondary_topology = false;
        {
            std::lock_guard<std::mutex> zl(d->zoom_lock);
            d->zoom_active = zoom;
            d->active_config = zoom ? kZoomConfig : kSingleConfig;
            target = d->client_target;
            fence = d->client_acquire_fence;
            d->client_acquire_fence = -1;

            if (!d->topology_seen || d->dual_internal_active != dual_internal) {
                d->topology_seen = true;
                d->dual_internal_active = dual_internal;
                reconfigure_secondary_topology = true;
            }
            if (dual_internal) {
                const bool available = SecondaryPanelAvailable();
                const int64_t now = MonotonicNs();
                if (available != d->secondary_panel_available) {
                    d->secondary_panel_available = available;
                    d->secondary_panel_available_since_ns = now;
                    d->secondary_panel_refresh_pending = available;
                    ALOGI("independent B panel -> %s", available ? "open" : "closed");
                }
                /* secondary_on() unblanks first and waits 50ms before it
                 * enables backlight.  Give MDSS a little more settling time,
                 * then ask SurfaceFlinger for exactly one fresh B frame. */
                if (d->secondary_panel_refresh_pending && d->secondary_attached.load() &&
                    now - d->secondary_panel_available_since_ns >= 150000000LL) {
                    d->secondary_panel_refresh_pending = false;
                    refresh_secondary = true;
                }
            } else {
                d->secondary_panel_available = false;
                d->secondary_panel_refresh_pending = false;
            }
            if (strcmp(d->last_mode, zoom ? "zoom" : "single") != 0) {
                reprobe_primary = d->mode_seen;
                d->mode_seen = true;
                snprintf(d->last_mode, sizeof(d->last_mode), "%s", zoom ? "zoom" : "single");
                /* B may have been blanked while the old logical mode was
                 * active.  Its legacy pipe must be recreated from the first
                 * target in the new mode, but the primary stays connected. */
                d->secondary_overlay_reset_pending.store(true);
                ALOGI("display mode -> %s (%s)", d->last_mode,
                      reprobe_primary ? "reprobe" : "initial");
                if (d->refresh_fn)
                    d->refresh_fn(d->refresh_data, kPrimaryDisplay);
            } else if (!d->mode_seen) {
                d->mode_seen = true;
            }
        }

        if (reconfigure_secondary_topology) {
            /* Dual <-> zoom must first retire/create B's physical display.
             * Re-enumerating the primary afterwards prevents SurfaceFlinger
             * from ever seeing the old wide target and local:1 together. */
            if (reprobe_primary)
                d->primary_refresh_after_topology.store(true);
            ScheduleSecondaryTopology(d);
        } else if (reprobe_primary) {
            SchedulePrimaryReprobe(d);
        }

        if (refresh_secondary) {
            HWC2_PFN_REFRESH fn = nullptr;
            hwc2_callback_data_t data = nullptr;
            {
                std::lock_guard<std::mutex> cl(d->cb_lock);
                fn = d->refresh_fn;
                data = d->refresh_data;
            }
            if (fn) {
                ALOGI("request B redraw after hinge-open");
                fn(data, kSecondaryDisplay);
            }
        }

        if (zoom && target) {
            int w = 0, h = 0, stride = 0, format = 0, flags = 0;
            GetGrallocStridePx(target, &stride, &w, &h, &format, &flags);
            if (w >= kZoomWidth) {
                /* fb0 belongs to the vendor composer.  Let it present the
                 * primary target and feed B only the right-side crop. */
                int32_t ret = d->fns.presentDisplay
                                      ? d->fns.presentDisplay(d->real, display, out_retire_fence)
                                      : HWC2_ERROR_UNSUPPORTED;
                ResetSecondaryOverlayIfNeeded(d);
                (void)PostZoomSecondaryOverlay(d, target, fence);
                return ret;
            }
            /* Still 1080 while zoom prop is set: present on A, mirror to B until SF switches. */
            int32_t ret = d->fns.presentDisplay
                                  ? d->fns.presentDisplay(d->real, display, out_retire_fence)
                                  : HWC2_ERROR_UNSUPPORTED;
            ResetSecondaryOverlayIfNeeded(d);
            (void)PostClientToFb1(d, target, fence);
            return ret;
        }

        int32_t ret = d->fns.presentDisplay ? d->fns.presentDisplay(d->real, display, out_retire_fence)
                                            : HWC2_ERROR_UNSUPPORTED;
        /* Single mode: never post to fb1 (broken UBWC copy left black B backlight). */
        if (fence >= 0)
            close(fence);
        (void)primary_b;
        (void)target;
        return ret;
    }
    return d->fns.presentDisplay ? d->fns.presentDisplay(d->real, display, out_retire_fence)
                                : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetActiveConfig(hwc2_device_t* device, hwc2_display_t display, hwc2_config_t config) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay)
        return HWC2_ERROR_BAD_DISPLAY;
    if (display == kPrimaryDisplay) {
        if (config != kSingleConfig && config != kZoomConfig)
            return HWC2_ERROR_BAD_CONFIG;
        std::lock_guard<std::mutex> zl(d->zoom_lock);
        d->active_config = config;
        d->zoom_active = (config == kZoomConfig) || WantZoomMode();
        ALOGI("SetActiveConfig primary -> %s", d->zoom_active ? "ZOOM 2160" : "SINGLE 1080");
        if (config == kSingleConfig && d->fns.setActiveConfig)
            return d->fns.setActiveConfig(d->real, display, 0);
        /* Zoom config is wrapper-only; keep real HWC on config 0. */
        if (d->fns.setActiveConfig)
            (void)d->fns.setActiveConfig(d->real, display, 0);
        return HWC2_ERROR_NONE;
    }
    return d->fns.setActiveConfig ? d->fns.setActiveConfig(d->real, display, config)
                                 : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetClientTarget(hwc2_device_t* device, hwc2_display_t display, buffer_handle_t target,
                               int32_t acquire_fence, int32_t dataspace, hwc_region_t damage) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay) {
        std::lock_guard<std::mutex> sc(d->sec.lock);
        if (d->sec.client_acquire_fence >= 0)
            close(d->sec.client_acquire_fence);
        d->sec.client_target = target;
        d->sec.client_acquire_fence = acquire_fence;
        (void)dataspace;
        (void)damage;
        return HWC2_ERROR_NONE;
    }
    if (display == kPrimaryDisplay) {
        const bool zoom = WantZoomMode();
        {
            std::lock_guard<std::mutex> zl(d->zoom_lock);
            if (d->client_acquire_fence >= 0)
                close(d->client_acquire_fence);
            d->client_target = target;
            d->client_acquire_fence = acquire_fence;
            d->zoom_client_target = target;
            d->zoom_acquire_fence = -1; /* ownership kept in client_acquire_fence */
            d->zoom_active = zoom;
            d->active_config = zoom ? kZoomConfig : kSingleConfig;
        }
        if (zoom) {
            /* Real msm HWC stays on 1080 config; wrapper owns zoom client target. */
            int w = 0, h = 0, stride = 0, format = 0, flags = 0;
            if (target)
                GetGrallocStridePx(target, &stride, &w, &h, &format, &flags);
            if (w >= kZoomWidth) {
                /* Feed the vendor primary composer as well.  Suppressing its
                 * client target leaves fb0 without any valid scanout pipe. */
                int fence_for_real = -1;
                if (acquire_fence >= 0)
                    fence_for_real = dup(acquire_fence);
                return d->fns.setClientTarget
                               ? d->fns.setClientTarget(d->real, display, target, fence_for_real,
                                                        dataspace, damage)
                               : HWC2_ERROR_UNSUPPORTED;
            }
            /* 1080 interim: still feed real HWC (dup fence). */
            int fence_for_real = -1;
            if (acquire_fence >= 0)
                fence_for_real = dup(acquire_fence);
            return d->fns.setClientTarget
                           ? d->fns.setClientTarget(d->real, display, target, fence_for_real, dataspace,
                                                    damage)
                           : HWC2_ERROR_UNSUPPORTED;
        }
        /* Single: real path owns the fence; we dup for optional fb1 post. */
        int fence_for_real = -1;
        if (acquire_fence >= 0)
            fence_for_real = dup(acquire_fence);
        return d->fns.setClientTarget
                       ? d->fns.setClientTarget(d->real, display, target, fence_for_real, dataspace,
                                                damage)
                       : HWC2_ERROR_UNSUPPORTED;
    }
    return d->fns.setClientTarget
               ? d->fns.setClientTarget(d->real, display, target, acquire_fence, dataspace, damage)
               : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetColorMode(hwc2_device_t* device, hwc2_display_t display, int32_t mode) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay)
        return (mode == HAL_COLOR_MODE_NATIVE) ? HWC2_ERROR_NONE : HWC2_ERROR_UNSUPPORTED;
    return d->fns.setColorMode ? d->fns.setColorMode(d->real, display, mode) : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetColorTransform(hwc2_device_t* device, hwc2_display_t display, const float* m,
                                 int32_t hint) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay)
        return HWC2_ERROR_NONE;
    return d->fns.setColorTransform ? d->fns.setColorTransform(d->real, display, m, hint)
                                   : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetCursorPosition(hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
                                 int32_t x, int32_t y) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay)
        return HWC2_ERROR_NONE;
    return d->fns.setCursorPosition ? d->fns.setCursorPosition(d->real, display, layer, x, y)
                                   : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetLayerBlendMode(hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
                                 int32_t mode) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay)
        return HWC2_ERROR_NONE;
    return d->fns.setLayerBlendMode ? d->fns.setLayerBlendMode(d->real, display, layer, mode)
                                   : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetLayerBuffer(hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
                              buffer_handle_t buffer, int32_t acquire_fence) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay) {
        if (acquire_fence >= 0)
            close(acquire_fence);
        return HWC2_ERROR_NONE;
    }
    return d->fns.setLayerBuffer
               ? d->fns.setLayerBuffer(d->real, display, layer, buffer, acquire_fence)
               : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetLayerColor(hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
                             hwc_color_t color) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay)
        return HWC2_ERROR_NONE;
    return d->fns.setLayerColor ? d->fns.setLayerColor(d->real, display, layer, color)
                               : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetLayerCompositionType(hwc2_device_t* device, hwc2_display_t display,
                                       hwc2_layer_t layer, int32_t type) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay) {
        std::lock_guard<std::mutex> sc(d->sec.lock);
        auto it = d->sec.layers.find(layer);
        if (it == d->sec.layers.end())
            return HWC2_ERROR_BAD_LAYER;
        it->second.requested = type;
        d->sec.validated = false;
        return HWC2_ERROR_NONE;
    }
    return d->fns.setLayerCompositionType
               ? d->fns.setLayerCompositionType(d->real, display, layer, type)
               : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetLayerDataspace(hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
                                 int32_t dataspace) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay)
        return HWC2_ERROR_NONE;
    return d->fns.setLayerDataspace ? d->fns.setLayerDataspace(d->real, display, layer, dataspace)
                                   : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetLayerDisplayFrame(hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
                                    hwc_rect_t frame) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay)
        return HWC2_ERROR_NONE;
    return d->fns.setLayerDisplayFrame
               ? d->fns.setLayerDisplayFrame(d->real, display, layer, frame)
               : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetLayerPlaneAlpha(hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
                                  float alpha) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay)
        return HWC2_ERROR_NONE;
    return d->fns.setLayerPlaneAlpha ? d->fns.setLayerPlaneAlpha(d->real, display, layer, alpha)
                                    : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetLayerSidebandStream(hwc2_device_t* device, hwc2_display_t display,
                                      hwc2_layer_t layer, const native_handle_t* stream) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay)
        return HWC2_ERROR_NONE;
    return d->fns.setLayerSidebandStream
               ? d->fns.setLayerSidebandStream(d->real, display, layer, stream)
               : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetLayerSourceCrop(hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
                                  hwc_frect_t crop) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay)
        return HWC2_ERROR_NONE;
    return d->fns.setLayerSourceCrop ? d->fns.setLayerSourceCrop(d->real, display, layer, crop)
                                    : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetLayerSurfaceDamage(hwc2_device_t* device, hwc2_display_t display,
                                     hwc2_layer_t layer, hwc_region_t damage) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay)
        return HWC2_ERROR_NONE;
    return d->fns.setLayerSurfaceDamage
               ? d->fns.setLayerSurfaceDamage(d->real, display, layer, damage)
               : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetLayerTransform(hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
                                 int32_t transform) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay)
        return HWC2_ERROR_NONE;
    return d->fns.setLayerTransform ? d->fns.setLayerTransform(d->real, display, layer, transform)
                                   : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetLayerVisibleRegion(hwc2_device_t* device, hwc2_display_t display,
                                     hwc2_layer_t layer, hwc_region_t visible) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay)
        return HWC2_ERROR_NONE;
    return d->fns.setLayerVisibleRegion
               ? d->fns.setLayerVisibleRegion(d->real, display, layer, visible)
               : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetLayerZOrder(hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
                              uint32_t z) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay)
        return HWC2_ERROR_NONE;
    return d->fns.setLayerZOrder ? d->fns.setLayerZOrder(d->real, display, layer, z)
                                : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetOutputBuffer(hwc2_device_t* device, hwc2_display_t display, buffer_handle_t buffer,
                               int32_t release_fence) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay) {
        if (release_fence >= 0)
            close(release_fence);
        return HWC2_ERROR_NONE;
    }
    return d->fns.setOutputBuffer ? d->fns.setOutputBuffer(d->real, display, buffer, release_fence)
                                 : HWC2_ERROR_UNSUPPORTED;
}

static int32_t SetPowerMode(hwc2_device_t* device, hwc2_display_t display, int32_t mode) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay) {
        bool on = (mode == HWC2_POWER_MODE_ON);
        {
            std::lock_guard<std::mutex> sc(d->sec.lock);
            d->sec.power_on = on;
        }
        /* Do not touch fb1 rails here.  The logical B display stays ON from
         * Android's perspective, while fujisan_halld is the sole authority
         * for physical power according to the hinge. */
        return HWC2_ERROR_NONE;
    }
    int32_t ret = d->fns.setPowerMode ? d->fns.setPowerMode(d->real, display, mode)
                                      : HWC2_ERROR_UNSUPPORTED;
    if (display == kPrimaryDisplay) {
        const bool on = (mode == HWC2_POWER_MODE_ON);
        SetDisplayPowerProp(on);
        /* Backlight is owned by halld.  Never FBIOBLANK fb1 from composer:
         * idle secondary scanout can block the legacy ioctl for 30 seconds. */
        if (!on) {
            WriteSysfs(kBl2Path, "0");
        }
    }
    return ret;
}

static int32_t SetVsyncEnabled(hwc2_device_t* device, hwc2_display_t display, int32_t enabled) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay) {
        std::lock_guard<std::mutex> sc(d->sec.lock);
        d->sec.vsync_on = (enabled == HWC2_VSYNC_ENABLE);
        EnsureVsyncThread(d);
        return HWC2_ERROR_NONE;
    }
    return d->fns.setVsyncEnabled ? d->fns.setVsyncEnabled(d->real, display, enabled)
                                 : HWC2_ERROR_UNSUPPORTED;
}

static int32_t ValidateDisplay(hwc2_device_t* device, hwc2_display_t display, uint32_t* out_types,
                               uint32_t* out_requests) {
    auto* d = ToDev(device);
    if (display == kSecondaryDisplay)
        return SecValidate(d, out_types, out_requests);
    return d->fns.validateDisplay ? d->fns.validateDisplay(d->real, display, out_types, out_requests)
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
    d->vsync_thread_run.store(false);
    if (d->vsync_thread)
        pthread_join(d->vsync_thread, nullptr);
    CloseFb1(d);
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

    if (DualInternalEnabled())
        property_set("debug.gralloc.gfx_ubwc_disable", "1");

    auto* d = new Device();
    int err = OpenRealComposer(&d->real, &d->real_so);
    if (err) {
        delete d;
        return err;
    }
    LoadRealFns(d);
    ProbeRealSecondary(d);

    d->base.common.tag = HARDWARE_DEVICE_TAG;
    d->base.common.version = HWC_DEVICE_API_VERSION_2_0;
    d->base.common.module = const_cast<hw_module_t*>(module);
    d->base.common.close = HwcClose;
    d->base.getCapabilities = WrapperGetCapabilities;
    d->base.getFunction = WrapperGetFunction;

    *device = &d->base.common;
    ALOGI("Fujisan HWC2 wrapper open (single/zoom hinge path, secondary hotplug off)");
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
