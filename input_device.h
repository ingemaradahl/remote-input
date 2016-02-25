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

struct input_device {
    int uinput_fd;
    int event_fd;
};

int device_create(const char* device_name, struct input_device* device);

void device_close(struct input_device* device);

void device_mouse_move(struct input_device*, int dx, int dy);

void device_mouse_wheel(struct input_device*, int dx, int dy);

void device_key_down(struct input_device*, uint16_t keycode);

void device_key_up(struct input_device*, uint16_t keycode);

void device_release_all_keys(struct input_device* device);

#endif /* _INPUT_DEVICE_H_ */
