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
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pwd.h>
#include <getopt.h>

#include <errno.h>
#include <signal.h>
#include <linux/input.h>

#include "input_device.h"
#include "logging.h"
#include "server.h"

const char const INPUT_DEVICE_NAME[] = "remote-input";

const uint16_t DEFAULT_PORT_NUMBER = 4004;


#define FATAL_ERRNO(format) { \
        LOG_ERRNO(format); \
        exit(EXIT_FAILURE); \
    }

bool caught_sigint = false;

void sig_handler(int signum) {
    if (signum == SIGINT) {
        caught_sigint = true;
    }
    LOG(INFO, "closing");
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

void handle_event(device_t* device, struct client_event* event) {
    if (event->type == EV_MOUSE_DX) {
        device_mouse_move(device, event->value, 0);
    } else if (event->type == EV_MOUSE_DY) {
        device_mouse_move(device, 0, event->value);
    } else if (event->type == EV_KEY_DOWN) {
        device_key_down(device, event->value);
    } else if (event->type == EV_KEY_UP) {
        device_key_up(device, event->value);
    }
}

void handle_client(int client_fd, device_t* device) {
    struct client_event event;

    while (read_client_event(client_fd, &event) > 0) {
        LOG(DEBUG, "handling event");
        handle_event(device, &event);
    }

    device_release_all_keys(device);

    LOG(INFO, "terminating connection");

    close(client_fd);
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

int main(int argc, char* argv[]) {
    bool dont_daemonize = false;
    int verbosity = LOG_NOTICE;
    uint16_t local_port = DEFAULT_PORT_NUMBER;
    char* local_host = NULL;

    struct option const long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"verbose", no_argument, NULL, 'v'},
        {NULL, 0, NULL, 0}
    };

    int ch;
    while ((ch = getopt_long(argc, argv, "dvhlp:", long_options, NULL)) > 0) {
        switch (ch) {
            case 'd':
                dont_daemonize = true;
                break;
            case 'v':
                if (verbosity < LOG_DEBUG) {
                    verbosity++;
                }
                break;
            case 'l':
                local_host = optarg;
                break;
            case 'p':
                {
                    int port = strtol(optarg, NULL, 10);
                    if (port < 1) {
                        LOG(ERROR, "bad port number: %s", optarg);
                        exit(EXIT_FAILURE);
                    }
                    local_port = (uint16_t) port;
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

    log_set_level(verbosity);

    struct input_device device;
    if (device_create(INPUT_DEVICE_NAME, &device) < 0) {
        LOG(FATAL, "couldn't create input device");
        exit(EXIT_FAILURE);
    }

    drop_privileges();

    install_signal_handlers();

    int server_fd = server_create(local_host, local_port);
    if (server_fd < 0) {
        exit(EXIT_FAILURE);
    }

    /* Wait until after server creation, making sure errors are obvious */
    if (!dont_daemonize) {
        daemonize();
    }

    int client_fd;
    while ((client_fd = server_accept(server_fd)) != -1) {
        handle_client(client_fd, &device);
        if (caught_sigint) {
            break;
        }
    }

    server_close(server_fd);
    device_close(&device);

    LOG(INFO, "terminating successfully");

    return 0;
}
