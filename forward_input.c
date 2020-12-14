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
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <sys/socket.h>

#include "keysym_to_linux_code.h"
#include "shared.h"

#define DEFAULT_SERVER_PORT_STR "4004"

static const uint32_t abort_mask = ShiftMask | ControlMask;

/*
 * TODO: SIGPIPE when remote closed
 */

struct point {
    uint16_t x;
    uint16_t y;
};

struct size {
    uint16_t width;
    uint16_t height;
};

struct pointer_info {
    struct point original_position;
    struct point reset_position;
};

struct args {
    bool verbose;
    bool quiet;
    char* server_host;
    char* server_port;
};

static const struct args argument_defaults = {
    .verbose = false,
    .quiet = false,
    .server_host = NULL,
    .server_port = DEFAULT_SERVER_PORT_STR
};

static struct size get_screen_size(Display* display) {
    Window root_window = DefaultRootWindow(display);

    XWindowAttributes attributes;
    XGetWindowAttributes(display, root_window, &attributes);

    return (struct size) {
        .width = attributes.width,
        .height = attributes.height
    };
}

static int32_t grab_root_window_keyboard(Display* display) {
    Window root_window = DefaultRootWindow(display);
    return XGrabKeyboard(display, root_window, True /* owner_events */,
            GrabModeAsync, GrabModeAsync, CurrentTime);
}

static int32_t lock_keyboard(Display* display) {
    int32_t grab_result;
    if ((grab_result = grab_root_window_keyboard(display)) == AlreadyGrabbed) {
        /* The program might have been invoked from a grabbing client, such as
         * xbindkeys. Wait for it to ungrab the keyboard, which generates a
         * FocusOut event.
         */
        XSelectInput(display, DefaultRootWindow(display), FocusChangeMask);

        XEvent focus_change_event;
        XMaskEvent(display, FocusChangeMask, &focus_change_event);

        return grab_root_window_keyboard(display);
    }

    return grab_result;
}

static void release_keyboard(Display* display) {
   XUngrabKeyboard(display, CurrentTime);
}

static void reset_pointer(Display* display, struct point* reset_position) {
    XWarpPointer(display, None, DefaultRootWindow(display), 0, 0, 0, 0,
            reset_position->x, reset_position->y);
}

static int32_t grab_and_hide_root_window_pointer(Display* display) {
    Window root_window = DefaultRootWindow(display);

    /* Replace the cursor with an invisible bitmap to "hide" it */
    static char cursor_pixmap_bits[] = {0};
    Pixmap cursor_pixmap = XCreateBitmapFromData(display, root_window,
            cursor_pixmap_bits, 1, 1);

    XColor xcolor;
    Cursor cursor = XCreatePixmapCursor(display, cursor_pixmap, cursor_pixmap,
            &xcolor, &xcolor, 1, 1);

    uint32_t pointer_event_mask =
        PointerMotionMask | ButtonPressMask | ButtonReleaseMask;

    int32_t grab_result =XGrabPointer(display, root_window, True,
            pointer_event_mask, GrabModeAsync, GrabModeAsync, root_window,
            cursor, CurrentTime);
    if (grab_result != GrabSuccess) {
        if (grab_result == AlreadyGrabbed) {
            /* Similarly to keyboard grabbing above, wait for the pointer to be
             * ungrabbed before trying to grab it */
            XSelectInput(display, DefaultRootWindow(display), LeaveWindowMask);

            XEvent leave_event;
            XMaskEvent(display, LeaveWindowMask, &leave_event);

            grab_result = XGrabPointer(display, root_window, True,
                    pointer_event_mask, GrabModeAsync, GrabModeAsync,
                    root_window, cursor, CurrentTime);
        }
    }

    return grab_result;
}

static int32_t lock_pointer(Display* display,
        struct pointer_info* pointer_info) {
    Window root_window = DefaultRootWindow(display);

    Window pointer_root_w, pointer_child_w;
    int root_x, root_y;
    int child_x, child_y;
    unsigned int pointer_modifier_mask;

    XQueryPointer(display, root_window, &pointer_root_w, &pointer_child_w,
            &root_x, &root_y, &child_x, &child_y, &pointer_modifier_mask);

    pointer_info->original_position.x = root_x;
    pointer_info->original_position.y = root_y;

    int32_t grab_result = grab_and_hide_root_window_pointer(display);
    if (grab_result != GrabSuccess) {
        return grab_result;
    }

    struct size screen_size = get_screen_size(display);

    /* Reset the pointer to the center of the screen to avoid edge conflicts */
    pointer_info->reset_position.x = screen_size.width / 2;
    pointer_info->reset_position.y = screen_size.height / 2;

    reset_pointer(display, &pointer_info->reset_position);

    return grab_result;
}

static void release_pointer(Display* display, struct point* original_position) {
    Window root_window = DefaultRootWindow(display);

    XWarpPointer(display, None, root_window, 0, 0, 0, 0,
            original_position->x,
            original_position->y);

    XUngrabPointer(display, CurrentTime);
}

static bool consume_autorepeat_event(Display* display, XEvent* event) {
    if (XEventsQueued(display, QueuedAfterReading)) {
        XEvent next_event;
        XPeekEvent(display, &next_event);

        if (next_event.type == KeyPress &&
                next_event.xkey.time == event->xkey.time &&
                next_event.xkey.keycode == event->xkey.keycode) {
            XNextEvent(display, event);
            return true;
        }
    }

    return false;
}

static int connect_to_server(const char* host, const char* service) {
    struct addrinfo connection_hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
    };

    struct addrinfo* server_addrs;
    int addr_res = getaddrinfo(host, service, &connection_hints, &server_addrs);
    if (addr_res != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(addr_res));
        return -1;
    }

    int socket_fd = -1;
    struct addrinfo* addr;
    for (addr = server_addrs; addr != NULL; addr = addr->ai_next) {
        if ((socket_fd = socket(addr->ai_family, addr->ai_socktype,
                addr->ai_protocol)) < 0) {
            continue;
        }

        if (connect(socket_fd, addr->ai_addr, addr->ai_addrlen) == 0)
            break;

        close(socket_fd);
    }

    freeaddrinfo(server_addrs);

    if (addr == NULL) {
        return -1;
    }

    return socket_fd;
}

static void write_client_event(int connection,
        struct client_event* client_event) {
    uint8_t event_buffer[EV_MSG_SIZE];

    EV_MSG_FIELD(event_buffer, type) = htons(client_event->type);
    EV_MSG_FIELD(event_buffer, value) = htons(client_event->value);

    write(connection, event_buffer, sizeof(event_buffer));
}

static void flush_events(Display* display) {
    XEvent e;
    while (XPending(display)) XNextEvent(display, &e);
}

static bool is_quit_combination(Display* display, XKeyEvent* event) {
    KeySym keysym = XkbKeycodeToKeysym(display, event->keycode, 0, 0);
    return keysym == XK_Tab && (event->state & abort_mask) == abort_mask;
}

static void forward_key_button_event(Display* display, XEvent* event,
        int connection, bool verbose, bool quiet) {
    struct client_event cl_event;

    switch (event->type) {
        case KeyPress:
        case ButtonPress:
            cl_event.type = EV_KEY_DOWN;
            break;
        case KeyRelease:
        case ButtonRelease:
            cl_event.type = EV_KEY_UP;
            break;
        default:
            return;
    }

    if (event->type == KeyPress || event->type == KeyRelease) {
        XKeyEvent* key_event = (XKeyEvent*)event;

        // TODO: Add option to get the "raw" keysym (no layout)
        uint32_t keycode = key_event->keycode;
        KeySym keysym = XkbKeycodeToKeysym(display, keycode, 0, 0);
        cl_event.value = keysym_to_key(keysym);

        if (cl_event.value == 0) {
            if (!quiet) {
                printf("No known translation for %#lx " "('%s', keycode %#x)\n",
                        keysym, XKeysymToString(keysym), keycode);
            }
            return;
        }

        if (verbose) {
            printf("[KEY %s] %#lx ('%s', keycode %#x) => %u\n",
                    cl_event.type == EV_KEY_DOWN ? "DOWN" : "  UP",
                    keysym, XKeysymToString(keysym), keycode, cl_event.value);
        }
    } else {
        XButtonEvent* button_event = (XButtonEvent*)event;

        uint32_t button = button_event->button;

        /* Linux handles mouse wheels as relative events */
        if (button > 3 && button < 8) {
            if (event->type == ButtonRelease) {
                /* Don't send duplicate events */
                return;
            }

            cl_event.type = button == 4 || button == 5 ? EV_WHEEL : EV_HWHEEL;
            cl_event.value = button == 5 || button == 6 ? -1 : 1;

            if (verbose) {
                printf("[BUTTON DOWN] %u => WHEEL %u\n", button,
                        cl_event.type);
            }
        } else {
            cl_event.value = keysym_to_key(button);

            if (cl_event.value == 0) {
                return;
            }

            if (verbose) {
                printf("[BUTTON %s] %u => %u\n",
                        cl_event.type == EV_KEY_DOWN ? "DOWN" : "  UP",
                        button, cl_event.value);
            }
        }
    }

    write_client_event(connection, &cl_event);
}

static void usage(const char* program_name) {
    printf("Usage: %s [OPTION] HOSTNAME [PORT]\n", program_name);
    puts("\nOptions:\n"
            "  -v  --verbose    write emitted events to stdout\n"
            "  -q  --quiet      suppress informative messages\n"
            "  -h  --help       show this help text and exit");
}

static struct args parse_args(int argc, char* argv[]) {
    struct args args = argument_defaults;

    struct option const long_options[] = {
        {"verbose", no_argument, NULL, 'v'},
        {"quiet", no_argument, NULL, 'q'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}

    };

    char option;
    while ((option = getopt_long(argc, argv, "vqh", long_options, NULL)) > 0) {
        switch (option) {
            case 'v':
                args.verbose = 1;
                args.quiet = 0;
                break;
            case 'q':
                args.verbose = 0;
                args.quiet = 1;
                break;
            case 'h':
                usage(argv[0]);
                exit(EXIT_SUCCESS);
            default:
                usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (argc == optind) {
        fprintf(stderr, "Missing hostname\n");
        usage(argv[0]);
        exit(EXIT_FAILURE);
    } else if (argc > optind + 2) {
        fprintf(stderr, "Invalid parameter: %s\n", argv[optind + 2]);
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    args.server_host = argv[optind++];

    if (argc > optind) {
        args.server_port = argv[optind];
    }

    return args;
}

int main(int argc, char* argv[]) {
    struct args args = parse_args(argc, argv);

    Display* display = XOpenDisplay(NULL);

    if (display == NULL) {
        fprintf(stderr, "Cannot open display\n");
        exit(EXIT_FAILURE);
    }

    int connection = connect_to_server(args.server_host, args.server_port);
    if (connection < 0) {
        perror("error connecting to server");
        exit(EXIT_FAILURE);
    }

    if (lock_keyboard(display) != GrabSuccess) {
        fprintf(stderr, "Couldn't grab keyboard!");
        exit(EXIT_FAILURE);
    }

    struct pointer_info pointer_info;
    if (lock_pointer(display, &pointer_info) != GrabSuccess) {
        fprintf(stderr, "Couldn't grab pointer!");
        exit(EXIT_FAILURE);
    }

    flush_events(display);

    if (!args.quiet) {
        printf("Forwarding input to %s, press Ctrl-Shift-Tab to quit\n",
                argv[1]);
    }

    bool quit = false;
    XEvent e;
    while (!quit) {
        XNextEvent(display, &e);
        switch (e.type) {
            case KeyPress:
                if (is_quit_combination(display, (XKeyEvent*)&e)) {
                    quit = true;
                    break;
                }
                /* fall through */
            case KeyRelease:
            case ButtonPress:
            case ButtonRelease:
                if (consume_autorepeat_event(display, &e)) {
                    break;
                }
                forward_key_button_event(display, &e, connection, args.verbose,
                        args.quiet);
                break;
            case MotionNotify:
                {
                    XMotionEvent* pointer_event = (XMotionEvent*)&e;
                    if (pointer_event->x == pointer_info.reset_position.x &&
                            pointer_event->y == pointer_info.reset_position.y) {
                        break;
                    }

                    struct client_event event;
                    int16_t dx =
                        pointer_event->x - pointer_info.reset_position.x;
                    int16_t dy =
                        pointer_event->y - pointer_info.reset_position.y;

                    if (dx != 0) {
                        event.type = EV_MOUSE_DX;
                        event.value = dx;
                        write_client_event(connection, &event);
                    }

                    if (dy != 0) {
                        event.type = EV_MOUSE_DY;
                        event.value = dy;
                        write_client_event(connection, &event);
                    }

                    reset_pointer(display, &pointer_info.reset_position);

                    /* Clear out any events generated by reset_pointer */
                    while (XCheckTypedEvent(display, MotionNotify, &e));
                }
                break;
            default:
                break;
        }
    }

    release_pointer(display, &pointer_info.original_position);
    release_keyboard(display);

    close(connection);

    if (XCloseDisplay(display)) {
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}
