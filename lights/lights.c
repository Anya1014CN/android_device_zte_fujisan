/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright (C) 2014 The Linux Foundation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 */

#include <errno.h>
#include <fcntl.h>
#include <hardware/lights.h>
#include <log/log.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static pthread_once_t g_init = PTHREAD_ONCE_INIT;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static struct light_state_t g_notification;
static struct light_state_t g_battery;

static const char kRedLedFile[] = "/sys/class/leds/red/brightness";
static const char kGreenLedFile[] = "/sys/class/leds/green/brightness";
static const char kBlueLedFile[] = "/sys/class/leds/blue/brightness";
static const char kLcdFile[] = "/sys/class/leds/lcd-backlight/brightness";
static const char kButtonFile[] = "/sys/class/leds/button-backlight/brightness";
static const char kRedBlinkFile[] = "/sys/class/leds/red/blink";
static const char kGreenBlinkFile[] = "/sys/class/leds/green/blink";
static const char kBlueBlinkFile[] = "/sys/class/leds/blue/blink";

static void init_globals(void) {
    pthread_mutex_init(&g_lock, NULL);
}

static int write_int(const char* path, int value) {
    static int already_warned;
    int fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        if (!already_warned) {
            ALOGE("write_int failed to open %s: %s", path, strerror(errno));
            already_warned = 1;
        }
        return -errno;
    }

    char buffer[20];
    const int bytes = snprintf(buffer, sizeof(buffer), "%d\n", value);
    const ssize_t written = write(fd, buffer, (size_t)bytes);
    close(fd);
    return written == -1 ? -errno : 0;
}

static int is_lit(const struct light_state_t* state) {
    return state->color & 0x00ffffff;
}

static int rgb_to_brightness(const struct light_state_t* state) {
    const int color = state->color & 0x00ffffff;
    return ((77 * ((color >> 16) & 0x00ff)) +
            (150 * ((color >> 8) & 0x00ff)) +
            (29 * (color & 0x00ff))) >> 8;
}

static int set_light_backlight(struct light_device_t* dev,
                               const struct light_state_t* state) {
    if (!dev) return -EINVAL;
    pthread_mutex_lock(&g_lock);
    const int err = write_int(kLcdFile, rgb_to_brightness(state));
    pthread_mutex_unlock(&g_lock);
    return err;
}

static int set_speaker_light_locked(struct light_device_t* dev,
                                    const struct light_state_t* state) {
    if (!dev) return -EINVAL;

    int on_ms = 0;
    int off_ms = 0;
    if (state->flashMode == LIGHT_FLASH_TIMED) {
        on_ms = state->flashOnMS;
        off_ms = state->flashOffMS;
    }
    const int blink = on_ms > 0 && off_ms > 0 ? (on_ms == off_ms ? 2 : 1) : 0;
    const unsigned int color = state->color;
    const int red = (color >> 16) & 0xff;
    const int green = (color >> 8) & 0xff;
    const int blue = color & 0xff;

    if (blink) {
        if (red && write_int(kRedBlinkFile, blink)) write_int(kRedLedFile, 0);
        if (green && write_int(kGreenBlinkFile, blink)) write_int(kGreenLedFile, 0);
        if (blue && write_int(kBlueBlinkFile, blink)) write_int(kBlueLedFile, 0);
    } else {
        write_int(kRedLedFile, red);
        write_int(kGreenLedFile, green);
        write_int(kBlueLedFile, blue);
    }
    return 0;
}

static void handle_speaker_battery_locked(struct light_device_t* dev) {
    set_speaker_light_locked(dev, is_lit(&g_battery) ? &g_battery : &g_notification);
}

static int set_light_battery(struct light_device_t* dev,
                             const struct light_state_t* state) {
    pthread_mutex_lock(&g_lock);
    g_battery = *state;
    handle_speaker_battery_locked(dev);
    pthread_mutex_unlock(&g_lock);
    return 0;
}

static int set_light_notifications(struct light_device_t* dev,
                                   const struct light_state_t* state) {
    pthread_mutex_lock(&g_lock);
    g_notification = *state;
    handle_speaker_battery_locked(dev);
    pthread_mutex_unlock(&g_lock);
    return 0;
}

static int set_light_attention(struct light_device_t* dev,
                               const struct light_state_t* state) {
    pthread_mutex_lock(&g_lock);
    handle_speaker_battery_locked(dev);
    pthread_mutex_unlock(&g_lock);
    return 0;
}

static int set_light_buttons(struct light_device_t* dev,
                             const struct light_state_t* state) {
    if (!dev) return -EINVAL;
    pthread_mutex_lock(&g_lock);
    const int err = write_int(kButtonFile, state->color & 0xff);
    pthread_mutex_unlock(&g_lock);
    return err;
}

static int close_lights(struct light_device_t* dev) {
    free(dev);
    return 0;
}

static int open_lights(const struct hw_module_t* module, const char* name,
                       struct hw_device_t** device) {
    int (*set_light)(struct light_device_t*, const struct light_state_t*);
    if (!strcmp(LIGHT_ID_BACKLIGHT, name))
        set_light = set_light_backlight;
    else if (!strcmp(LIGHT_ID_BATTERY, name))
        set_light = set_light_battery;
    else if (!strcmp(LIGHT_ID_NOTIFICATIONS, name))
        set_light = set_light_notifications;
    else if (!strcmp(LIGHT_ID_BUTTONS, name))
        set_light = set_light_buttons;
    else if (!strcmp(LIGHT_ID_ATTENTION, name))
        set_light = set_light_attention;
    else
        return -EINVAL;

    pthread_once(&g_init, init_globals);
    struct light_device_t* dev = calloc(1, sizeof(*dev));
    if (!dev) return -ENOMEM;
    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t*)module;
    dev->common.close = (int (*)(struct hw_device_t*))close_lights;
    dev->set_light = set_light;
    *device = (struct hw_device_t*)dev;
    return 0;
}

static struct hw_module_methods_t lights_module_methods = {
    .open = open_lights,
};

struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 1,
    .version_minor = 0,
    .id = LIGHTS_HARDWARE_MODULE_ID,
    .name = "MSM8996 lights module",
    .author = "The Android Open Source Project",
    .methods = &lights_module_methods,
};
