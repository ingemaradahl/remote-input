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
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <netdb.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <sys/socket.h>

#include "keysym_to_linux_code.h"
#include "shared.h"


/*
 * TODO: SIGPIPE when remote closed
 * TODO: Ungrab pointer/keyboard on signals (SIGSEGV)
 * TODO: Mouse wheel
 */


int g_original_pointer_x;
int g_original_pointer_y;

int g_reset_pointer_x;
int g_reset_pointer_y;

int verbose = 0;

unsigned int abort_mask = ShiftMask | ControlMask;

void usage(const char* program_name) {
    printf("Usage: %s [OPTION] HOSTNAME [PORT]\n", program_name);
    puts("\nOptions:\n"
            "  -v  --verbose    write emitted events to stdout\n"
            "  -h  --help       show this help text and exit");
}

void get_screen_size(Display* display, int* ret_w, int* ret_h) {
    Window root_window = DefaultRootWindow(display);

    XWindowAttributes attributes;
    XGetWindowAttributes(display, root_window, &attributes);

    *ret_w = attributes.width;
    *ret_h = attributes.height;
}

void lock_keyboard(Display* display) {
    Window root_window = DefaultRootWindow(display);
    XGrabKeyboard(display, root_window, True /* owner_events */,
           GrabModeAsync, GrabModeAsync, CurrentTime);
}

void release_keyboard(Display* display) {
   XUngrabKeyboard(display, CurrentTime);
}

void reset_pointer(Display* display) {
    XWarpPointer(display, None, DefaultRootWindow(display), 0, 0, 0, 0,
            g_reset_pointer_x, g_reset_pointer_y);
}

void lock_pointer(Display* display) {
    Window root_window = DefaultRootWindow(display);

    Window pointer_root_w, pointer_child_w;
    int root_x, root_y;
    int child_x, child_y;
    unsigned int pointer_modifier_mask;

    XQueryPointer(display, root_window, &pointer_root_w, &pointer_child_w,
            &root_x, &root_y, &child_x, &child_y, &pointer_modifier_mask);

    g_original_pointer_x = root_x;
    g_original_pointer_y = root_y;


    /* Replace with an invisible cursor to "hide" it */
    static char cursor_pixmap_bits[] = {0};
    Pixmap cursor_pixmap = XCreateBitmapFromData(display, root_window,
            cursor_pixmap_bits, 1, 1);

    XColor xcolor;
    Cursor cursor = XCreatePixmapCursor(display, cursor_pixmap, cursor_pixmap,
            &xcolor, &xcolor, 1, 1);


    unsigned int pointer_events =
        PointerMotionMask | ButtonPressMask | ButtonReleaseMask;
    XGrabPointer(display, root_window, True, pointer_events, GrabModeAsync,
            GrabModeAsync, root_window, cursor, CurrentTime);


    int screen_width, screen_height;
    get_screen_size(display, &screen_width, &screen_height);

    /* Reset the pointer to the center of the screen to avoid edge conflicts */
    g_reset_pointer_x = screen_width / 2;
    g_reset_pointer_y = screen_height / 2;

    reset_pointer(display);
}

void release_pointer(Display* display) {
    Window root_window = DefaultRootWindow(display);

    XWarpPointer(display, None, root_window, 0, 0, 0, 0, g_original_pointer_x,
            g_original_pointer_y);

    XUngrabPointer(display, CurrentTime);
}

int consume_autorepeat_event(Display* display, XEvent* event) {
    if (XEventsQueued(display, QueuedAfterReading)) {
        XEvent next_event;
        XPeekEvent(display, &next_event);

        if (next_event.type == KeyPress &&
                next_event.xkey.time == event->xkey.time &&
                next_event.xkey.keycode == event->xkey.keycode) {
            XNextEvent(display, event);
            return 1;
        }
    }

    return 0;
}

int connect_to_server(const char* host, const char* service) {
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

void flush_events(Display* display) {
    XEvent e;
    while (XPending(display)) XNextEvent(display, &e);
}

int is_quit_combination(Display* display, XKeyEvent* event) {
    KeySym keysym = XkbKeycodeToKeysym(display, event->keycode, 0, 0);
    return keysym == XK_Tab && (event->state & abort_mask) == abort_mask;
}

void forward_key_button_event(Display* display, XEvent* event, int connection) {
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
        unsigned int keycode = key_event->keycode;
        KeySym keysym = XkbKeycodeToKeysym(display, keycode, 0, 0);
        cl_event.value = keysym_to_key(keysym);

        if (cl_event.value == 0) {
            const char* key_string = XKeysymToString(keysym);
            printf("No known translation for %#lx " "('%s', keycode %#x)\n",
                    keysym, key_string, keycode);
            return;
        }

        if (verbose) {
            printf("[KEY %s] %#lx ('%s', keycode %#x) => %u\n",
                    cl_event.type == EV_KEY_DOWN ? "DOWN" : "  UP",
                    keysym, XKeysymToString(keysym), keycode, cl_event.value);
        }
    } else {
        XButtonEvent* button_event = (XButtonEvent*)event;

        unsigned int button = button_event->button;
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

    write(connection, &cl_event, sizeof(cl_event));
}

int main(int argc, char* argv[]) {
    int option;
    while ((option = getopt(argc, argv, "vh")) > 0) {
        switch (option) {
            case 'v':
                verbose = 1;
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

    char* server_host = argv[optind++];
    char* server_port = "4004";
    if (argc > optind) {
        server_port = argv[optind];
    }

    Display* display = XOpenDisplay(NULL);

    if (display == NULL) {
        fprintf(stderr, "Cannot open display\n");
        exit(EXIT_FAILURE);
    }

    int connection = connect_to_server(server_host, server_port);
    if (connection < 0) {
        perror("error connecting to server");
        exit(EXIT_FAILURE);
    }

    lock_keyboard(display);
    lock_pointer(display);

    flush_events(display);

    printf("Forwarding input to %s, press Ctrl-Shift-Tab to quit\n", argv[1]);

    int quit = 0;
    XEvent e;
    while (!quit) {
        XNextEvent(display, &e);
        switch (e.type) {
            case KeyPress:
                if (is_quit_combination(display, (XKeyEvent*)&e)) {
                    quit =~ 0;
                    break;
                }
            case KeyRelease:
            case ButtonPress:
            case ButtonRelease:
                if (consume_autorepeat_event(display, &e)) {
                    break;
                }
                forward_key_button_event(display, &e, connection);
                break;
            case MotionNotify:
                {
                    XMotionEvent* pointer_event = (XMotionEvent*)&e;
                    if (pointer_event->x == g_reset_pointer_x &&
                            pointer_event->y == g_reset_pointer_y) {
                        break;
                    }

                    struct client_event event;
                    int dx = pointer_event->x - g_reset_pointer_x;
                    int dy = pointer_event->y - g_reset_pointer_y;

                    if (dx != 0) {
                        event.type = EV_MOUSE_DX;
                        event.value = dx;
                        write(connection, &event, sizeof(event));
                    }

                    if (dy != 0) {
                        event.type = EV_MOUSE_DY;
                        event.value = dy;
                        write(connection, &event, sizeof(event));
                    }

                    reset_pointer(display);

                    /* Clear out any events generated by reset_pointer */
                    while (XCheckTypedEvent(display, MotionNotify, &e));
                }
                break;
            default:
                break;
        }
    }

    release_pointer(display);
    release_keyboard(display);

    close(connection);

    if (XCloseDisplay(display)) {
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}
