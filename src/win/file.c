#include <assert.h>

#include "uv.h"
#include "../uv-common.h"
#include "internal.h"

static int uv_set_file_handle(uv_file_t* handle) {
  static const LARGE_INTEGER zero = {0};

  if(CreateIoCompletionPort(handle->handle,
                            LOOP->iocp,
                            (ULONG_PTR)handle,
                            0) == NULL) {
    uv_set_sys_error(GetLastError());
    return -1;
  }
  if(!SetFilePointerEx(handle->handle, zero, &handle->file_pointer, FILE_CURRENT)) {
    uv_set_sys_error(GetLastError());
    return -1;
  }
  return 0;
}

int uv_file_init(uv_file_t* handle, uv_native_file_t native) {
  uv_stream_init((uv_stream_t*)handle);
  handle->reqs_pending = 0;
  handle->type = UV_FILE;
  handle->handle = native;

  uv_counters()->file_init++;

  if(uv_set_file_handle(handle)) {
    uv_set_sys_error(GetLastError());
    return -1;
  }

  handle->flags |= UV_HANDLE_BOUND;

  return 0;
}

void close_file(uv_file_t* file, int* status, uv_err_t* err) {
  CloseHandle(file->handle);
  file->handle = INVALID_HANDLE_VALUE;
  file->flags |= UV_HANDLE_SHUT;
}

void uv_file_endgame(uv_file_t* handle) {
  uv_err_t err;
  int status;

  if (handle->flags & UV_HANDLE_SHUTTING &&
      !(handle->flags & UV_HANDLE_SHUT) &&
      handle->write_reqs_pending == 0) {
    close_file(handle, &status, &err);
    handle->reqs_pending--;
  }

  if (handle->flags & UV_HANDLE_CLOSING &&
      handle->reqs_pending == 0) {
    assert(!(handle->flags & UV_HANDLE_CLOSED));
    handle->flags |= UV_HANDLE_CLOSED;

    if (handle->close_cb) {
      handle->close_cb((uv_handle_t*)handle);
    }

    uv_unref();
  }
}

int uv_file_read(uv_read_t* req,
                 uv_file_t* file,
                 uv_buf_t bufs[],
                 int bufcnt,
                 uv_read_cb read_cb) {
  return uv_file_read_offset(req, file, 0, UV_CURRENT, bufs, bufcnt, read_cb);
}

int uv_file_read_offset(uv_read_t* req,
                        uv_file_t* file,
                        ssize_t offset,
                        uv_offset_disposition disposition,
                        uv_buf_t bufs[],
                        int bufcnt,
                        uv_read_cb read_cb) {
  int result;
  LARGE_INTEGER true_offset = {0};

  if (bufcnt != 1) {
    uv_set_error(UV_ENOTSUP, 0);
    return -1;
  }

  switch (disposition) {
  case UV_START:
    true_offset.QuadPart = offset;
    break;
  case UV_CURRENT:
    true_offset = file->file_pointer;
    true_offset.QuadPart += offset;
    break;
  case UV_END:
    uv_set_error(UV_ENOTSUP, 0);
    return -1;
  }

  uv_req_init((uv_req_t*) req);
  req->type = UV_READ;
  req->handle = (uv_handle_t*)file;
  req->cb = read_cb;
  req->bufs = bufs;
  req->bufcnt = bufcnt;
  memset(&req->overlapped, 0, sizeof(req->overlapped));
  req->overlapped.Offset = true_offset.LowPart;
  req->overlapped.OffsetHigh = true_offset.HighPart;

  /* 
   * I move the pointer "early" so that streaming reads/writes don't 
   * get confused. Submitting the read request/write request should 
   * "reserve" the chunk of file, and prevent any other streaming 
   * requests from accessing the same data.
   *
   * There are still all sorts of thread safety issues with using 
   * the handles on multiple threads simultaneously; this just means 
   * that things won't get fouled up if the operating system services
   * requests out-of-order.
   *
   */
  file->file_pointer.QuadPart += req->bufs[0].len;

  result = ReadFile(file->handle,
                    req->bufs[0].base,
                    req->bufs[0].len,
                    NULL,
                    &req->overlapped);
  if(!result && GetLastError() != ERROR_IO_PENDING) {
    uv_set_sys_error(GetLastError());
    return -1;
  }

  if(result) {
    req->queued_bytes = 0;
  } else {
    req->queued_bytes = uv_count_bufs(bufs, bufcnt);
    file->read_queue_size += req->queued_bytes;
  }

  file->reqs_pending++;
  file->read_reqs_pending++;

  return 0;
}

int uv_file_write(uv_write_t* req,
                  uv_file_t* file,
                  uv_buf_t bufs[],
                  int bufcnt,
                  uv_write_cb cb) {
  return uv_file_write_offset(req, file, 0, UV_CURRENT, bufs, bufcnt, cb);
}

int uv_file_write_offset(uv_write_t* req,
                         uv_file_t* file,
                         ssize_t offset,
                         uv_offset_disposition disposition,
                         uv_buf_t bufs[],
                         int bufcnt,
                         uv_write_cb cb) {
  int result;
  LARGE_INTEGER true_offset = {0};

  if (bufcnt != 1) {
    uv_set_error(UV_ENOTSUP, 0);
    return -1;
  }

  switch (disposition) {
  case UV_START:
    true_offset.QuadPart = offset;
    break;
  case UV_CURRENT:
    true_offset = file->file_pointer;
    true_offset.QuadPart += offset;
    break;
  case UV_END:
    uv_set_error(UV_ENOTSUP, 0);
    return -1;
  }

  uv_req_init((uv_req_t*) req);
  req->type = UV_WRITE;
  req->handle = (uv_handle_t*)file;
  req->cb = cb;
  req->bufs = bufs;
  req->bufcnt = bufcnt;
  memset(&req->overlapped, 0, sizeof(req->overlapped));
  req->overlapped.Offset = true_offset.LowPart;
  req->overlapped.OffsetHigh = true_offset.HighPart;

  file->file_pointer.QuadPart += req->bufs[0].len;

  result = WriteFile(file->handle,
                     req->bufs[0].base,
                     req->bufs[0].len,
                     NULL,
                     &req->overlapped);
  if(!result && GetLastError() != ERROR_IO_PENDING) {
    uv_set_sys_error(GetLastError());
    return -1;
  }

  if(result) {
    req->queued_bytes = 0;
  } else {
    req->queued_bytes = uv_count_bufs(bufs, bufcnt);
    file->write_queue_size += req->queued_bytes;
  }

  file->reqs_pending++;
  file->write_reqs_pending++;

  return 0;
}

void uv_process_file_write_req(uv_file_t* handle, uv_write_t* req) {
  DWORD bytes_transferred;

  assert(handle->type == UV_FILE);

  handle->write_queue_size -= req->queued_bytes;
  if(!GetOverlappedResult(handle->handle, &req->overlapped, &bytes_transferred, FALSE)) {
    bytes_transferred = -1;
  }

  if (req->cb) {
    LOOP->last_error = req->error;
    (req->cb)(req, LOOP->last_error.code == UV_OK ? UV_OK : -1);
  }

  handle->write_reqs_pending--;
  DECREASE_PENDING_REQ_COUNT(handle);
}

void uv_process_file_read_req(uv_file_t* handle, uv_read_t* req) {
  DWORD bytes_transferred;

  assert(handle->type == UV_FILE);

  handle->read_queue_size -= req->queued_bytes;
  if(!GetOverlappedResult(handle->handle, &req->overlapped, &bytes_transferred, FALSE)) {
    bytes_transferred = -1;
  }

  if (req->cb) {
    LOOP->last_error = req->error;
    (req->cb)(req, bytes_transferred, req->bufs[0]);
  }

  handle->read_reqs_pending--;
  DECREASE_PENDING_REQ_COUNT(handle);
}
