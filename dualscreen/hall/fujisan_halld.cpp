/*
 * Fujisan hall / panel power helper.
 *
 * closed + primary A: panel B fully off (blank POWERDOWN + bl=0)
 * closed + primary B: panel A off, B on
 * open (zoom): both on
 * screen off (primary bl=0 / blanked): force B off so power key / fold works
 */
#define LOG_TAG "FujisanHalld"
#include <cutils/properties.h>
#include <log/log.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int read_int_file(const char* path, int fallback) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return fallback;
    char buf[32] = {};
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

static void secondary_off() {
    write_sysfs("/sys/class/leds/lcd-backlight-2/brightness", "0");
    write_sysfs("/sys/class/graphics/fb1/blank", "4");
}

static void secondary_on(int bl) {
    char b[16];
    snprintf(b, sizeof(b), "%d", bl);
    write_sysfs("/sys/class/graphics/fb1/blank", "0");
    write_sysfs("/sys/class/leds/lcd-backlight-2/brightness", b);
}

static void primary_off() {
    write_sysfs("/sys/class/leds/lcd-backlight/brightness", "0");
    write_sysfs("/sys/class/graphics/fb0/blank", "4");
}

static void primary_unblank_only() {
    write_sysfs("/sys/class/graphics/fb0/blank", "0");
}

static bool screen_wants_off() {
    int bl = read_int_file("/sys/class/leds/lcd-backlight/brightness", -1);
    int blank = read_int_file("/sys/class/graphics/fb0/blank", 0);
    if (bl == 0)
        return true;
    if (blank != 0)
        return true;
    return false;
}

int main() {
    ALOGI("fujisan_halld start");
    int last_st = -1;
    int last_off = -1;
    char last_primary[8] = {};

    for (;;) {
        char preferred[PROPERTY_VALUE_MAX] = "a";
        property_get("persist.vendor.fujisan.primary_panel", preferred, "a");

        int st = read_int_file("/sys/module/ah1898/parameters/hall_status", -1);
        if (st < 0)
            st = read_int_file("/sys/module/mxm1120/parameters/hall_status", 1);
        if (st < 1 || st > 3)
            st = 1;

        char status_s[8];
        snprintf(status_s, sizeof(status_s), "%d", st);
        property_set("vendor.fujisan.hall_status", status_s);

        char force[PROPERTY_VALUE_MAX] = "0";
        property_get("persist.vendor.fujisan.primary_force", force, "0");

        const char* state = "closed_a";
        const char* mode = "single";
        char primary[8] = "a";

        if (st == 2) {
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

        bool off = screen_wants_off();
        bool changed =
            (st != last_st) || (off != last_off) || (strcmp(primary, last_primary) != 0);

        if (off) {
            /* Power key / sleep: always kill B. Leave A to SF/HWC. */
            secondary_off();
        } else if (st == 2) {
            primary_unblank_only();
            secondary_on(180);
        } else if (primary[0] == 'b') {
            primary_off();
            secondary_on(180);
        } else {
            primary_unblank_only();
            secondary_off();
        }

        if (changed) {
            ALOGI("hall %d state=%s mode=%s primary=%s screen_off=%d", st, state, mode, primary,
                  off ? 1 : 0);
        }

        last_st = st;
        last_off = off ? 1 : 0;
        snprintf(last_primary, sizeof(last_primary), "%s", primary);
        usleep(200 * 1000);
    }
    return 0;
}
