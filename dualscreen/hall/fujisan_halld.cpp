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
    char b[16];
    snprintf(b, sizeof(b), "%d", bl);
    /* Keep fb1 scanout alive across the hinge transition.  Its client target
     * is refreshed by HWC after the panel is visible again. */
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

int main() {
    ALOGI("fujisan_halld start (A-primary stable power; B only for zoom)");
    enable_m1120();
    usleep(100 * 1000);

    int last_st = -1;
    char last_primary[8] = {};
    char last_mode[16] = {};
    int last_power = -1;
    int last_bl1 = -1;
    int last_want_b = -1;

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
            state = "open";
            mode = "zoom";
            snprintf(primary, sizeof(primary), "%s", preferred[0] == 'b' ? "b" : "a");
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
        /* In independent-display mode B remains hotplugged to Android, but
         * its physical panel is usable only while the hinge is open.  Keeping
         * it powered while folded lets HWC race the hall transition and causes
         * a one-frame flash on the next open. */
        const bool want_b = power_on &&
                            (mode[0] == 'z' ||
                             (mode[0] == 'd' && st == 2) ||
                             force_b[0] == '1');

        int bl1 = read_int_file("/sys/class/leds/lcd-backlight-2/brightness", -1);
        const int want_b_int = want_b ? 1 : 0;
        if (want_b_int != last_want_b) {
            if (want_b)
                secondary_on(180);
            else
                secondary_off();
            last_want_b = want_b_int;
        }
        /* A failed brightness write is safe to retry; unlike FBIOBLANK it
         * does not stall the hall worker or tear down the MDP overlay. */
        if (!want_b && bl1 > 0)
            secondary_off();

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
