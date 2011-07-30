/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdio.h>
#include <string.h>

#include "runner.h"
#include "task.h"

/* Actual tests and helpers are defined in test-list.h */
#include "test-list.h"


/* The time in milliseconds after which a single test times out. */
#define TEST_TIMEOUT  25000


int main(int argc, char **argv) {
  char buffer[32];
  platform_init(argc, argv);

  switch (argc) {
  case 1: return run_tests(TEST_TIMEOUT, 0);
  case 2: {
    if (strcmp(argv[1], "spawn_helper1") == 0) {
      return 1;
    }

    if (strcmp(argv[1], "spawn_helper2") == 0) {
      printf("hello world\n");
      return 1;
    }

    if (strcmp(argv[1], "spawn_helper3") == 0) {
      gets(buffer);
      printf(buffer);
      return 1;
    }

    if (strcmp(argv[1], "spawn_helper4") == 0) {
      // sleep
      uv_sleep(10000);
      return 100;
    }

    return run_test(argv[1], TEST_TIMEOUT, 0);
  }
  case 3: return run_test_part(argv[1], argv[2]);
  default:
    LOGF("Too many arguments.\n");
    return 1;
  }
}
