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
#ifndef _INPUT_DEVICE_H_
#define _INPUT_DEVICE_H_

#include <stdint.h>

typedef struct input_device {
    int uinput_fd;
    int event_fd;
} device_t;

int device_create(const char* device_name, device_t* device);

void device_close(device_t* device);

void device_mouse_move(device_t*, int dx, int dy);

void device_mouse_wheel(device_t*, int dx, int dy);

void device_key_down(device_t*, uint16_t keycode);

void device_key_up(device_t*, uint16_t keycode);

void device_release_all_keys(device_t* device);

#endif /* _INPUT_DEVICE_H_ */
