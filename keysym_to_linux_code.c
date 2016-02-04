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
#include "keysym_to_linux_code.h"

#include <X11/X.h>
#include <X11/keysym.h>
#include <linux/input.h>

#define MAP(x11_keysym, linux_keysym) case x11_keysym: return linux_keysym
#define AUTOMAP(sym) MAP(XK_##sym, KEY_##sym)

uint16_t keysym_to_key(unsigned int keysym) {
    switch (keysym){
        MAP(XK_BackSpace, KEY_BACKSPACE);
        MAP(XK_Tab, KEY_TAB);
        MAP(XK_Linefeed, KEY_LINEFEED);
        MAP(XK_Clear, KEY_CLEAR);
        MAP(XK_Return, KEY_ENTER);
        MAP(XK_Pause, KEY_PAUSE);
        MAP(XK_Scroll_Lock, KEY_SCROLLLOCK);
        MAP(XK_Sys_Req, KEY_SYSRQ);
        MAP(XK_Escape, KEY_ESC);
        MAP(XK_Delete, KEY_DELETE);

        MAP(XK_Home, KEY_HOME);
        MAP(XK_Left, KEY_LEFT);
        MAP(XK_Up, KEY_UP);
        MAP(XK_Right, KEY_RIGHT);
        MAP(XK_Down, KEY_DOWN);
        //MAP(XK_Prior, );
        MAP(XK_Page_Up, KEY_PAGEUP);
        //MAP(XK_Next, KEY_NEXT);
        MAP(XK_Page_Down, KEY_PAGEDOWN);
        MAP(XK_End, KEY_END);
        MAP(XK_Begin, KEY_HOME);

        MAP(XK_KP_Space, KEY_SPACE);
        MAP(XK_KP_Tab, KEY_TAB);
        MAP(XK_KP_Enter, KEY_ENTER);
        MAP(XK_KP_Home, KEY_HOME);
        MAP(XK_KP_Left, KEY_LEFT);
        MAP(XK_KP_Up, KEY_UP);
        MAP(XK_KP_Right, KEY_RIGHT);
        MAP(XK_KP_Down, KEY_DOWN);
        //MAP(XK_KP_Prior, );
        MAP(XK_KP_Page_Up, KEY_PAGEUP);
        //MAP(XK_KP_Next, KEY_NEXT);
        MAP(XK_KP_Page_Down, KEY_PAGEDOWN);
        MAP(XK_KP_End, KEY_END);
        MAP(XK_KP_Begin, KEY_HOME);
        MAP(XK_KP_Insert, KEY_INSERT);
        MAP(XK_KP_Delete, KEY_DELETE);
        MAP(XK_KP_Equal, KEY_KPEQUAL);
        MAP(XK_KP_Multiply, KEY_KPASTERISK);
        MAP(XK_KP_Add, KEY_KPPLUS);
        MAP(XK_KP_Separator, KEY_KPCOMMA);
        MAP(XK_KP_Subtract, KEY_KPMINUS);
        //MAP(XK_KP_Decimal, );
        MAP(XK_KP_Divide, KEY_KPSLASH);

        MAP(XK_KP_0, KEY_KP0);
        MAP(XK_KP_1, KEY_KP1);
        MAP(XK_KP_2, KEY_KP2);
        MAP(XK_KP_3, KEY_KP3);
        MAP(XK_KP_4, KEY_KP4);
        MAP(XK_KP_5, KEY_KP5);
        MAP(XK_KP_6, KEY_KP6);
        MAP(XK_KP_7, KEY_KP7);
        MAP(XK_KP_8, KEY_KP8);
        MAP(XK_KP_9, KEY_KP9);

        AUTOMAP(F1);
        AUTOMAP(F2);
        AUTOMAP(F3);
        AUTOMAP(F4);
        AUTOMAP(F5);
        AUTOMAP(F6);
        AUTOMAP(F7);
        AUTOMAP(F8);
        AUTOMAP(F9);
        AUTOMAP(F10);
        AUTOMAP(F11);
        AUTOMAP(F12);

        MAP(XK_Shift_L, KEY_LEFTSHIFT);
        MAP(XK_Shift_R, KEY_RIGHTSHIFT);
        MAP(XK_Control_L, KEY_LEFTCTRL);
        MAP(XK_Control_R, KEY_RIGHTCTRL);
        MAP(XK_Caps_Lock, KEY_CAPSLOCK);

        MAP(XK_Meta_L, KEY_LEFTMETA);
        MAP(XK_Meta_R, KEY_RIGHTMETA);
        MAP(XK_Alt_L, KEY_LEFTALT);
        MAP(XK_Alt_R, KEY_RIGHTALT);

        /* Latin 1 */
        MAP(XK_space, KEY_SPACE);
        //MAP(XK_exclam, KEY_EXCLAM);
        //MAP(XK_quotedbl, KEY_QUOTEDBL);
        //MAP(XK_numbersign, KEY_NUMBERSIGN);
        //MAP(XK_dollar, KEY_DOLLAR);
        //MAP(XK_percent, KEY_PERCENT);
        //MAP(XK_ampersand, KEY_AMPERSAND);
        MAP(XK_apostrophe, KEY_APOSTROPHE);
        /*MAP(XK_quoteright, KEY_QUOTERIGHT);*/
        //MAP(XK_parenleft, KEY_LEFTBRACE); /* ?? */
        //MAP(XK_parenright, KEY_RIGHTBRACE); /* ?? */
        //MAP(XK_asterisk, KEY_ASTERISK);
        //MAP(XK_plus, KEY_PLUS);
        MAP(XK_comma, KEY_COMMA);

        MAP(XK_minus, KEY_MINUS);
        MAP(XK_period, KEY_DOT);
        MAP(XK_slash, KEY_SLASH);
        MAP(XK_0, KEY_0);
        MAP(XK_1, KEY_1);
        MAP(XK_2, KEY_2);
        MAP(XK_3, KEY_3);
        MAP(XK_4, KEY_4);
        MAP(XK_5, KEY_5);
        MAP(XK_6, KEY_6);
        MAP(XK_7, KEY_7);
        MAP(XK_8, KEY_8);
        MAP(XK_9, KEY_9);
        //MAP(XK_colon, KEY_COLON);
        MAP(XK_semicolon, KEY_SEMICOLON);
        //MAP(XK_less, KEY_LESS);
        MAP(XK_equal, KEY_EQUAL);
        //MAP(XK_greater, KEY_GREATER);
        //MAP(XK_question, KEY_QUESTION);
        //MAP(XK_at, KEY_AT);

        MAP(XK_A, KEY_A);
        MAP(XK_B, KEY_B);
        MAP(XK_C, KEY_C);
        MAP(XK_D, KEY_D);
        MAP(XK_E, KEY_E);
        MAP(XK_F, KEY_F);
        MAP(XK_G, KEY_G);
        MAP(XK_H, KEY_H);
        MAP(XK_I, KEY_I);
        MAP(XK_J, KEY_J);
        MAP(XK_K, KEY_K);
        MAP(XK_L, KEY_L);
        MAP(XK_M, KEY_M);
        MAP(XK_N, KEY_N);
        MAP(XK_O, KEY_O);
        MAP(XK_P, KEY_P);
        MAP(XK_Q, KEY_Q);
        MAP(XK_R, KEY_R);
        MAP(XK_S, KEY_S);
        MAP(XK_T, KEY_T);
        MAP(XK_U, KEY_U);
        MAP(XK_V, KEY_V);
        MAP(XK_W, KEY_W);
        MAP(XK_X, KEY_X);
        MAP(XK_Y, KEY_Y);
        MAP(XK_Z, KEY_Z);

        MAP(XK_a, KEY_A);
        MAP(XK_b, KEY_B);
        MAP(XK_c, KEY_C);
        MAP(XK_d, KEY_D);
        MAP(XK_e, KEY_E);
        MAP(XK_f, KEY_F);
        MAP(XK_g, KEY_G);
        MAP(XK_h, KEY_H);
        MAP(XK_i, KEY_I);
        MAP(XK_j, KEY_J);
        MAP(XK_k, KEY_K);
        MAP(XK_l, KEY_L);
        MAP(XK_m, KEY_M);
        MAP(XK_n, KEY_N);
        MAP(XK_o, KEY_O);
        MAP(XK_p, KEY_P);
        MAP(XK_q, KEY_Q);
        MAP(XK_r, KEY_R);
        MAP(XK_s, KEY_S);
        MAP(XK_t, KEY_T);
        MAP(XK_u, KEY_U);
        MAP(XK_v, KEY_V);
        MAP(XK_w, KEY_W);
        MAP(XK_x, KEY_X);
        MAP(XK_y, KEY_Y);
        MAP(XK_z, KEY_Z);

        MAP(XK_bracketleft, KEY_LEFTBRACE);
        MAP(XK_backslash, KEY_BACKSLASH);
        MAP(XK_bracketright, KEY_RIGHTBRACE);
        //MAP(XK_asciicircum, KEY_ASCIICIRCUM);
        //MAP(XK_underscore, KEY_UNDERSCORE);
        MAP(XK_grave, KEY_GRAVE);
        //MAP(XK_quoteleft, KEY_QUOTELEFT);

        //MAP(XK_braceleft, KEY_BRACELEFT);
        //MAP(XK_bar, KEY_BAR);
        //MAP(XK_braceright, KEY_BRACERIGHT);
        //MAP(XK_asciitilde, KEY_ASCIITILDE);

        /* Pointer mapping */
        MAP(Button1, BTN_LEFT);
        MAP(Button2, BTN_MIDDLE);
        MAP(Button3, BTN_RIGHT);
        MAP(Button4, BTN_FORWARD);
        MAP(Button5, BTN_BACK);
    }

    return 0;
}
