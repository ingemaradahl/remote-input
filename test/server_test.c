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
#include "test/server_test.h"

#include <check.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include "logging.h"
#include "server.h"
#include "test/socket_mock.h"

struct server_info mock_server(int fd, const char* address, uint16_t port) {
    struct server_info server_info = {
        .sv_port = port,
        .sv_fd = fd
    };

    snprintf(server_info.sv_addr, sizeof(server_info.sv_addr), "%s", address);

    return server_info;
}

START_TEST(test_server_accept_ipv4) {
    int server_fd = 4;
    int client_fd = 5;

    struct server_info server = mock_server(server_fd, "localhost", 4000);
    mock_accept_response(server.sv_fd, client_fd, "192.168.0.42", AF_INET);

    struct client_info client;
    ck_assert_int_eq(0, server_accept(&server, &client));
    ck_assert_int_eq(client.cl_fd, client_fd);
    ck_assert_str_eq(client.cl_addr, "192.168.0.42");

    free_accept_responses();
} END_TEST

START_TEST(test_server_accept_ipv6) {
    int server_fd = 6;
    int client_fd = 7;

    struct server_info server = mock_server(server_fd, "localhost", 4000);
    mock_accept_response(server.sv_fd, client_fd, "::ffff:204.152.189.116",
            AF_INET6);

    struct client_info client;
    ck_assert_int_eq(0, server_accept(&server, &client));
    ck_assert_int_eq(client.cl_fd, client_fd);
    ck_assert_str_eq(client.cl_addr, "::ffff:204.152.189.116");

    free_accept_responses();
} END_TEST

START_TEST(test_server_accept_interrupt) {
    int server_fd = 8;
    int client_fd = -1;

    struct server_info server = mock_server(server_fd, "localhost", 4000);
    mock_accept_response(server.sv_fd, client_fd, "invalid", EINTR);

    struct client_info client;
    ck_assert_int_eq(-1, server_accept(&server, &client));
    ck_assert_int_eq(client.cl_fd, client_fd);

    free_accept_responses();
} END_TEST

START_TEST(test_server_accept_ebadf) {
    int server_fd = 9;
    int client_fd = -1;

    struct server_info server = mock_server(server_fd, "localhost", 4000);
    mock_accept_response(server.sv_fd, client_fd, "invalid", EBADF);

    /* Quench errno logging */
    log_set_level(LOG_CRIT);

    struct client_info client = {{0}};
    ck_assert_int_eq(-1, server_accept(&server, &client));
    ck_assert_int_eq(client.cl_fd, client_fd);
    ck_assert_str_eq(client.cl_addr, "");

    free_accept_responses();
} END_TEST

Suite* server_suite() {
    Suite* server_suite = suite_create("server.c");
    TCase* server_testcase = tcase_create("core");

    suite_add_tcase(server_suite, server_testcase);
    tcase_add_test(server_testcase, test_server_accept_ipv4);
    tcase_add_test(server_testcase, test_server_accept_ipv6);
    tcase_add_test(server_testcase, test_server_accept_interrupt);
    tcase_add_test(server_testcase, test_server_accept_ebadf);

    return server_suite;
}
