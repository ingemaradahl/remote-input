/*
 * Copyright (C) 2017 Ingemar Ådahl
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
#ifndef _TEST_SOCKET_MOCK_H_
#define _TEST_SOCKET_MOCK_H_

void mock_accept_response(int bound_socket, int client_socket,
        const char* client_addr, unsigned short sa_family);

void free_accept_responses();

#endif  /* _TEST_SOCKET_MOCK_H_ */
