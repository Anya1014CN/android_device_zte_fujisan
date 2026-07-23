/*
 * Fujisan hall / display-mode helper (no framework patch).
 *
 * Reads mxm1120 status (1=A, 2=B open, 3=C) from sysfs and publishes:
 *   vendor.fujisan.hall_status
 *   vendor.fujisan.device_state   closed_a | open | closed_b
 *   vendor.fujisan.display_mode   single | zoom
 *   persist.vendor.fujisan.primary_panel stays user preference (a|b)
 *
 * When closed, forces single mode on preferred/face panel.
 * When open (B), requests zoom (virtual 2160x1920) for HWC.
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
    if (fd < 0) return fallback;
    char buf[32] = {};
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return fallback;
    int v = fallback;
    if (sscanf(buf, "%d", &v) != 1) return fallback;
    return v;
}

static void write_sysfs(const char* path, const char* val) {
    int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) return;
    (void)write(fd, val, strlen(val));
    close(fd);
}

static void apply_panel_power(const char* primary /* "a" or "b" */, bool open_zoom) {
    /* blank unused panel when single; unblank both in zoom */
    if (open_zoom) {
        write_sysfs("/sys/class/graphics/fb0/blank", "0");
        write_sysfs("/sys/class/graphics/fb1/blank", "0");
        write_sysfs("/sys/class/leds/lcd-backlight/brightness", "180");
        write_sysfs("/sys/class/leds/lcd-backlight-2/brightness", "180");
        return;
    }
    if (primary[0] == 'b') {
        write_sysfs("/sys/class/graphics/fb0/blank", "1");
        write_sysfs("/sys/class/graphics/fb1/blank", "0");
        write_sysfs("/sys/class/leds/lcd-backlight/brightness", "0");
        write_sysfs("/sys/class/leds/lcd-backlight-2/brightness", "180");
    } else {
        write_sysfs("/sys/class/graphics/fb0/blank", "0");
        write_sysfs("/sys/class/graphics/fb1/blank", "1");
        write_sysfs("/sys/class/leds/lcd-backlight/brightness", "180");
        write_sysfs("/sys/class/leds/lcd-backlight-2/brightness", "0");
    }
}

int main() {
    ALOGI("fujisan_halld start");
    int last = -1;
    char preferred[PROPERTY_VALUE_MAX] = "a";
    for (;;) {
        property_get("persist.vendor.fujisan.primary_panel", preferred, "a");
        int st = read_int_file("/sys/module/ah1898/parameters/hall_status", -1);
        if (st < 0)
            st = read_int_file("/sys/module/mxm1120/parameters/hall_status", 1);
        if (st < 1 || st > 3) st = 1;

        char status_s[8];
        snprintf(status_s, sizeof(status_s), "%d", st);
        property_set("vendor.fujisan.hall_status", status_s);

        const char* state = "closed_a";
        const char* mode = "single";
        char primary[8] = "a";
        if (st == 2) {
            state = "open";
            mode = "zoom";
            /* keep preferred for when we re-fold */
            snprintf(primary, sizeof(primary), "%s", preferred[0] == 'b' ? "b" : "a");
        } else if (st == 3) {
            state = "closed_b";
            mode = "single";
            /* face C => B outward unless user forced A */
            if (preferred[0] == 'a' || preferred[0] == 'b') {
                /* When folded on B face, default primary to b; user persist wins if set via tile as force */
                snprintf(primary, sizeof(primary), "%s", preferred);
                if (preferred[0] != 'a' && preferred[0] != 'b')
                    snprintf(primary, sizeof(primary), "b");
            }
            /* physical face B: prefer b when user hasn't chosen force-a via tile flag */
            char force[PROPERTY_VALUE_MAX] = "0";
            property_get("persist.vendor.fujisan.primary_force", force, "0");
            if (force[0] != '1')
                snprintf(primary, sizeof(primary), "b");
        } else {
            state = "closed_a";
            mode = "single";
            char force[PROPERTY_VALUE_MAX] = "0";
            property_get("persist.vendor.fujisan.primary_force", force, "0");
            if (force[0] == '1')
                snprintf(primary, sizeof(primary), "%s", preferred[0] == 'b' ? "b" : "a");
            else
                snprintf(primary, sizeof(primary), "a");
        }

        property_set("vendor.fujisan.device_state", state);
        property_set("vendor.fujisan.display_mode", mode);
        property_set("vendor.fujisan.active_primary", primary);

        if (st != last) {
            ALOGI("hall %d state=%s mode=%s primary=%s", st, state, mode, primary);
            apply_panel_power(primary, st == 2);
            last = st;
        }
        usleep(200 * 1000);
    }
    return 0;
}
