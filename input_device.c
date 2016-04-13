/*
 * Copyright (C) 2016 Ingemar Ådahl
 *
 * This file is part of remote-input.
 *
 * remote-input is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * remote-input is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with remote-input.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "input_device.h"

#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "logging.h"

#define IOCTL_BIND(fd, message, cleanup_label) { \
        int __bound_ioctl_fd = fd; \
        const char* __message = message; \
        void* __cleanup_label = &&cleanup_label;
#define IOCTL(request, ...) \
        if (ioctl(__bound_ioctl_fd, request, ## __VA_ARGS__) < 0) { \
            LOG_ERRNO_HERE("%s", __message); \
            goto *__cleanup_label; \
        }
#define IOCTL_END }

#define BUTTON_PRESS    1
#define BUTTON_RELEASE  0

int open_event_device(const char* sysfs_device) {
    DIR* sysfs_dir = opendir(sysfs_device);
    if (sysfs_dir == NULL) {
        LOG_ERRNO("error reading device from %s", sysfs_device);
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
            LOG_ERRNO("error opening %s", path_buf);
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

            LOG_ERRNO("error opening event device %s", path_buf);
        } else {
            break;
        }


fallback:
        snprintf(path_buf, PATH_MAX, "/dev/input/%s", node_entry->d_name);
        if ((event_fd = open(path_buf, O_WRONLY)) < 0) {
            LOG_ERRNO("error opening fallback event device %s", path_buf);
        }

        break;
    }

    closedir(sysfs_dir);

    return event_fd;
}

#if !defined(UI_GET_SYSNAME)
int read_sysfs_device_path(const char* device_name, char* sysfs_device_path,
        size_t device_path_size) {
    FILE* device_stream = fopen("/proc/bus/input/devices", "r");
    if (device_stream == NULL) {
        LOG_ERRNO("error opening /proc/bus/input/devices");
        return -1;
    }

    char name_pattern[512];
    snprintf(name_pattern, sizeof(name_pattern), "N: Name=\"%s\"\n",
            device_name);

    int status = 0;
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    while ((read = getline(&line, &len, device_stream)) != -1) {
        // First, find the correct device section
        if (read <= sizeof(name_pattern) &&
                strncmp(line, name_pattern, read) != 0) {
            continue;
        }

        // Found it, now look for the line pointing to the sysfs device
        while ((read = getline(&line, &len, device_stream)) != -1) {
            const char sysfs_pattern[] = "S: Sysfs=";
            ssize_t pattern_len = sizeof(sysfs_pattern) - 1 /* \0 */;
            if (strncmp(line, sysfs_pattern, pattern_len) != 0) {
                continue;
            }

            // Found this line as well, copy it and break out of both loops
            ssize_t sysfs_len = sizeof("/sys") - 1 /* \0 */;
            ssize_t device_len = read - pattern_len + sysfs_len;
            snprintf(sysfs_device_path, device_len, "/sys%s",
                    &line[pattern_len] /* skip past the leading pattern */);
            goto exit;
        }

        break;
    }

    status = -1;
    LOG(ERROR, "error reading sysfs device from procfs");

exit:
    free(line);
    return status;
}
#endif

int device_create(const char* device_name, struct input_device* device) {
    device->uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (device->uinput_fd < 0) {
        if (errno == ENOENT) {
            /* Some systems use an alternative location for the uinput device */
            device->uinput_fd = open("/dev/input/uinput",
                    O_WRONLY | O_NONBLOCK);

            LOG_ERRNO("couldn't open either /dev/uinput or /dev/input/uinput");
            LOG(ERROR, "is the uinput module loaded?");
        }
        LOG_ERRNO("couldn't open /dev/uinput");
        return -1;
    }

    IOCTL_BIND(device->uinput_fd, "error setting up device", error);
    IOCTL(UI_SET_EVBIT, EV_KEY);

    for (uint16_t key = KEY_ESC; key < KEY_MAX; key++) {
#ifdef ANDROID
        if (key == BTN_TOUCH) {
            /* BTN_TOUCH has been known to cause trouble on some devices */
            continue;
        }
#endif
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

    char sysfs_device_path[] = "/sys/devices/virtual/input/inputNNNN";
#if defined(UI_GET_SYSNAME)
    size_t dirname_size = sizeof("/sys/devices/virtual/input/") - 1 /* \0 */;
    size_t device_name_size = sizeof("inputNNNN");
    IOCTL(UI_GET_SYSNAME(device_name_size), &sysfs_device_path[dirname_size]);
    LOG(DEBUG, "created %s", sysfs_device_path);

    device->event_fd = open_event_device(sysfs_device_path);
#else
    if (read_sysfs_device_path(device_name, sysfs_device_path,
                sizeof(sysfs_device_path)) != -1) {
        LOG(DEBUG, "created %s", sysfs_device_path);
        device->event_fd = open_event_device(sysfs_device_path);
    }
#endif

    if (device->event_fd < 0) {
        LOG(WARNING, "unable to open event device!");
    }

    IOCTL_END;

    return 0;

error:
    close(device->uinput_fd);
    return -1;
}

void device_release_all_keys(struct input_device* device) {
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

void device_close(struct input_device* device) {
    device_release_all_keys(device);

    if (device->event_fd > 0 && close(device->event_fd < 0)) {
        LOG_ERRNO("error closing event device");
    }
    device->event_fd = -1;

    if (ioctl(device->uinput_fd, UI_DEV_DESTROY) < 0) {
        LOG_ERRNO("error closing device");
    }

    if (close(device->uinput_fd) < 0) {
        LOG_ERRNO("error closing uinput device");
    }

    device->uinput_fd = -1;
}

void commit_event(struct input_device* device, struct input_event* event) {
    gettimeofday(&event->time, NULL);
    if (write(device->uinput_fd, event, sizeof(struct input_event)) < 0) {
        LOG_ERRNO("error committing event");
    }
}

void sync_device(struct input_device* device) {
    static struct input_event sync_event = {
        .type = EV_SYN,
        .code = SYN_REPORT
    };

    commit_event(device, &sync_event);
}

void commit_mouse_event(struct input_device* device, uint16_t event_code_x,
        uint16_t event_code_y, int dx, int dy) {
    bool has_written = 0;
    struct input_event event = {
        .type = EV_REL
    };

    if (dx != 0) {
        event.code = event_code_x;
        event.value = dx;
        commit_event(device, &event);
        has_written = 1;
    }

    if (dy != 0) {
        event.code = event_code_y;
        event.value = dy;
        commit_event(device, &event);
        has_written = 1;
    }

    if (has_written) {
        sync_device(device);
    }
}

void device_mouse_move(struct input_device* device, int dx, int dy) {
    LOG(DEBUG, "MOUSE MOVE [%d,%d]", dx, dy);
    commit_mouse_event(device, REL_X, REL_Y, dx, dy);
}

void device_mouse_wheel(struct input_device* device, int dx, int dy) {
    LOG(DEBUG, "MOUSE WHEEL [%d,%d]", dx, dy);
    commit_mouse_event(device, REL_HWHEEL, REL_WHEEL, dx, dy);
}

void commit_device_key_event(struct input_device* device, uint16_t keycode,
        int32_t value) {
    struct input_event event = {
        .type = EV_KEY,
        .code = keycode,
        .value = value
    };
    commit_event(device, &event);
    sync_device(device);
}

void device_key_down(struct input_device* device, uint16_t keycode) {
    LOG(DEBUG, "KEY DOWN [%d]", keycode);
    commit_device_key_event(device, keycode, BUTTON_PRESS);
}

void device_key_up(struct input_device* device, uint16_t keycode) {
    LOG(DEBUG, "KEY UP [%d]", keycode);
    commit_device_key_event(device, keycode, BUTTON_RELEASE);
}
