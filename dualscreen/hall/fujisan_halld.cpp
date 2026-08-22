/*
 * Fujisan hall topology publisher.
 *
 * The kernel is the authority for A, B and A+B scanout.  This daemon only
 * translates the hardware hall posture into the properties consumed by the
 * HWC config wrapper.  It never drives framebuffers, panel power, brightness,
 * touch calibration, or framework services.
 */
#define LOG_TAG "FujisanHalld"

#include <cutils/properties.h>
#include <cutils/sockets.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <log/log.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int read_int_file(const char* path, int fallback) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return fallback;
    char buf[64] = {};
    const ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0)
        return fallback;
    int value = fallback;
    return sscanf(buf, "%d", &value) == 1 ? value : fallback;
}

static int read_hall_status() {
    int status = read_int_file("/sys/module/ah1898/parameters/hall_status", -1);
    if (status < 0)
        status = read_int_file("/sys/module/mxm1120/parameters/hall_status", 1);
    return status >= 1 && status <= 3 ? status : 1;
}

static void write_sysfs(const char* path, const char* value) {
    const int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0)
        return;
    (void)write(fd, value, strlen(value));
    close(fd);
}

static void set_property_if_changed(const char* key, const char* value) {
    char current[PROPERTY_VALUE_MAX] = {};
    property_get(key, current, "");
    if (strcmp(current, value) != 0)
        property_set(key, value);
}

/* The driver exposes its enable switch below the dynamically assigned input
 * directory.  Enabling the sensor is its only sysfs operation here. */
static void enable_m1120() {
    DIR* inputs = opendir("/sys/class/input");
    if (!inputs)
        return;
    struct dirent* input;
    while ((input = readdir(inputs)) != nullptr) {
        if (strncmp(input->d_name, "input", 5) != 0)
            continue;
        char name_path[256];
        snprintf(name_path, sizeof(name_path), "/sys/class/input/%s/name", input->d_name);
        const int name_fd = open(name_path, O_RDONLY | O_CLOEXEC);
        if (name_fd < 0)
            continue;
        char name[64] = {};
        (void)read(name_fd, name, sizeof(name) - 1);
        close(name_fd);
        if (strncmp(name, "m1120", 5) != 0)
            continue;
        char enable_path[256];
        snprintf(enable_path, sizeof(enable_path), "/sys/class/input/%s/enable", input->d_name);
        write_sysfs(enable_path, "1");
        break;
    }
    closedir(inputs);
}

/* Event numbers are not ABI, so resolve m1120 from its input-device name. */
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
        const int name_fd = open(name_path, O_RDONLY | O_CLOEXEC);
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
            if (strncmp(event->d_name, "event", 5) == 0) {
                char event_path[64];
                snprintf(event_path, sizeof(event_path), "/dev/input/%s", event->d_name);
                fd = open(event_path, O_RDONLY | O_CLOEXEC);
                break;
            }
        }
        closedir(events);
    }
    closedir(inputs);
    return fd;
}

/* The QS tile requests an A/B selection only while folded.  Keeping this
 * narrow control endpoint avoids granting an app permission to set vendor
 * properties directly. */
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

static void handle_primary_request(int server_fd) {
    const int client = accept4(server_fd, nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK);
    if (client < 0)
        return;
    struct pollfd pfd {};
    pfd.fd = client;
    pfd.events = POLLIN;
    if (poll(&pfd, 1, 1000) <= 0 || !(pfd.revents & POLLIN)) {
        close(client);
        return;
    }
    char request[32] = {};
    if (read(client, request, sizeof(request) - 1) <= 0) {
        close(client);
        return;
    }

    char preferred[PROPERTY_VALUE_MAX] = "a";
    property_get("persist.vendor.fujisan.primary_panel", preferred, "a");
    const int hall = read_hall_status();
    char response[32] = {};
    char selected = '\0';
    if (strncmp(request, "toggle", 6) == 0)
        selected = preferred[0] == 'b' ? 'a' : 'b';
    else if (request[0] == 's' && request[1] == 'e' && request[2] == 't' &&
             (request[3] == ' ' || request[3] == '\t') &&
             (request[4] == 'a' || request[4] == 'b'))
        selected = request[4];

    if (selected == '\0') {
        snprintf(response, sizeof(response), "error protocol\n");
    } else if (hall != 1) {
        snprintf(response, sizeof(response), "unavailable %c %d\n",
                 preferred[0] == 'b' ? 'b' : 'a', hall);
    } else {
        property_set("persist.vendor.fujisan.primary_panel", selected == 'b' ? "b" : "a");
        property_set("persist.vendor.fujisan.primary_force", selected == 'b' ? "1" : "0");
        snprintf(response, sizeof(response), "ok %c %d\n", selected, hall);
        ALOGI("primary panel -> %c", selected);
    }
    (void)write(client, response, strlen(response));
    close(client);
}

static void wait_for_hinge_or_primary_request(int control_fd) {
    for (;;) {
        const int hinge_fd = open_m1120_event();
        if (hinge_fd < 0) {
            struct pollfd control {};
            control.fd = control_fd;
            control.events = POLLIN;
            if (poll(&control, 1, 1000) > 0 && (control.revents & POLLIN))
                handle_primary_request(control_fd);
            return;
        }

        struct pollfd pfds[2] = {};
        pfds[0].fd = hinge_fd;
        pfds[0].events = POLLIN;
        pfds[1].fd = control_fd;
        pfds[1].events = POLLIN;
        const int rc = poll(pfds, 2, -1);
        if (rc > 0 && (pfds[1].revents & POLLIN))
            handle_primary_request(control_fd);
        if (rc > 0 && (pfds[0].revents & POLLIN)) {
            struct input_event event;
            (void)read(hinge_fd, &event, sizeof(event));
        }
        close(hinge_fd);
        return;
    }
}

static void publish_topology() {
    const int hall = read_hall_status();
    char hall_value[8];
    snprintf(hall_value, sizeof(hall_value), "%d", hall);
    set_property_if_changed("vendor.fujisan.hall_status", hall_value);

    char boot_completed[PROPERTY_VALUE_MAX] = "0";
    property_get("sys.boot_completed", boot_completed, "0");
    char preferred[PROPERTY_VALUE_MAX] = "a";
    property_get("persist.vendor.fujisan.primary_panel", preferred, "a");
    char force[PROPERTY_VALUE_MAX] = "0";
    property_get("persist.vendor.fujisan.primary_force", force, "0");

    const bool boot_done = boot_completed[0] == '1';
    const bool wide = boot_done && (hall == 2 || hall == 3);
    /* Keep the boot geometry single-panel, but do not erase the user's folded
     * A/B choice.  HWC consumes active_primary (and, before property replay,
     * the persisted selection directly) to route the 1080x1920 BootAnimation
     * target.  The wide topology remains gated on boot completion. */
    const bool primary_b = !wide && force[0] == '1' && preferred[0] == 'b';
    set_property_if_changed("vendor.fujisan.device_state",
                            wide ? "open" : (boot_done ? "closed_a" : "boot_single"));
    set_property_if_changed("vendor.fujisan.display_mode", wide ? "zoom" : "single");
    set_property_if_changed("vendor.fujisan.active_primary", primary_b ? "b" : "a");
    ALOGI("hall=%d topology=%s primary=%c", hall, wide ? "C" : "single",
          primary_b ? 'b' : 'a');
}

int main() {
    ALOGI("fujisan_halld start");
    enable_m1120();
    usleep(100 * 1000);
    const int control_fd = open_primary_control_socket();
    if (control_fd < 0)
        return 1;

    for (;;) {
        publish_topology();
        wait_for_hinge_or_primary_request(control_fd);
    }
}
