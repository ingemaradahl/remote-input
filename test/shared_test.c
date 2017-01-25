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
#include "test/test_suites.h"

#include <check.h>
#include <stdint.h>

#include "shared.h"

START_TEST(test_macro_write_read) {
    uint8_t event[EV_MSG_SIZE];
    EV_MSG_FIELD(event, type) = EV_KEY_UP;
    EV_MSG_FIELD(event, value) = 42;

    ck_assert_uint_eq(EV_MSG_FIELD(event, type), EV_KEY_UP);
    ck_assert_int_eq(EV_MSG_FIELD(event, value), 42);
} END_TEST

START_TEST(test_macro_noclobber) {
    uint8_t event[3][EV_MSG_SIZE] = {{0}, {0}, {0}};

    for (int i = 0; i < 3; i++) {
        ck_assert_uint_eq(EV_MSG_FIELD(event[i], type), 0x0);
        ck_assert_int_eq(EV_MSG_FIELD(event[i], value), 0x0);
    }

    EV_MSG_FIELD(event[1], type) = UINT16_MAX;
    EV_MSG_FIELD(event[1], value) = INT16_MIN;

    ck_assert_uint_eq(EV_MSG_FIELD(event[0], type), 0x0);
    ck_assert_int_eq(EV_MSG_FIELD(event[0], value), 0x0);
    ck_assert_uint_eq(EV_MSG_FIELD(event[1], type), UINT16_MAX);
    ck_assert_int_eq(EV_MSG_FIELD(event[1], value), INT16_MIN);
    ck_assert_uint_eq(EV_MSG_FIELD(event[2], type), 0x0);
    ck_assert_int_eq(EV_MSG_FIELD(event[2], value), 0x0);
} END_TEST

Suite* shared_suite(void) {
    Suite* shared_suite = suite_create("shared.h");
    TCase* shared_testcase = tcase_create("core");

    suite_add_tcase(shared_suite, shared_testcase);
    tcase_add_test(shared_testcase, test_macro_write_read);
    tcase_add_test(shared_testcase, test_macro_noclobber);

    return shared_suite;
}
