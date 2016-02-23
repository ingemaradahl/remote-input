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
#include <netinet/in.h>

struct client_event;

struct server_info {
    char sv_addr[INET6_ADDRSTRLEN];
    uint16_t sv_port;
    int sv_fd;
};

struct client_info {
    char cl_addr[INET6_ADDRSTRLEN];
    int cl_fd;
};

int server_create(const char* local_ip, uint16_t port, struct server_info*);

void server_close(struct server_info*);

int server_accept(struct server_info*, struct client_info* client);

int read_client_event(struct client_info* client, struct client_event* event);

#endif /* _SERVER_H_ */
