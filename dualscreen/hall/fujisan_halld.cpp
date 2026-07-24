/*
 * Fujisan hall / panel power helper.
 *
 * hall_status (mxm1120 / ah1898):
 *   1 = closed, A face out
 *   2 = open (flat) -> ZOOM large screen
 *   3 = closed, B face out
 *
 * Panel policy:
 *   closed_a: A on (SF), B off
 *   closed_b: keep A panel powered for composition, A BL=0, B on (HWC copies UI to fb1)
 *   open/zoom: both panels on (HWC splits 2160 or mirrors until SF picks zoom)
 *
 * Display sleep: HWC sets vendor.fujisan.display_power=0 and blanks B.
 * halld must NOT re-light B while asleep; on wake re-apply posture every loop.
 */
#define LOG_TAG "FujisanHalld"
#include <cutils/properties.h>
#include <log/log.h>
#include <dirent.h>
#include <errno.h>
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
    write_sysfs("/sys/class/leds/lcd-backlight-2/brightness", "0");
    write_sysfs("/sys/class/graphics/fb1/blank", "4");
}

static void secondary_on(int bl) {
    char b[16];
    snprintf(b, sizeof(b), "%d", bl);
    write_sysfs("/sys/class/graphics/fb1/blank", "0");
    usleep(50 * 1000);
    write_sysfs("/sys/class/leds/lcd-backlight-2/brightness", b);
}

/* Keep composition alive: only unblank, never powerdown fb0 from halld. */
static void primary_keep_alive() {
    write_sysfs("/sys/class/graphics/fb0/blank", "0");
}

static void primary_bl_off() {
    write_sysfs("/sys/class/leds/lcd-backlight/brightness", "0");
}

static bool display_is_on() {
    /* HWC may set this; not always reachable due to sepolicy. */
    char p[PROPERTY_VALUE_MAX] = "1";
    property_get("vendor.fujisan.display_power", p, "1");
    if (p[0] == '0')
        return false;

    /* Reliable on LOS19 / Android 12: Display.STATE_OFF=1, STATE_ON=2. */
    char ss[PROPERTY_VALUE_MAX] = "2";
    property_get("debug.tracing.screen_state", ss, "2");
    int state = 2;
    if (sscanf(ss, "%d", &state) == 1) {
        if (state == 1 /* OFF */ || state == 3 /* DOZE */ || state == 4 /* DOZE_SUSPEND */)
            return false;
    }
    return true;
}

int main() {
    ALOGI("fujisan_halld start (single/zoom posture)");
    enable_m1120();
    usleep(100 * 1000);

    int last_st = -1;
    char last_primary[8] = {};
    char last_mode[16] = {};
    int last_power = -1;

    for (;;) {
        enable_m1120();

        char preferred[PROPERTY_VALUE_MAX] = "a";
        property_get("persist.vendor.fujisan.primary_panel", preferred, "a");
        char force[PROPERTY_VALUE_MAX] = "0";
        property_get("persist.vendor.fujisan.primary_force", force, "0");
        char force_b[PROPERTY_VALUE_MAX] = "0";
        property_get("persist.vendor.fujisan.force_b_on", force_b, "0");

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

        if (force_b[0] == '1') {
            state = "force_b";
            mode = "single";
            snprintf(primary, sizeof(primary), "b");
        } else if (st == 2) {
            state = "open";
            mode = "zoom";
            snprintf(primary, sizeof(primary), "%s", preferred[0] == 'b' ? "b" : "a");
        } else if (st == 3) {
            state = "closed_b";
            mode = "single";
            if (force[0] == '1')
                snprintf(primary, sizeof(primary), "%s", preferred[0] == 'b' ? "b" : "a");
            else
                snprintf(primary, sizeof(primary), "b");
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
        if (!power_on) {
            /* Sleep: leave primary to HWC/SF; ensure B stays off. */
            secondary_off();
        } else if (st == 2 || force_b[0] == '1') {
            /* Open/zoom: both panels powered. */
            primary_keep_alive();
            secondary_on(180);
        } else if (primary[0] == 'b') {
            /* B face: keep A composing, hide A BL, show B (HWC copies). */
            primary_keep_alive();
            primary_bl_off();
            secondary_on(180);
        } else {
            /* A face single: B fully off. */
            primary_keep_alive();
            secondary_off();
        }

        int bl1 = read_int_file("/sys/class/leds/lcd-backlight-2/brightness", -1);
        bool changed = (st != last_st) || (strcmp(primary, last_primary) != 0) ||
                       (strcmp(mode, last_mode) != 0) || (power_on != (last_power == 1));
        if (changed) {
            ALOGI("hall %d state=%s mode=%s primary=%s power=%d bl1=%d", st, state, mode, primary,
                  power_on ? 1 : 0, bl1);
            last_st = st;
            last_power = power_on ? 1 : 0;
            snprintf(last_primary, sizeof(last_primary), "%s", primary);
            snprintf(last_mode, sizeof(last_mode), "%s", mode);
        }

        usleep(200 * 1000);
    }
    return 0;
}
