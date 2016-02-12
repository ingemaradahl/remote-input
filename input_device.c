#include "input_device.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>

#include <sys/time.h>
#include "logging.h"

// TODO: If /dev/uinput wasn't found, try /dev/input/uinput

#define IOCTL_BIND(fd, message, cleanup_label) { \
        int __bound_ioctl_fd = fd; \
        const char* __message = message; \
        void* __cleanup_label = &&cleanup_label;
#define IOCTL(request, ...) \
        if (ioctl(__bound_ioctl_fd, request, ## __VA_ARGS__) < 0) { \
            LOG_ERRNO_HERE(__message); \
            goto *__cleanup_label; \
        }
#define IOCTL_END }

#define BUTTON_PRESS    1
#define BUTTON_RELEASE  0

int device_create(const char* device_name, device_t* device) {
    device->uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (device->uinput_fd < 0) {
        LOG_ERRNO("couldn't open /dev/uinput");
        return -1;
    }

    IOCTL_BIND(device->uinput_fd, "error setting up device", error);
    IOCTL(UI_SET_EVBIT, EV_KEY);

    IOCTL(UI_SET_KEYBIT, BTN_LEFT);
    IOCTL(UI_SET_KEYBIT, BTN_RIGHT);
    IOCTL(UI_SET_KEYBIT, BTN_MIDDLE);

    for (uint16_t key=KEY_ESC; key<KEY_KPDOT; key++) {
        IOCTL(UI_SET_KEYBIT, key);
    }

    IOCTL(UI_SET_EVBIT, EV_REL);
    IOCTL(UI_SET_RELBIT, REL_X);
    IOCTL(UI_SET_RELBIT, REL_Y);
    IOCTL(UI_SET_RELBIT, REL_WHEEL);
    IOCTL(UI_SET_RELBIT, REL_HWHEEL);

    struct uinput_user_dev uinput_device = {
        .id.bustype = BUS_VIRTUAL,
        .id.vendor = 0x1,
        .id.product = 0x1,
        .id.version = 1
    };
    strncpy(uinput_device.name, device_name, UINPUT_MAX_NAME_SIZE);

    if (write(device->uinput_fd, &uinput_device, sizeof(uinput_device)) < 0) {
        LOG_ERRNO("write error creating device");
        goto error;
    }

    IOCTL(UI_DEV_CREATE);
    IOCTL_END;

    return 0;

error:
    close(device->uinput_fd);
    return -1;
}

int device_close(device_t* device) {
    if (ioctl(device->uinput_fd, UI_DEV_DESTROY) < 0) {
        LOG_ERRNO("error closing device");
    }

    return close(device->uinput_fd);
}

void commit_event(device_t* device, const struct input_event* event) {
    if (write(device->uinput_fd, event, sizeof(struct input_event)) < 0) {
        LOG_ERRNO("error commiting event");
    }
}

void sync_device(device_t* device) {
    static struct input_event sync_event = {
        .type = EV_SYN,
        .code = SYN_REPORT
    };

    gettimeofday(&sync_event.time, NULL);
    commit_event(device, &sync_event);
}

void device_mouse_move(device_t* device, int dx, int dy) {
    bool has_written = 0;
    struct input_event event;

    if (dx != 0) {
        event.type = EV_REL;
        event.code = REL_X;
        event.value = dx;
        commit_event(device, &event);
        has_written = 1;
    }

    if (dy != 0) {
        event.type = EV_REL;
        event.code = REL_Y;
        event.value = dy;
        commit_event(device, &event);
        has_written = 1;
    }

    if (has_written) {
        sync_device(device);
    }
}

void device_key_down(device_t* device, uint16_t keycode) {
    LOG(DEBUG, "KEY DOWN [%d]", keycode);
    struct input_event event = {
        .type = EV_KEY,
        .code = keycode,
        .value = BUTTON_PRESS
    };
    gettimeofday(&event.time, NULL);
    commit_event(device, &event);
    sync_device(device);
}

void device_key_up(device_t* device, uint16_t keycode) {
    LOG(DEBUG, "KEY UP [%d]", keycode);
    struct input_event event = {
        .type = EV_KEY,
        .code = keycode,
        .value = BUTTON_RELEASE
    };
    gettimeofday(&event.time, NULL);
    commit_event(device, &event);
    sync_device(device);
}
