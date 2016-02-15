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

#include <errno.h>
#include <signal.h>
#include <linux/input.h>

#include "input_device.h"
#include "logging.h"
#include "server.h"

const char const INPUT_DEVICE_NAME[] = "remote-input";


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

int main(int argc, char** argv) {
    int should_daemonize = 0;

    log_init();
    if (argc > 1) {
        if (strcmp(argv[1], "-d") == 0) {
            should_daemonize = 1;
        }
    }

    struct input_device device;
    if (device_create(INPUT_DEVICE_NAME, &device) < 0) {
        LOG(FATAL, "couldn't create input device");
        exit(EXIT_FAILURE);
    }

    drop_privileges();

    install_signal_handlers();

    int server_fd = server_create(NULL, 4004);
    if (server_fd < 0) {
        exit(EXIT_FAILURE);
    }

    /* Wait until after server creation, making sure errors are obvious */
    if (should_daemonize) {
        daemonize();
    }

    int client_fd;
    while ((client_fd = server_accept(server_fd)) > 0) {
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
