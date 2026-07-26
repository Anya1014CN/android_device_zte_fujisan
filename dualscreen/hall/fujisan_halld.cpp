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
#include <errno.h>
#include <log/log.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/inotify.h>
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

/* m1120 emits SW_LID (and a REL_X edge) through evdev.  Wait on that input
 * queue instead of repeatedly sampling the module parameter.  event numbers
 * are not ABI, so discover the m1120 event node from sysfs at each wait. */
static int open_m1120_event() {
    DIR* inputs = opendir("/sys/class/input");
    if (!inputs)
        return -1;

    int fd = -1;
    struct dirent* input;
    while ((input = readdir(inputs)) != nullptr && fd < 0) {
        if (strncmp(input->d_name, "input", 5) != 0)
            continue;

        char name_path[256];
        snprintf(name_path, sizeof(name_path), "/sys/class/input/%s/name", input->d_name);
        int name_fd = open(name_path, O_RDONLY | O_CLOEXEC);
        if (name_fd < 0)
            continue;
        char name[64] = {};
        (void)read(name_fd, name, sizeof(name) - 1);
        close(name_fd);
        if (strncmp(name, "m1120", 5) != 0)
            continue;

        char input_path[256];
        snprintf(input_path, sizeof(input_path), "/sys/class/input/%s", input->d_name);
        DIR* events = opendir(input_path);
        if (!events)
            continue;
        struct dirent* event;
        while ((event = readdir(events)) != nullptr) {
            if (strncmp(event->d_name, "event", 5) != 0)
                continue;
            char event_path[64];
            snprintf(event_path, sizeof(event_path), "/dev/input/%s", event->d_name);
            fd = open(event_path, O_RDONLY | O_CLOEXEC);
            break;
        }
        closedir(events);
    }
    closedir(inputs);
    return fd;
}

static int open_primary_brightness_watch() {
    const int fd = inotify_init1(IN_CLOEXEC);
    if (fd < 0)
        return -1;
    if (inotify_add_watch(fd, "/sys/class/leds/lcd-backlight/brightness",
                          IN_CLOSE_WRITE | IN_MODIFY) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/* Wait for either a real hinge transition or a system/Lights brightness
 * write.  Both kernel sources block this daemon at zero CPU while idle. */
static void wait_for_hinge_or_brightness_event() {
    for (;;) {
        const int hinge_fd = open_m1120_event();
        const int brightness_fd = open_primary_brightness_watch();
        if (hinge_fd < 0 || brightness_fd < 0) {
            if (hinge_fd >= 0)
                close(hinge_fd);
            if (brightness_fd >= 0)
                close(brightness_fd);
            /* This only retries while the input driver is absent during an
             * abnormal boot; normal operation has no timer wakeups. */
            ALOGW("hinge or brightness event node unavailable; retrying after one second");
            sleep(1);
            continue;
        }

        for (;;) {
            struct pollfd pfds[2] {};
            pfds[0].fd = hinge_fd;
            pfds[0].events = POLLIN;
            pfds[1].fd = brightness_fd;
            pfds[1].events = POLLIN;
            const int rc = poll(pfds, 2, -1);
            if (rc < 0 && errno == EINTR)
                continue;
            if (rc <= 0)
                break;

            if (pfds[1].revents & POLLIN) {
                char events[sizeof(struct inotify_event) + NAME_MAX + 1];
                (void)read(brightness_fd, events, sizeof(events));
                close(hinge_fd);
                close(brightness_fd);
                return;
            }
            if (!(pfds[0].revents & POLLIN))
                break;

            struct input_event event;
            const ssize_t n = read(hinge_fd, &event, sizeof(event));
            if (n != static_cast<ssize_t>(sizeof(event)))
                break;
            if ((event.type == EV_SW && event.code == SW_LID) ||
                event.type == EV_REL) {
                close(hinge_fd);
                close(brightness_fd);
                return;
            }
        }
        close(hinge_fd);
        close(brightness_fd);
    }
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
    /* Never touch fb1/blank here.  Even the apparent "unblank" value 0 can
     * enter mdss_mdp_display_commit and block forever while the secondary
     * overlay is idle, which subsequently wedges SurfaceFlinger.  HWC owns
     * scanout; the hall daemon only controls B's backlight. */
    write_sysfs("/sys/class/leds/lcd-backlight-2/brightness", "0");
    usleep(20 * 1000);
    write_sysfs("/sys/class/leds/lcd-backlight-2/brightness", b);
}

static bool display_is_on() {
    char p[PROPERTY_VALUE_MAX] = "1";
    property_get("vendor.fujisan.display_power", p, "1");
    /* SetPowerMode in our HWC updates this property on every real panel
     * transition.  debug.tracing.screen_state is only tracing state; it can
     * remain OFF after the composer process is recreated even while A is
     * visibly on, which would incorrectly keep B black in dual mode. */
    return p[0] != '0';
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

    /* Android 12's name/unique-id association only filters dispatch; its
     * TouchInputMapper still chooses the first INTERNAL viewport.  A port
     * association reaches the mapper, so B's physical input port selects
     * A(port 0) for zoom or B(port 1) for independent mode. */
    const char* remove_unique[] = {
        "/system/bin/service", "call", "input", "40",
        "s16", "zte-touchscreen-2nd", nullptr,
    };
    if (!run_service_call(remove_unique))
        ALOGW("failed to clear legacy B touch unique-id association");

    const char* associate_port[] = {
        "/system/bin/service", "call", "input", "37",
        "s16", "synaptics_dsx/touch_input_2nd", "i32", zoom ? "0" : "1", nullptr,
    };
    if (!run_service_call(associate_port))
        ALOGW("failed to associate B touch port with %s display", zoom ? "zoom" : "independent");

    if (zoom) {
        /* InputReader stores an affine matrix per display rotation.  The
         * matrix runs before rotateAndScale(), so the same natural-coordinate
         * left/right split is correct for all four rotations. */
        for (int rotation = 0; rotation < 4; ++rotation) {
            set_touch_calibration(kPrimaryTouch, rotation, "0.5", "0");
            set_touch_calibration(kSecondaryTouch, rotation, "0.5", "540");
        }
    } else {
        /* Clear every rotation too, otherwise a folded transition after an
         * orientation change can retain the wide-display matrix. */
        for (int rotation = 0; rotation < 4; ++rotation) {
            set_touch_calibration(kPrimaryTouch, rotation, "1", "0");
            set_touch_calibration(kSecondaryTouch, rotation, "1", "0");
        }
    }
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
        char preferred[PROPERTY_VALUE_MAX] = "a";
        property_get("persist.vendor.fujisan.primary_panel", preferred, "a");
        char force[PROPERTY_VALUE_MAX] = "0";
        property_get("persist.vendor.fujisan.primary_force", force, "0");
        char force_b[PROPERTY_VALUE_MAX] = "0";
        property_get("persist.vendor.fujisan.force_b_on", force_b, "0");
        char force_mode[PROPERTY_VALUE_MAX] = "";
        property_get("persist.vendor.fujisan.force_mode", force_mode, "");
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

        if (force_mode[0] == 'z') {
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
        const bool posture_wants_b = mode[0] == 'z' ||
                                     (mode[0] == 'd' && st != 1) ||
                                     force_b[0] == '1';
        /* SetPowerMode reports OFF while BootAnimation owns the primary
         * surface, even though both physical panels can scan out.  Honor the
         * hinge during that interval so an unfolded boot lights B; once
         * Android is ready, return to the normal power-state policy. */
        const bool want_b = posture_wants_b && (boot_done ? power_on : true);

        const int bl0 = read_int_file("/sys/class/leds/lcd-backlight/brightness", -1);
        int bl1 = read_int_file("/sys/class/leds/lcd-backlight-2/brightness", -1);
        const int want_b_int = want_b ? 1 : 0;
        /* A fold-open boot already has HWC's virtual-wide topology before
         * boot completion.  Light B in that posture so BootAnimation reaches
         * both panels; a folded boot still leaves B untouched until Android
         * is ready.  We only control backlight here, never fb1 blank/rails. */
        if ((boot_done || want_b) &&
            (!boot_panel_reconciled || want_b_int != last_want_b)) {
            if (want_b)
                /* Both panels expose the same 0..255 range.  Bring B up at
                 * the current system brightness.  Before Lights has written
                 * A, use the panel's normal boot brightness as a temporary
                 * value; the existing post-boot mirror takes over later. */
                secondary_on(bl0 > 0 ? bl0 : 87);
            else
                secondary_off();
            last_want_b = want_b_int;
            boot_panel_reconciled = true;
        }
        /* A failed brightness write is safe to retry; unlike FBIOBLANK it
         * does not stall the hall worker or tear down the MDP overlay. */
        if (boot_done && !want_b && bl1 > 0)
            secondary_off();

        /* msm8996 applies system brightness through Lights, bypassing the
         * composer brightness callback.  The primary sysfs attribute emits
         * an inotify event on every write, so mirror only actual changes. */
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
            /* Do not use `wm size` for posture changes.  It persists a
             * forced display size in Settings and makes the next folded boot
             * render BootAnimation as a 2160-wide desktop until this daemon
             * reaches sys.boot_completed.  HWC's active topology is the
             * authoritative logical size. */
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

        /* Blocks at zero CPU until m1120, Lights or init reports a change. */
        wait_for_hinge_or_brightness_event();
    }
    return 0;
}
