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
#ifndef _SERVER_H_
#define _SERVER_H_

#include <stdint.h>

struct client_event;

int server_create(const char* local_ip, uint16_t port);

void server_close(int server_fd);

int server_accept(int server_fd);

int read_client_event(int client_fd, struct client_event* event);

#endif /* _SERVER_H_ */
