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

#include "uv.h"
#include "task.h"
#include <stdio.h>
#include <stdlib.h>

wchar_t* make_program_args(char** args, int verbatim_arguments);
wchar_t* quote_cmd_arg(const wchar_t *source, wchar_t *target);

TEST_IMPL(argument_escaping) {
  const wchar_t* test_str[] = {
    L"HelloWorld",
    L"Hello World",
    L"Hello\"World",
    L"Hello World\\",
    L"Hello\\\"World",
    L"Hello\\World",
    L"Hello\\\\World",
    L"Hello World\\",
    L"c:\\path\\to\\node.exe --eval \"require('c:\\\\path\\\\to\\\\test.js')\""
  };
  const int count = sizeof(test_str) / sizeof(*test_str);
  wchar_t** test_output;
  wchar_t* command_line;
  wchar_t** cracked;
  size_t total_size = 0;
  int i;
  int num_args;

  char* verbatim[] = {
    "cmd.exe",
    "/c",
    "c:\\path\\to\\node.exe --eval \"require('c:\\\\path\\\\to\\\\test.js')\"",
    NULL
  };
  wchar_t* verbatim_output;
  wchar_t* non_verbatim_output;

  test_output = calloc(count, sizeof(wchar_t*));
  for (i = 0; i < count; ++i) {
    test_output[i] = calloc(2 * (wcslen(test_str[i]) + 2), sizeof(wchar_t));
    quote_cmd_arg(test_str[i], test_output[i]);
    wprintf(L"input : %s\n", test_str[i]);
    wprintf(L"output: %s\n", test_output[i]);
    total_size += wcslen(test_output[i]) + 1;
  }
  command_line = calloc(total_size + 1, sizeof(wchar_t));
  for (i = 0; i < count; ++i) {
    wcscat(command_line, test_output[i]);
    wcscat(command_line, L" ");
  }
  command_line[total_size - 1] = L'\0';

  wprintf(L"command_line: %s\n", command_line);

  cracked = CommandLineToArgvW(command_line, &num_args);
  for (i = 0; i < num_args; ++i) {
    wprintf(L"%d: %s\t%s\n", i, test_str[i], cracked[i]);
    ASSERT(wcscmp(test_str[i], cracked[i]) == 0);
  }

  LocalFree(cracked);
  for (i = 0; i < count; ++i) {
    free(test_output[i]);
  }

  verbatim_output = make_program_args(verbatim, 1);
  non_verbatim_output = make_program_args(verbatim, 0);

  wprintf(L"    verbatim_output: %s\n", verbatim_output);
  wprintf(L"non_verbatim_output: %s\n", non_verbatim_output);

  ASSERT(wcscmp(verbatim_output, L"cmd.exe /c c:\\path\\to\\node.exe --eval \"require('c:\\\\path\\\\to\\\\test.js')\"") == 0);
  ASSERT(wcscmp(non_verbatim_output, L"cmd.exe /c \"c:\\path\\to\\node.exe --eval \\\"require('c:\\\\path\\\\to\\\\test.js')\\\"\"") == 0);

  return 0;
}
