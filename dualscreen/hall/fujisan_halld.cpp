/*
 * Fujisan hall / panel power helper.
 *
 * hall_status: 1=folded A, 2=opening, 3=fully open (zoom)
 *
 * Panel power policy (bring-up, stable power key first):
 *   - Always leave panel A to SurfaceFlinger / Lights (never force A BL off).
 *   - B is OFF only in the folded-A posture.
 *   - B is ON only for open/zoom (or debug force flags).
 *   - Sleep (screen_state OFF/DOZE): force B off; do not touch A.
 *
 * Content routing for closed_b / real 2160 zoom comes later; do not break power.
 */
#define LOG_TAG "FujisanHalld"
#include <cutils/properties.h>
#include <cutils/sockets.h>
#include <errno.h>
#include <log/log.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <time.h>
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

static int read_hall_status() {
    int st = read_int_file("/sys/module/ah1898/parameters/hall_status", -1);
    if (st < 0)
        st = read_int_file("/sys/module/mxm1120/parameters/hall_status", 1);
    return st >= 1 && st <= 3 ? st : 1;
}

static void write_sysfs(const char* path, const char* val) {
    int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0)
        return;
    (void)write(fd, val, strlen(val));
    close(fd);
}

/* property_set() advances the property's serial even when the value has not
 * changed.  The HWC waits on display_mode's serial to notice real posture
 * transitions, so only publish actual geometry changes. */
static void set_property_if_changed(const char* key, const char* value) {
    char current[PROPERTY_VALUE_MAX] = {};
    property_get(key, current, "");
    if (strcmp(current, value) != 0)
        property_set(key, value);
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

/* Brightness mirroring belongs to the paired MDSS backlight transaction.
 * This daemon only waits for a real hinge transition. */
static int open_primary_control_socket() {
    const int fd = android_get_control_socket("fujisan_primary");
    if (fd < 0) {
        ALOGE("missing fujisan_primary control socket");
        return -1;
    }
    const int flags = fcntl(fd, F_GETFL);
    if (flags >= 0)
        (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (listen(fd, 4) != 0) {
        ALOGE("listen on fujisan_primary failed: %s", strerror(errno));
        return -1;
    }
    return fd;
}

static void reply_primary_request(int client, const char* response) {
    if (client >= 0 && response)
        (void)write(client, response, strlen(response));
}

/* The tile only requests a toggle.  The daemon is the authority for the
 * physical state and persistence, so a stale tile cannot switch while open. */
static void handle_primary_request(int server_fd) {
    int client = accept4(server_fd, nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK);
    if (client < 0)
        return;

    struct pollfd pollfd {};
    pollfd.fd = client;
    pollfd.events = POLLIN;
    if (poll(&pollfd, 1, 1000) <= 0 || !(pollfd.revents & POLLIN)) {
        close(client);
        return;
    }
    char request[32] = {};
    const ssize_t n = read(client, request, sizeof(request) - 1);
    if (n <= 0) {
        close(client);
        return;
    }

    char preferred[PROPERTY_VALUE_MAX] = "a";
    property_get("persist.vendor.fujisan.primary_panel", preferred, "a");
    const int hall = read_hall_status();
    char response[32];
    if (strncmp(request, "toggle", 6) == 0) {
        if (hall != 1) {
            snprintf(response, sizeof(response), "unavailable %c %d\n",
                     preferred[0] == 'b' ? 'b' : 'a', hall);
        } else {
            const char next = preferred[0] == 'b' ? 'a' : 'b';
            property_set("persist.vendor.fujisan.primary_panel", next == 'b' ? "b" : "a");
            property_set("persist.vendor.fujisan.primary_force", next == 'b' ? "1" : "0");
            snprintf(response, sizeof(response), "ok %c %d\n", next, hall);
            ALOGI("primary panel request: %c -> %c", preferred[0] == 'b' ? 'b' : 'a', next);
        }
    } else if (request[0] == 's' && request[1] == 'e' && request[2] == 't'
            && (request[3] == ' ' || request[3] == '\t')
            && (request[4] == 'a' || request[4] == 'b')) {
        const char next = request[4];
        if (hall != 1) {
            snprintf(response, sizeof(response), "unavailable %c %d\n",
                     preferred[0] == 'b' ? 'b' : 'a', hall);
        } else {
            property_set("persist.vendor.fujisan.primary_panel", next == 'b' ? "b" : "a");
            property_set("persist.vendor.fujisan.primary_force", next == 'b' ? "1" : "0");
            snprintf(response, sizeof(response), "ok %c %d\n", next, hall);
            ALOGI("primary panel request: %c -> %c", preferred[0] == 'b' ? 'b' : 'a', next);
        }
    } else {
        snprintf(response, sizeof(response), "error protocol\n");
    }
    reply_primary_request(client, response);
    close(client);
}

static void wait_for_hinge_or_primary_request(int control_fd) {
    for (;;) {
        const int hinge_fd = open_m1120_event();
        if (hinge_fd < 0) {
            struct pollfd control {};
            control.fd = control_fd;
            control.events = POLLIN;
            (void)poll(&control, 1, 1000);
            if (control.revents & POLLIN)
                handle_primary_request(control_fd);
            continue;
        }

        for (;;) {
            struct pollfd pfds[2] = {};
            pfds[0].fd = hinge_fd;
            pfds[0].events = POLLIN;
            pfds[1].fd = control_fd;
            pfds[1].events = POLLIN;
            const int rc = poll(pfds, 2, -1);
            if (rc < 0 && errno == EINTR)
                continue;
            if (rc <= 0)
                break;

            if (pfds[1].revents & POLLIN) {
                handle_primary_request(control_fd);
                close(hinge_fd);
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
                return;
            }
        }
        close(hinge_fd);
    }
}

static void set_secondary_display_allowed(bool allowed) {
    write_sysfs("/sys/module/mdss_fb/parameters/fujisan_secondary_display_allowed",
                allowed ? "1" : "0");
}

static void secondary_off() {
    /* Do not FBIOBLANK fb1 here.  On this MDSS it waits for a kickoff that
     * never comes once the secondary overlay is idle, blocking this daemon
     * for 30 seconds and resetting B on the next attempted transition.
     * Backlight=0 is sufficient to hide the folded panel. */
    /* Gate late non-zero Lights writes before sending Display Off. */
    set_secondary_display_allowed(false);
    write_sysfs("/sys/class/leds/lcd-backlight-2/brightness", "0");
}

static void secondary_on(int bl) {
    if (bl < 0)
        bl = 0;
    if (bl > 255)
        bl = 255;
    char b[16];
    snprintf(b, sizeof(b), "%d", bl);
    /* Never touch fb1/blank here.  HWC owns scanout; this explicit B write
     * only establishes the hinge-selected Display On state.  A's MDSS DCS
     * path mirrors every subsequent brightness and sleep/wake step. */
    set_secondary_display_allowed(true);
    /* A suppressed late write while folded can leave the LED class cached at
     * this same value.  Force a real callback after reopening so B cannot
     * remain in Display Off because the target brightness appears unchanged. */
    write_sysfs("/sys/class/leds/lcd-backlight-2/brightness", "0");
    write_sysfs("/sys/class/leds/lcd-backlight-2/brightness", b);
}

static void primary_off() {
    write_sysfs("/sys/class/leds/lcd-backlight/brightness", "0");
}

static void primary_on(int bl) {
    if (bl < 0)
        bl = 0;
    if (bl > 255)
        bl = 255;
    char b[16];
    snprintf(b, sizeof(b), "%d", bl);
    write_sysfs("/sys/class/leds/lcd-backlight/brightness", b);
}

static void set_primary_b_backlight_route(bool enabled) {
    /* mdss_dsi owns the actual Lights/DCS route.  This avoids a userspace
     * brightness watcher: wide remains mirrored, while closed B receives the
     * normal Android brightness endpoint without relighting A. */
    write_sysfs("/sys/module/mdss_dsi/parameters/fujisan_primary_b", enabled ? "1" : "0");
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

static int get_system_setting_int(const char* key, int fallback) {
    int pipefd[2];
    if (pipe2(pipefd, O_CLOEXEC) != 0)
        return fallback;
    const pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return fallback;
    }
    if (pid == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        const char* argv[] = {"/system/bin/settings", "get", "system", key, nullptr};
        execv(argv[0], const_cast<char* const*>(argv));
        _exit(127);
    }
    close(pipefd[1]);
    char output[32] = {};
    const ssize_t n = read(pipefd[0], output, sizeof(output) - 1);
    close(pipefd[0]);
    int status = 0;
    if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status) != 0 || n <= 0)
        return fallback;
    int value = fallback;
    return sscanf(output, "%d", &value) == 1 ? value : fallback;
}

static void put_system_setting_int(const char* key, int value) {
    char value_s[16];
    snprintf(value_s, sizeof(value_s), "%d", value);
    const char* argv[] = {"/system/bin/settings", "put", "system", key, value_s, nullptr};
    if (!run_service_call(argv))
        ALOGW("failed to write system setting %s", key);
}

/* Automatic brightness is global on Android 12.  Save whether this daemon
 * disabled it, and restore it as soon as B is no longer the folded primary so
 * wide mode keeps the user's original automatic-brightness behavior. */
static void reconcile_auto_brightness(bool boot_done, bool primary_b) {
    const bool b_single = boot_done && primary_b;
    char restore[PROPERTY_VALUE_MAX] = "0";
    property_get("persist.vendor.fujisan.restore_auto_brightness", restore, "0");
    if (b_single) {
        if (restore[0] != '1' && get_system_setting_int("screen_brightness_mode", 0) == 1) {
            property_set("persist.vendor.fujisan.restore_auto_brightness", "1");
            put_system_setting_int("screen_brightness_mode", 0);
        }
    } else if (restore[0] == '1') {
        put_system_setting_int("screen_brightness_mode", 1);
        property_set("persist.vendor.fujisan.restore_auto_brightness", "0");
    }
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

/* Both OEM and the rebase Synaptics drivers expose this per-controller
 * suspend attribute.  Discover the input directory at runtime because its
 * number is not an ABI, then stop the hidden controller rather than merely
 * dropping its events in InputReader. */
static void set_touch_controller_suspended(const char* expected_name,
                                           const char* suspend_attr, bool suspend) {
    DIR* inputs = opendir("/sys/class/input");
    if (!inputs)
        return;
    struct dirent* input;
    while ((input = readdir(inputs)) != nullptr) {
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
        name[strcspn(name, "\r\n")] = '\0';
        if (strcmp(name, expected_name) != 0)
            continue;
        char suspend_path[256];
        snprintf(suspend_path, sizeof(suspend_path), "/sys/class/input/%s/%s",
                 input->d_name, suspend_attr);
        write_sysfs(suspend_path, suspend ? "1" : "0");
        break;
    }
    closedir(inputs);
}

static void configure_touch_for_mode(bool zoom, bool primary_b,
                                     bool resume_visible_controllers) {
    static constexpr const char* kPrimaryTouch =
        "954faadc99bb5a7c1d0537b923e0490c90b47e98";
    static constexpr const char* kSecondaryTouch =
        "b99b5f2fc557ba939628ebbc5b685e1d66f25a78";

    /* The combined 2160px input device is required for a gesture spanning
     * both panels.  It must be bypassed while folded: Android otherwise
     * scales its 2160px range into the 1080px single-panel viewport. */
    write_sysfs("/sys/module/zte_touch_expand/parameters/separate_inputs",
                zoom ? "N" : "Y");

    /* Android 12's name/unique-id association only filters dispatch; its
     * TouchInputMapper still chooses the first INTERNAL viewport.  B must use
     * port 0 whenever it is the only logical primary, since single-A and
     * single-B deliberately share one 1080x1920 display and HWC exposes no
     * port-1 viewport in either case. */
    const char* remove_unique[] = {
        "/system/bin/service", "call", "input", "40",
        "s16", "zte-touchscreen-2nd", nullptr,
    };
    if (!run_service_call(remove_unique))
        ALOGW("failed to clear legacy B touch unique-id association");

    const char* associate_secondary_port[] = {
        "/system/bin/service", "call", "input", "37",
        "s16", "synaptics_dsx/touch_input_2nd", "i32",
        (zoom || primary_b) ? "0" : "1", nullptr,
    };
    if (!run_service_call(associate_secondary_port))
        ALOGW("failed to associate B touch port with %s display",
              zoom ? "zoom" : (primary_b ? "single-B primary" : "single-A"));

    /* Keep TD4322 powered across posture changes.  Folded mode already removes
     * B's display association and enables separate inputs, while the panel is
     * not physically power-cycled.  Suspending its controller here therefore
     * leaves it asleep after the next unfold because no MDSS reset follows. */
    if (primary_b)
        set_touch_controller_suspended("zte-touchscreen", "suspend", true);
    else if (resume_visible_controllers)
        set_touch_controller_suspended("zte-touchscreen", "suspend", false);

    if (zoom && resume_visible_controllers)
        set_touch_controller_suspended("zte-touchscreen-2nd", "suspend_2nd",
                                       false);

    /* Always restore A's real mapping before it becomes active.  It also
     * clears a stale runtime association from a previous single-B session. */
    const char* associate_primary_port[] = {
        "/system/bin/service", "call", "input", "37",
        "s16", "synaptics_dsx/touch_input", "i32",
        "0", nullptr,
    };
    if (!run_service_call(associate_primary_port))
        ALOGW("failed to associate A touch port with %s display",
              zoom ? "zoom" : (primary_b ? "single-B transition" : "single-A primary"));

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
    ALOGI("fujisan_halld start");
    enable_m1120();
    usleep(100 * 1000);
    const int control_fd = open_primary_control_socket();
    if (control_fd < 0)
        return 1;

    int last_st = -1;
    char last_primary[8] = {};
    char last_mode[16] = {};
    int last_power = -1;
    int last_bl1 = -1;
    int last_want_b = -1;
    int last_primary_b = -1;
    int last_touch_primary_b = -1;
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

        const int st = read_hall_status();

        char status_s[8];
        snprintf(status_s, sizeof(status_s), "%d", st);
        set_property_if_changed("vendor.fujisan.hall_status", status_s);

        const char* state = "closed_a";
        const char* mode = "single";
        char primary[8] = "a";

        /* The boot-animation compositor only has a reliable primary-panel
         * contract.  Never publish the virtual 2160px configuration before
         * Android declares boot complete: an unfolded cold boot consequently
         * stays on A for the whole animation.  init restarts this daemon on
         * sys.boot_completed=1, at which point the very first reconciliation
         * applies the actual hinge posture once, without any polling.
         *
         * Keep reading/exporting hall_status during this gate so the first
         * post-boot pass cannot race a mechanical transition. */
        if (!boot_done) {
            state = "boot_single";
            mode = "single";
            snprintf(primary, sizeof(primary), "a");
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
            snprintf(primary, sizeof(primary), "b");
        } else if (force_b[0] == '1') {
            state = "force_b";
            mode = "single";
            snprintf(primary, sizeof(primary), "a");
        } else if (st == 2 || st == 3) {
            state = "open";
            mode = "zoom";
            snprintf(primary, sizeof(primary), "%s", preferred[0] == 'b' ? "b" : "a");
        } else {
            state = "closed_a";
            mode = "single";
            if (force[0] == '1')
                snprintf(primary, sizeof(primary), "%s", preferred[0] == 'b' ? "b" : "a");
            else
                snprintf(primary, sizeof(primary), "a");
        }

        set_property_if_changed("vendor.fujisan.device_state", state);
        set_property_if_changed("vendor.fujisan.display_mode", mode);
        set_property_if_changed("vendor.fujisan.active_primary", primary);

        const bool power_on = display_is_on();
        const bool primary_b = mode[0] == 's' && primary[0] == 'b';
        reconcile_auto_brightness(boot_done, primary_b);
        /* Actual fujisan posture order is A(1) folded, B(2) mid-open,
         * C(3) fully open.  Both 2 and 3 expose the two inside panels; only
         * the folded A posture must turn B off. */
        const bool posture_wants_b = mode[0] == 'z' ||
                                     (mode[0] == 'd' && st != 1) ||
                                     force_b[0] == '1';
        /* The startup gate deliberately keeps B dark.  The primary display
         * power callback runs after MDSS starts to blank both panels, so do
         * not write B's LED from that transition: its native callback sends
         * DCS commands on the paired command-mode CTL. */
        const bool want_b = posture_wants_b || primary_b;
        const bool manage_b = boot_done && power_on;

        const int bl0 = read_int_file("/sys/class/leds/lcd-backlight/brightness", -1);
        int bl1 = read_int_file("/sys/class/leds/lcd-backlight-2/brightness", -1);
        const int want_b_int = want_b ? 1 : 0;
        if (manage_b && (!boot_panel_reconciled || want_b_int != last_want_b ||
                         static_cast<int>(primary_b) != last_primary_b)) {
            if (primary_b) {
                /* The atomic single-B backend still arms both CTLs.  Dark A
                 * through the normal mirrored route first, then direct future
                 * Android brightness updates to B before it is lit. */
                set_primary_b_backlight_route(false);
                primary_off();
                set_primary_b_backlight_route(true);
                secondary_on(bl0 > 0 ? bl0 : (bl1 > 0 ? bl1 :
                             get_system_setting_int("screen_brightness", 87)));
            } else if (want_b) {
                const int active_bl = bl0 > 0 ? bl0 : (bl1 > 0 ? bl1 :
                                      get_system_setting_int("screen_brightness", 87));

                /* This service is restarted on display-power transitions, so
                 * last_primary_b cannot be used to recover the wide route.
                 * Clear the B-only route before replaying A's brightness:
                 * the paired MDSS DCS transaction then restores A while the
                 * native B path completes B's Display On lifecycle. */
                set_primary_b_backlight_route(false);
                primary_on(active_bl);
                secondary_on(active_bl);
            } else {
                if (last_primary_b == 1) {
                    set_primary_b_backlight_route(false);
                    primary_on(bl1 > 0 ? bl1 : get_system_setting_int("screen_brightness", 87));
                }
                secondary_off();
            }
            last_want_b = want_b_int;
            last_primary_b = primary_b ? 1 : 0;
            boot_panel_reconciled = true;
        }
        /* A failed brightness write is safe to retry; unlike FBIOBLANK it
         * does not stall the hall worker or tear down the MDP overlay. */
        if (manage_b && !want_b && bl1 > 0)
            secondary_off();
        if (!power_on && primary_b)
            secondary_off();

        if (boot_done &&
            (!touch_mode_initialized || strcmp(mode, touch_mode) != 0 ||
             static_cast<int>(primary_b) != last_touch_primary_b)) {
            const bool was_initialized = touch_mode_initialized;
            configure_touch_for_mode(mode[0] == 'z', primary_b,
                                     touch_mode_initialized && power_on);
            /* Do not use `wm size` for posture changes.  It persists a
             * forced display size in Settings and makes the next folded boot
             * render BootAnimation as a 2160-wide desktop until this daemon
             * reaches sys.boot_completed.  HWC's active topology is the
             * authoritative logical size. */
            snprintf(touch_mode, sizeof(touch_mode), "%s", mode);
            last_touch_primary_b = primary_b ? 1 : 0;
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

        /* Blocks at zero CPU until m1120 or init reports a change. */
        wait_for_hinge_or_primary_request(control_fd);
    }
    return 0;
}
