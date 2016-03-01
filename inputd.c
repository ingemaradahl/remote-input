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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <pwd.h>
#include <signal.h>
#include <syslog.h>

#include "input_device.h"
#include "logging.h"
#include "server.h"
#include "shared.h"
#include "out/gen/keymap.h"

#define INPUT_DEVICE_NAME "remote-input"

#define DEFAULT_PORT_NUMBER 4004

#define FATAL_ERRNO(format) { \
        LOG_ERRNO(format); \
        exit(EXIT_FAILURE); \
    }

bool should_exit = false;

struct args {
    bool dont_daemonize;
    int verbosity;
    uint16_t local_port;
    char* local_host;
};

const struct args argument_defaults = {
    .dont_daemonize = false,
    .verbosity = LOG_NOTICE,
    .local_port = DEFAULT_PORT_NUMBER,
    .local_host = NULL
};

void sig_handler(int signum) {
    switch (signum) {
        case SIGINT:
        case SIGTERM:
            should_exit = true;
            break;
    }
}

int install_signal_handlers() {
    static struct sigaction sa = {
        .sa_handler = sig_handler
    };

    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    return 0;
}

void daemonize() {
    pid_t pid = fork();
    if (pid < 0) FATAL_ERRNO("couldn't fork");
    if (pid > 0) {
        LOG(INFO, "forked daemon with pid [%d]", pid);
        exit(EXIT_SUCCESS);
    }

    log_set_target(SYSLOG);

    fclose(stdin);
    fclose(stdout);
    fclose(stderr);

    pid = fork();
    if (pid < 0) FATAL_ERRNO("couldn't fork");
    if (pid > 0) {
        LOG(INFO, "forked again to [%d]", pid);
        exit(EXIT_SUCCESS);
    }
}

void drop_privileges() {
    struct passwd* nobody_user = getpwnam("nobody");
    if(setgid(nobody_user->pw_gid) < 0) {
        FATAL_ERRNO("couldn't change group");
    }
    if(setuid(nobody_user->pw_uid) < 0) {
        FATAL_ERRNO("couldn't change user");
    }
}

void handle_event(struct input_device* device, struct client_event* event) {
    switch (event->type) {
        case EV_MOUSE_DX:
            device_mouse_move(device, event->value, 0);
            break;
        case EV_MOUSE_DY:
            device_mouse_move(device, 0, event->value);
            break;
        case EV_KEY_DOWN:
            device_key_down(device, lookup_keycode(event->value));
            break;
        case EV_KEY_UP:
            device_key_up(device, lookup_keycode(event->value));
            break;
        case EV_WHEEL:
            device_mouse_wheel(device, 0, event->value);
            break;
        case EV_HWHEEL:
            device_mouse_wheel(device, event->value, 0);
            break;
        default:
            LOG(ERROR, "unknown event type: %u", event->type);
    }
}

void handle_client(struct client_info* client, struct input_device* device) {
    struct client_event event;

    while (read_client_event(client, &event) > 0) {
        handle_event(device, &event);
    }

    device_release_all_keys(device);

    LOG(NOTICE, "terminating connection from %s", client->cl_addr);

    close(client->cl_fd);
}

void usage(const char* program_name) {
    printf("Usage: %s [OPTION]\n", program_name);
    printf("\nOptions:\n"
            "  -d               don't detach and do not become a daemon\n"
            "  -l hostname/ip   hostname or ip on which to listen on\n"
            "  -p port_number   "
                "specify which port to bind to (defaults to %u)\n"
            "  -v  --verbose    increase verbosity/logging level\n"
            "  -h  --help       show this help text and exit\n"
            , DEFAULT_PORT_NUMBER);
}

struct args parse_args(int argc, char* argv[]) {
    struct args args = argument_defaults;

    struct option const long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"verbose", no_argument, NULL, 'v'},
        {NULL, 0, NULL, 0}
    };

    int ch;
    while ((ch = getopt_long(argc, argv, "dvhlp:", long_options, NULL)) > 0) {
        switch (ch) {
            case 'd':
                args.dont_daemonize = true;
                break;
            case 'v':
                if (args.verbosity < LOG_DEBUG) {
                    args.verbosity++;
                }
                break;
            case 'l':
                args.local_host = optarg;
                break;
            case 'p':
                {
                    int port = strtol(optarg, NULL, 10);
                    if (port < 1 || port > UINT16_MAX) {
                        LOG(ERROR, "bad port number: %s", optarg);
                        exit(EXIT_FAILURE);
                    }
                    args.local_port = (uint16_t) port;
                }
                break;
            case 'h':
                usage(argv[0]);
                exit(EXIT_SUCCESS);
            default:
                usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    return args;
}

int main(int argc, char* argv[]) {
    struct args args = parse_args(argc, argv);

    log_set_level(args.verbosity);

    struct input_device device;
    if (device_create(INPUT_DEVICE_NAME, &device) < 0) {
        LOG(FATAL, "couldn't create input device");
        exit(EXIT_FAILURE);
    }

    install_signal_handlers();

    struct server_info server;
    if (server_create(args.local_host, args.local_port, &server) < 0) {
        exit(EXIT_FAILURE);
    }

    LOG(NOTICE, "listening for connections on %s:%d", server.sv_addr,
            server.sv_port);

    drop_privileges();

    /* Wait until after server creation, making sure errors are obvious */
    if (!args.dont_daemonize) {
        daemonize();
    }

    struct client_info client;
    while (!should_exit && server_accept(&server, &client) != -1) {
        LOG(NOTICE, "accepted connection from %s", client.cl_addr);
        handle_client(&client, &device);
    }

    server_close(&server);
    device_close(&device);

    LOG(INFO, "terminating successfully");

    return 0;
}
