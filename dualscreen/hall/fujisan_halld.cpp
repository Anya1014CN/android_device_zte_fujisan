/*
 * Fujisan hall / panel power helper.
 *
 * hall_status: 1=closed A, 2=open(zoom), 3=closed B face
 *
 * Panel power policy (bring-up, stable power key first):
 *   - Always leave panel A to SurfaceFlinger / Lights (never force A BL off).
 *   - B is OFF in single mode (closed_a and closed_b).
 *   - B is ON only for open/zoom (or debug force flags).
 *   - Sleep (screen_state OFF/DOZE): force B off; do not touch A.
 *
 * Content routing for closed_b / real 2160 zoom comes later; do not break power.
 */
#define LOG_TAG "FujisanHalld"
#include <cutils/properties.h>
#include <log/log.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int read_int_file(const char* path, int fallback) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return fallback;
    char buf[64] = {};
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0)
        return fallback;
    int v = fallback;
    if (sscanf(buf, "%d", &v) != 1)
        return fallback;
    return v;
}

static void write_sysfs(const char* path, const char* val) {
    int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0)
        return;
    (void)write(fd, val, strlen(val));
    close(fd);
}

static void enable_m1120() {
    DIR* d = opendir("/sys/class/input");
    if (!d)
        return;
    struct dirent* de;
    while ((de = readdir(d)) != nullptr) {
        if (strncmp(de->d_name, "input", 5) != 0)
            continue;
        char name_path[256];
        snprintf(name_path, sizeof(name_path), "/sys/class/input/%s/name", de->d_name);
        int fd = open(name_path, O_RDONLY | O_CLOEXEC);
        if (fd < 0)
            continue;
        char name[64] = {};
        (void)read(fd, name, sizeof(name) - 1);
        close(fd);
        if (strncmp(name, "m1120", 5) != 0)
            continue;
        char en[256];
        snprintf(en, sizeof(en), "/sys/class/input/%s/enable", de->d_name);
        write_sysfs(en, "1");
    }
    closedir(d);
}

static void secondary_off() {
    /* Do not FBIOBLANK fb1 here.  On this MDSS it waits for a kickoff that
     * never comes once the secondary overlay is idle, blocking this daemon
     * for 30 seconds and resetting B on the next attempted transition.
     * Backlight=0 is sufficient to hide the folded panel. */
    write_sysfs("/sys/class/leds/lcd-backlight-2/brightness", "0");
}

static void secondary_on(int bl) {
    if (bl < 0)
        bl = 0;
    if (bl > 255)
        bl = 255;
    char b[16];
    snprintf(b, sizeof(b), "%d", bl);
    /* B may still be blanked by the initial fbdev setup.  Unblanking (0) is
     * non-blocking; the dangerous operation is blanking (4), which waits for
     * an idle kickoff and must never be issued from this daemon.  Pulse the
     * backlight afterwards so a panel which came up before MDSS was ready
     * latches the new state. */
    write_sysfs("/sys/class/graphics/fb1/blank", "0");
    write_sysfs("/sys/class/leds/lcd-backlight-2/brightness", "0");
    usleep(20 * 1000);
    write_sysfs("/sys/class/leds/lcd-backlight-2/brightness", b);
}

static bool display_is_on() {
    char p[PROPERTY_VALUE_MAX] = "1";
    property_get("vendor.fujisan.display_power", p, "1");
    if (p[0] == '0')
        return false;

    /* Display.STATE_OFF=1, ON=2, DOZE=3, DOZE_SUSPEND=4, VR=5 */
    char ss[PROPERTY_VALUE_MAX] = "2";
    property_get("debug.tracing.screen_state", ss, "2");
    int state = 2;
    if (sscanf(ss, "%d", &state) == 1) {
        if (state != 2 /* ON */ && state != 5 /* VR */)
            return false;
    }
    return true;
}

/* InputManager keeps touch calibration and display associations independently
 * of HWC.  A zoom posture is one logical 2160px display, while a folded
 * posture is the original 1080px A panel.  Configure both sides before
 * SurfaceFlinger is asked to re-enumerate the display mode. */
static bool run_service_call(const char* const argv[]) {
    pid_t pid = fork();
    if (pid < 0) {
        ALOGE("fork service call failed");
        return false;
    }
    if (pid == 0) {
        int null_fd = open("/dev/null", O_RDWR | O_CLOEXEC);
        if (null_fd >= 0) {
            dup2(null_fd, STDOUT_FILENO);
            dup2(null_fd, STDERR_FILENO);
            if (null_fd > STDERR_FILENO)
                close(null_fd);
        }
        execv(argv[0], const_cast<char* const*>(argv));
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) != pid)
        return false;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static void set_touch_calibration(const char* descriptor, int rotation,
                                  const char* x_scale, const char* x_offset) {
    /* IInputManager#setTouchCalibrationForInputDevice, transaction 11 in
     * Android 12.  The matrix is applied to raw touch coordinates before the
     * InputReader scales them to the active display viewport. */
    const char* argv[] = {
        "/system/bin/service", "call", "input", "11",
        "s16", descriptor, "i32", nullptr, "i32", "1",
        "f", x_scale, "f", "0", "f", x_offset,
        "f", "0", "f", "1", "f", "0", nullptr,
    };
    char rotation_s[4];
    snprintf(rotation_s, sizeof(rotation_s), "%d", rotation);
    argv[7] = rotation_s;
    if (!run_service_call(argv))
        ALOGW("touch calibration service call failed for %s rotation %d",
              descriptor, rotation);
}

static void configure_touch_for_mode(bool zoom) {
    static constexpr const char* kPrimaryTouch =
        "954faadc99bb5a7c1d0537b923e0490c90b47e98";
    static constexpr const char* kSecondaryTouch =
        "b99b5f2fc557ba939628ebbc5b685e1d66f25a78";

    if (zoom) {
        /* IInputManager#addUniqueIdAssociation, transaction 39.  This
         * overrides B's normal local:1 IDC association only while the two
         * physical panels form local:0. */
        const char* associate[] = {
            "/system/bin/service", "call", "input", "39",
            "s16", "zte-touchscreen-2nd", "s16", "local:0", nullptr,
        };
        if (!run_service_call(associate))
            ALOGW("failed to associate B touch with zoom display");
        /* InputReader stores an affine matrix per display rotation.  The
         * matrix runs before rotateAndScale(), so the same natural-coordinate
         * left/right split is correct for all four rotations. */
        for (int rotation = 0; rotation < 4; ++rotation) {
            set_touch_calibration(kPrimaryTouch, rotation, "0.5", "0");
            set_touch_calibration(kSecondaryTouch, rotation, "0.5", "540");
        }
    } else {
        /* IInputManager#removeUniqueIdAssociation, transaction 40. */
        const char* unassociate[] = {
            "/system/bin/service", "call", "input", "40",
            "s16", "zte-touchscreen-2nd", nullptr,
        };
        if (!run_service_call(unassociate))
            ALOGW("failed to restore B touch association");
        /* Clear every rotation too, otherwise a folded transition after an
         * orientation change can retain the wide-display matrix. */
        for (int rotation = 0; rotation < 4; ++rotation) {
            set_touch_calibration(kPrimaryTouch, rotation, "1", "0");
            set_touch_calibration(kSecondaryTouch, rotation, "1", "0");
        }
    }
}

static void configure_logical_display_size(bool zoom) {
    const char* argv[] = {
        "/system/bin/wm", "size", zoom ? "2160x1920" : "1080x1920", nullptr,
    };
    if (!run_service_call(argv))
        ALOGW("failed to set logical display size for %s", zoom ? "zoom" : "single");
}

int main() {
    ALOGI("fujisan_halld start (A-primary stable power; B only for zoom)");
    enable_m1120();
    usleep(100 * 1000);

    int last_st = -1;
    char last_primary[8] = {};
    char last_mode[16] = {};
    int last_power = -1;
    int last_bl1 = -1;
    int last_bl0 = -1;
    int last_want_b = -1;
    bool boot_panel_reconciled = false;
    bool touch_mode_initialized = false;
    char touch_mode[16] = {};

    for (;;) {
        static int tick;
        if ((tick++ % 25) == 0)
            enable_m1120();

        char preferred[PROPERTY_VALUE_MAX] = "a";
        property_get("persist.vendor.fujisan.primary_panel", preferred, "a");
        char force[PROPERTY_VALUE_MAX] = "0";
        property_get("persist.vendor.fujisan.primary_force", force, "0");
        char force_b[PROPERTY_VALUE_MAX] = "0";
        property_get("persist.vendor.fujisan.force_b_on", force_b, "0");
        char force_mode[PROPERTY_VALUE_MAX] = "";
        property_get("persist.vendor.fujisan.force_mode", force_mode, "");
        char dual_internal[PROPERTY_VALUE_MAX] = "0";
        property_get("persist.vendor.fujisan.dual_internal", dual_internal, "0");
        /* The QS tile writes this persistent, vendor-public property once per
         * tap.  Reading it here is a property-area lookup in the daemon's
         * existing hinge loop; it does not spawn a command or add a polling
         * worker.  An unset value preserves the historical persistent default. */
        char user_mode[PROPERTY_VALUE_MAX] = "";
        property_get("persist.vendor.fujisan.user_mode", user_mode, "");
        if (!strcmp(user_mode, "dual"))
            dual_internal[0] = '1';
        else if (!strcmp(user_mode, "zoom"))
            dual_internal[0] = '0';
        char boot_completed[PROPERTY_VALUE_MAX] = "0";
        property_get("sys.boot_completed", boot_completed, "0");
        const bool boot_done = boot_completed[0] == '1';

        int st = read_int_file("/sys/module/ah1898/parameters/hall_status", -1);
        if (st < 0)
            st = read_int_file("/sys/module/mxm1120/parameters/hall_status", 1);
        if (st < 1 || st > 3)
            st = 1;

        char status_s[8];
        snprintf(status_s, sizeof(status_s), "%d", st);
        property_set("vendor.fujisan.hall_status", status_s);

        const char* state = "closed_a";
        const char* mode = "single";
        char primary[8] = "a";

        if (dual_internal[0] == '1') {
            /* Two independent INTERNAL displays.  HWC owns B's client target;
             * keep its rails up instead of entering the single-display zoom path. */
            state = "dual";
            mode = "dual";
        } else if (force_mode[0] == 'z') {
            /* "zoom" selects the virtual-wide display policy; it must not
             * pin it on while the device is physically folded. */
            if (st == 1) {
                state = "closed_a";
                mode = "single";
                snprintf(primary, sizeof(primary), "a");
            } else {
                state = "open";
                mode = "zoom";
                snprintf(primary, sizeof(primary), "%s", preferred[0] == 'b' ? "b" : "a");
            }
        } else if (force_mode[0] == 'a') {
            state = "closed_a";
            mode = "single";
            snprintf(primary, sizeof(primary), "a");
        } else if (force_mode[0] == 'b' && force_mode[1] != 'o') {
            state = "closed_b";
            mode = "single";
            /* Still drive content on A until B path is solid. */
            snprintf(primary, sizeof(primary), "a");
        } else if (force_b[0] == '1') {
            state = "force_b";
            mode = "single";
            snprintf(primary, sizeof(primary), "a");
        } else if (st == 2) {
            state = "open";
            mode = "zoom";
            snprintf(primary, sizeof(primary), "%s", preferred[0] == 'b' ? "b" : "a");
        } else if (st == 3) {
            state = "closed_b";
            mode = "single";
            /* Keep SF/Lights on A. Remember face in props for future primary switch. */
            if (force[0] == '1')
                snprintf(primary, sizeof(primary), "%s", preferred[0] == 'b' ? "b" : "a");
            else
                snprintf(primary, sizeof(primary), "a");
        } else {
            state = "closed_a";
            mode = "single";
            if (force[0] == '1')
                snprintf(primary, sizeof(primary), "%s", preferred[0] == 'b' ? "b" : "a");
            else
                snprintf(primary, sizeof(primary), "a");
        }

        property_set("vendor.fujisan.device_state", state);
        property_set("vendor.fujisan.display_mode", mode);
        property_set("vendor.fujisan.active_primary", primary);

        const bool power_on = display_is_on();
        /* Actual fujisan posture order is A(1) folded, B(2) mid-open,
         * C(3) fully open.  Both 2 and 3 expose the two inside panels; only
         * the folded A posture must turn B off. */
        const bool want_b = power_on &&
                            (mode[0] == 'z' ||
                             (mode[0] == 'd' && st != 1) ||
                             force_b[0] == '1');

        const int bl0 = read_int_file("/sys/class/leds/lcd-backlight/brightness", -1);
        int bl1 = read_int_file("/sys/class/leds/lcd-backlight-2/brightness", -1);
        const int want_b_int = want_b ? 1 : 0;
        /* MDSS may acknowledge an unblank before its initial boot setup is
         * complete, then leave B dark until the next hinge transition.  Do
         * not touch B's rails during that window.  Once Android is fully
         * booted, reconcile the current hinge state exactly once; from then
         * on, only a real B on/off transition writes panel power. */
        if (boot_done &&
            (!boot_panel_reconciled || want_b_int != last_want_b)) {
            if (want_b)
                /* Both panels expose the same 0..255 range.  Bring B up at
                 * the current system brightness, never at a fixed level. */
                secondary_on(bl0 >= 0 ? bl0 : 180);
            else
                secondary_off();
            last_want_b = want_b_int;
            boot_panel_reconciled = true;
        }
        /* A failed brightness write is safe to retry; unlike FBIOBLANK it
         * does not stall the hall worker or tear down the MDP overlay. */
        if (boot_done && !want_b && bl1 > 0)
            secondary_off();

        /* HWC mirrors slider/auto-brightness changes immediately.  This is
         * only a recovery path for legacy composer brightness writes and for
         * B coming online after the main brightness update; it reuses the
         * existing hinge loop and writes only when A actually changed. */
        if (boot_done && want_b && bl0 >= 0 && bl0 != last_bl0) {
            char brightness[16];
            snprintf(brightness, sizeof(brightness), "%d", bl0);
            write_sysfs("/sys/class/leds/lcd-backlight-2/brightness", brightness);
        }
        if (bl0 >= 0)
            last_bl0 = bl0;

        if (boot_done &&
            (!touch_mode_initialized || strcmp(mode, touch_mode) != 0)) {
            const bool was_initialized = touch_mode_initialized;
            configure_touch_for_mode(mode[0] == 'z');
            configure_logical_display_size(mode[0] == 'z');
            snprintf(touch_mode, sizeof(touch_mode), "%s", mode);
            touch_mode_initialized = true;

            /* HWC observes vendor.fujisan.display_mode and asks
             * SurfaceFlinger for a fresh frame/configuration.  Do not restart
             * SurfaceFlinger here: that needlessly plays BootAnimation on
             * every fold transition. */
            if (was_initialized)
                ALOGI("hinge mode %s -> %s: requesting HWC reconfiguration",
                      last_mode, mode);
        }

        bool changed = (st != last_st) || (strcmp(primary, last_primary) != 0) ||
                       (strcmp(mode, last_mode) != 0) || (power_on != (last_power == 1)) ||
                       (bl1 != last_bl1 && (bl1 == 0 || last_bl1 == 0));
        if (changed) {
            ALOGI("hall %d state=%s mode=%s primary=%s power=%d want_b=%d bl1=%d", st, state, mode,
                  primary, power_on ? 1 : 0, want_b ? 1 : 0, bl1);
            last_st = st;
            last_power = power_on ? 1 : 0;
            last_bl1 = bl1;
            snprintf(last_primary, sizeof(last_primary), "%s", primary);
            snprintf(last_mode, sizeof(last_mode), "%s", mode);
        }

        usleep(200 * 1000);
    }
    return 0;
}
