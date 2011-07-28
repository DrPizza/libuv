#include "uv.h"
#include "task.h"
#include <stdio.h>
#include <stdlib.h>

static uv_file_t the_file;

static int close_called = 0;
static int read_called = 0;
static int write_called = 0;

static void after_close(uv_handle_t* handle) {
  ++close_called;
}

static void after_read(uv_read_t* req, ssize_t nread, uv_buf_t buf) {
  ASSERT(nread == buf.len);
  free(req->bufs[0].base);

  ++read_called;
  if(read_called == 3 && write_called == 2) {
    uv_close(req->handle, &after_close);
  }
}

static void after_write(uv_write_t* req, int status) {
  ASSERT(status == UV_OK);
  free(req->bufs[0].base);

  ++write_called;
  if(read_called == 3 && write_called == 2) {
    uv_close(req->handle, &after_close);
  }
}

TEST_IMPL(file_io) {
  int r;
  WCHAR path[MAX_PATH], filename[MAX_PATH];
  HANDLE test_file;
  LARGE_INTEGER position;
  int file_created = 0;
  uv_read_t* read_reqs;
  uv_buf_t* read_buffers;
  uv_write_t* write_reqs;
  uv_buf_t* write_buffers;
  ssize_t read_offsets[] = { 0, 512 * 1024 * 1024, 1023 * 1024 * 1024 };
  ssize_t write_offsets[] = { 256 * 1024 * 1024, 768 * 1024 * 1024 };
  int i;

  uv_init();

  if (GetTempPathW(sizeof(path) / sizeof(WCHAR), path) == 0)
    goto error;
  if (GetTempFileNameW(path, L"uv", 0, filename) == 0)
    goto error;

  test_file = CreateFileW(filename, GENERIC_ALL, 0, NULL, CREATE_ALWAYS, 0, NULL);
  if (test_file == INVALID_HANDLE_VALUE) {
    goto error;
  }
  file_created = 1;

  position.QuadPart = 1024 * 1024 * 1024;
  if (!SetFilePointerEx(test_file, position, NULL, FILE_BEGIN)) {
    goto error;
  }
  if (!SetEndOfFile(test_file) ) {
    goto error;
  }

  CloseHandle(test_file);

  test_file = CreateFileW(filename, GENERIC_ALL, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_DELETE_ON_CLOSE, NULL);
  if (test_file == INVALID_HANDLE_VALUE) {
    goto error;
  }

  file_created = 0;

  r = uv_file_init(&the_file, test_file);
  ASSERT(r == 0);

  read_reqs = calloc(3, sizeof(uv_read_t));
  read_buffers = calloc(3, sizeof(uv_buf_t));
  for(i = 0; i < 3; ++i) {
    read_buffers[i].len = 1024 * 1024;
    read_buffers[i].base = calloc(read_buffers[i].len, 1);
  }
  write_reqs = calloc(2, sizeof(uv_write_t));
  write_buffers = calloc(2, sizeof(uv_buf_t));
  for(i = 0; i < 2; ++i) {
    write_buffers[i].len = 1024 * 1024;
    write_buffers[i].base = calloc(write_buffers[i].len, 1);
  }

  for(i = 0; i < 3; ++i) {
    r = uv_file_read_offset(&read_reqs[i], &the_file, read_offsets[i], UV_START, &read_buffers[i], 1, &after_read);
    ASSERT(r == 0);
  }
  for(i = 0; i < 2; ++i) {
    r = uv_file_write_offset(&write_reqs[i], &the_file, write_offsets[i], UV_START, &write_buffers[i], 1, &after_write);
    ASSERT(r == 0);
  }

  r = uv_run();
  ASSERT(r == 0);

  ASSERT(read_called == 3);
  ASSERT(write_called == 2);
  ASSERT(close_called == 1);

  free(write_buffers);
  free(write_reqs);
  free(read_buffers);
  free(read_reqs);

  return 0;

error:
  if (test_file != INVALID_HANDLE_VALUE) {
    CloseHandle(test_file);
  }
  if (file_created) {
    DeleteFileW(filename);
  }
  return -1;
}
