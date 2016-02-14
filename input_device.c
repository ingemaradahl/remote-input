#include "input_device.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>

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

int open_event_device(const char* sysfs_device) {
    DIR* sysfs_dir = opendir(sysfs_device);
    if (sysfs_dir == NULL) {
        LOG_ERRNO("error reading device descriptors");
        return -1;
    }

    struct dirent* node_entry;
    int event_fd = -1;
    while((node_entry = readdir(sysfs_dir)) != NULL && event_fd < 0) {
        char path_buf[PATH_MAX];
        snprintf(path_buf, PATH_MAX, "%s/%s", sysfs_device, node_entry->d_name);
        struct stat stat_buf;
        if (stat(path_buf, &stat_buf) < 0) {
            LOG_ERRNO("stat error");
            continue;
        }

        if (!S_ISDIR(stat_buf.st_mode) ||
                strncmp(node_entry->d_name, "event", 5) != 0) {
            continue;
        }

        snprintf(path_buf, PATH_MAX, "%s/%s/dev", sysfs_device,
                node_entry->d_name);
        FILE* dev_file = fopen(path_buf, "r");
        if (dev_file == NULL) {
            LOG_ERRNO("error reading device descriptors");
            continue;
        }

        unsigned int major, minor;
        if (fscanf(dev_file, "%u:%u", &major, &minor) != 2) {
            LOG(ERROR, "error reading character device");
            fclose(dev_file);
            goto fallback;
        }

        fclose(dev_file);

        snprintf(path_buf, PATH_MAX, "/dev/char/%u:%u", major, minor);
        if ((event_fd = open(path_buf, O_WRONLY)) < 0) {
            if (errno == ENOENT) {
                /* The entry in /dev/char is a symlink to the actual input
                 * device in /dev/input, but it is created asynchronously,
                 * meaning it might not yet exist.
                 */
                goto fallback;
            }

            LOG_ERRNO("error opening event device");
        }

fallback:
        snprintf(path_buf, PATH_MAX, "/dev/input/%s", node_entry->d_name);
        if ((event_fd = open(path_buf, O_WRONLY)) < 0) {
            LOG_ERRNO("error opening event device");
        }
    }

    closedir(sysfs_dir);

    return event_fd;
}

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

#if defined(UI_GET_SYSNAME)
    char sysfs_device_path[] = "/sys/devices/virtual/input/inputNNNN";
    size_t dirname_size = sizeof("/sys/devices/virtual/input/") - 1 /* \0 */;
    size_t device_name_size = sizeof("inputNNNN");
    IOCTL(UI_GET_SYSNAME(device_name_size), &sysfs_device_path[dirname_size]);
    LOG(DEBUG, "created %s", sysfs_device_path);

    device->event_fd = open_event_device(sysfs_device_path);
#else
    /* TODO: Finding the sysfs node is unreliable, it's better to look up the
     * device in /proc/bus/input/devices (based on const char* device_name) and
     * find the appropriate handler (probably called event-something). That
     * should later correspond to the proper device node in /dev/input.
     * Hopefully.
     */
#warning "No UI_GET_SYSNAME"
#endif

    IOCTL_END;

    return 0;

error:
    close(device->uinput_fd);
    return -1;
}

void device_release_all_keys(device_t* device) {
    if (device->event_fd < 0) return;

    uint8_t keys[(KEY_MAX + (8 - 1)) / 8] = {0};
    int res = ioctl(device->event_fd, EVIOCGKEY(sizeof(keys)), &keys);
    if (res < 0) {
        LOG_ERRNO("ioctl");
        return;
    }

    for (int i = 0; i < sizeof(keys); i++) {
        if (keys[i] == 0) continue;
        for (int j = 0; j < 8; j++) {
            if (keys[i] & (1 << j)) {
                device_key_up(device, (i*8) + j);

            }
        }
    }
}

void device_close(device_t* device) {
    device_release_all_keys(device);
    if (ioctl(device->uinput_fd, UI_DEV_DESTROY) < 0) {
        LOG_ERRNO("error closing device");
    }

    if (device->event_fd > 0 && close(device->event_fd < 0)) {
        LOG_ERRNO("error closing event device");
    }
    device->event_fd = -1;

    if (close(device->uinput_fd) < 0) {
        LOG_ERRNO("error closing uinput device");
    }

    device->uinput_fd = -1;
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
