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
#include <check.h>

#include "test/test_suites.h"

int main(int argc, char* argv[]) {
    SRunner* runner = srunner_create(server_suite());
    srunner_add_suite(runner, shared_suite());

    srunner_run_all(runner, CK_ENV);
    int number_failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return number_failed > 0;
}
