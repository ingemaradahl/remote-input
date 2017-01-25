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

#include "test/socket_mock.h"

#include <arpa/inet.h>
#include <check.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

struct accept_response {
    int bound_socket;
    int client_socket;
    char client_addr[INET6_ADDRSTRLEN];
    unsigned short sa_family;
    int errno_val;
    struct accept_response* next;
};

struct accept_response* g_responses = NULL;

void mock_accept_response(int bound_socket, int client_socket,
        const char* client_addr, unsigned short sa_family) {
    struct accept_response* response = malloc(sizeof(struct accept_response));
    memset(response, 0x0, sizeof(struct accept_response));

    response->bound_socket = bound_socket;
    response->client_socket = client_socket;

    if (client_socket < 0) {
        response->errno_val = sa_family;
    } else {
        ck_assert_uint_lt(strlen(client_addr), sizeof(response->client_addr));
        snprintf(response->client_addr, sizeof(response->client_addr), "%s",
                client_addr);
        response->sa_family = sa_family;
    }

    response->next = g_responses;
    g_responses = response;
}

static struct accept_response* remove_recorded_response(int bound_socket) {
    struct accept_response* head = g_responses;
    struct accept_response* prev = NULL;
    while(head != NULL && head->bound_socket != bound_socket) {
        prev = head;
        head = head->next;
    }

    if (head == NULL) {
        return NULL;
    }

    if (prev == NULL) {
        g_responses = head->next;
    } else {
        prev->next = head->next;
    }

    return head;
}

void free_accept_responses(void) {
    while (g_responses != NULL) {
        struct accept_response* tmp = g_responses;
        g_responses = g_responses->next;
        free(tmp);
    }
}

int accept(int socket, struct sockaddr* restrict address,
        socklen_t* restrict address_len) {
    struct accept_response* response = remove_recorded_response(socket);
    ck_assert_msg(response, "No response recorded for accept on %d", socket);
    ck_assert_msg(address, "Mocked accept() can not handle null address");

    int client_socket = response->client_socket;
    if (client_socket < 0) {
        errno = response->errno_val;
        free(response);
        return -1;
    }

    address->sa_family = response->sa_family;
    if (address->sa_family == AF_INET) {
        struct sockaddr_in* ipv4_addr = (struct sockaddr_in*)address;
        inet_pton(AF_INET, response->client_addr, &ipv4_addr->sin_addr);
        *address_len = sizeof(struct sockaddr_in);
    } else {
        ck_assert_msg(address->sa_family == AF_INET6, "invalid family: %d",
                address->sa_family);
        struct sockaddr_in6* ipv6_addr = (struct sockaddr_in6*)address;
        inet_pton(AF_INET6, response->client_addr, &(ipv6_addr->sin6_addr));
        *address_len = sizeof(struct sockaddr_in6);
    }

    free(response);

    return client_socket;
}
