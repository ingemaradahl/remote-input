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
#ifndef _SHARED_H_
#define _SHARED_H_

#include <stdint.h>

#define EV_DISCONNECT   0
#define EV_KEY_DOWN     1
#define EV_KEY_UP       2
#define EV_MOUSE_DX     3
#define EV_MOUSE_DY     4
#define EV_WHEEL        5
#define EV_HWHEEL       5

struct client_event {
    uint16_t type;
    int16_t value;
};

#endif /* _SHARED_H_ */
