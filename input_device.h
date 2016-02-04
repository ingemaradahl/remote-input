#ifndef _INPUT_DEVICE_H_
#define _INPUT_DEVICE_H_

#include <stdint.h>

typedef struct input_device {
    int uinput_fd;
} device_t;

int device_create(const char* device_name, device_t* device);

int device_close(device_t* device);

void device_mouse_move(device_t*, int dx, int dy);

void device_key_down(device_t*, uint16_t keycode);

void device_key_up(device_t*, uint16_t keycode);

#endif /* _INPUT_DEVICE_H_ */
