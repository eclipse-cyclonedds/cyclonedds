/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "dds/ddsrt/log.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/threads.h"

#define MAX_TIMESTAMP_LEN (10 + 1 + 6)
#define MAX_TID_LEN (10)
#define HDR_LEN (MAX_TIMESTAMP_LEN + 1 + MAX_TID_LEN + 2)

#define BUF_OFFSET HDR_LEN

typedef struct {
  char buf[2048];
  size_t pos;
} log_buffer_t;

typedef struct {
  dds_log_write_fn_t funcs[2];
  void *ptr;
  FILE *out;
} log_sink_t;

static ddsrt_thread_local log_buffer_t log_buffer;

static ddsrt_once_t lock_inited = DDSRT_ONCE_INIT;
static ddsrt_rwlock_t lock;

static uint32_t log_mask = DDS_LC_ERROR | DDS_LC_WARNING;

static void default_sink(void *ptr, const dds_log_data_t *data)
{
  fwrite(data->message - HDR_LEN, 1, HDR_LEN + data->size + 1, (FILE *)ptr);
  fflush((FILE *)ptr);
}

static void nop_sink(void *ptr, const dds_log_data_t *data)
{
  (void)ptr;
  (void)data;
  return;
}

#define LOG (0)
#define TRACE (1)
#define USE (0)
#define SET (1)

static log_sink_t sinks[] = {
  /* Log */
  { .funcs = { default_sink, default_sink }, .ptr = NULL, .out = NULL },
  /* Trace */
  { .funcs = { nop_sink,     default_sink }, .ptr = NULL, .out = NULL }
};

uint32_t *const dds_log_mask = &log_mask;

#define RDLOCK (1)
#define WRLOCK (2)

static void init_lock(void)
{
  ddsrt_rwlock_init(&lock);
  sinks[LOG].ptr = sinks[TRACE].ptr = stderr;
  sinks[LOG].out = sinks[TRACE].out = stderr;
}

static void lock_sink(int type)
{
  assert(type == RDLOCK || type == WRLOCK);
  ddsrt_once(&lock_inited, &init_lock);

  if (type == RDLOCK) {
    ddsrt_rwlock_read(&lock);
  } else {
    ddsrt_rwlock_write(&lock);
  }
}

static void unlock_sink(void)
{
  ddsrt_rwlock_unlock(&lock);
}

static void set_active_log_sinks(void)
{
  if (dds_get_log_mask() & DDS_LOG_MASK) {
    sinks[LOG].funcs[USE] = sinks[LOG].funcs[SET];
  } else {
    sinks[LOG].funcs[USE] = nop_sink;
  }
  if (dds_get_log_mask() & DDS_TRACE_MASK) {
    sinks[TRACE].funcs[USE] = sinks[TRACE].funcs[SET];
  } else {
    sinks[TRACE].funcs[USE] = nop_sink;
  }
  if (sinks[LOG].funcs[USE] == sinks[TRACE].funcs[USE] &&
      sinks[LOG].ptr == sinks[TRACE].ptr)
  {
    sinks[LOG].funcs[USE] = nop_sink;
  }
}

static void
set_log_sink(
  log_sink_t *sink,
  dds_log_write_fn_t func,
  void *ptr)
{
  assert(sink != NULL);

  /* No life cycle management is done for log sinks, the caller is
     responsible for that. Ensure this operation is deterministic and that on
     return, no thread in the DDS stack still uses the deprecated sink. */
  lock_sink(WRLOCK);

  if (func == 0) {
    sink->funcs[SET] = default_sink;
    sink->ptr = sink->out;
  } else {
    sink->funcs[SET] = func;
    sink->ptr = ptr;
  }

  set_active_log_sinks();
  unlock_sink();
}

/* dds_set_log_file must be considered private. */
void dds_set_log_file(FILE *file)
{
  lock_sink(WRLOCK);
  sinks[LOG].out = (file == NULL ? stderr : file);
  if (sinks[LOG].funcs[SET] == default_sink) {
    sinks[LOG].ptr = sinks[LOG].out;
  }
  set_active_log_sinks();
  unlock_sink();
}

void dds_set_trace_file(FILE *file)
{
  lock_sink(WRLOCK);
  sinks[TRACE].out = (file == NULL ? stderr : file);
  if (sinks[TRACE].funcs[SET] == default_sink) {
    sinks[TRACE].ptr = sinks[TRACE].out;
  }
  set_active_log_sinks();
  unlock_sink();
}

void dds_set_log_sink(
  dds_log_write_fn_t callback,
  void *userdata)
{
  set_log_sink(&sinks[LOG], callback, userdata);
}

void dds_set_trace_sink(
  dds_log_write_fn_t callback,
  void *userdata)
{
  set_log_sink(&sinks[TRACE], callback, userdata);
}

extern inline uint32_t
dds_get_log_mask(void);

void
dds_set_log_mask(uint32_t cats)
{
  lock_sink(WRLOCK);
  *dds_log_mask = (cats & (DDS_LOG_MASK | DDS_TRACE_MASK));
  set_active_log_sinks();
  unlock_sink();
}

static void print_header(char *str)
{
  int cnt;
  char *tid, buf[MAX_TID_LEN+1] = { 0 };
  static const char fmt[] = "%10u.%06d/%*.*s:";
  dds_time_t time;
  unsigned sec;
  int usec;

  (void)ddsrt_thread_getname(buf, sizeof(buf));
  tid = (buf[0] == '\0' ? "(anon)" : buf);
  time = dds_time();
  sec = (unsigned)(time / DDS_NSECS_IN_SEC);
  usec = (int)((time % DDS_NSECS_IN_SEC) / DDS_NSECS_IN_USEC);

  cnt = snprintf(
    str, HDR_LEN, fmt, sec, usec, MAX_TID_LEN, MAX_TID_LEN, tid);
  assert(cnt == (HDR_LEN - 1));
  str[cnt] = ' '; /* Replace snprintf null byte by space. */
}

static void vlog(
  uint32_t cat,
  const char *file,
  uint32_t line,
  const char *func,
  const char *fmt,
  va_list ap)
{
  int n, trunc = 0;
  size_t nrem;
  log_buffer_t *lb;
  dds_log_data_t data;

  if (*fmt == 0) {
    return;
  }

  lock_sink(RDLOCK);
  lb = &log_buffer;

  /* Thread-local buffer is always initialized with all zeroes. The pos
     member must always be greater or equal to BUF_OFFSET. */
  if (lb->pos < BUF_OFFSET) {
      lb->pos = BUF_OFFSET;
      lb->buf[lb->pos] = 0;
  }
  nrem = sizeof (lb->buf) - lb->pos;
  if (nrem > 0) {
    n = vsnprintf(lb->buf + lb->pos, nrem, fmt, ap);
    if (n >= 0 && (size_t) n < nrem) {
      lb->pos += (size_t) n;
    } else {
      lb->pos += nrem;
      trunc = 1;
    }
    if (trunc) {
      static const char msg[] = "(trunc)\n";
      const size_t msglen = sizeof (msg) - 1;
      assert(lb->pos <= sizeof (lb->buf));
      assert(lb->pos >= msglen);
      memcpy(lb->buf + lb->pos - msglen, msg, msglen);
    }
  }

  if (fmt[strlen (fmt) - 1] == '\n') {
    print_header(lb->buf);

    data.priority = cat;
    data.file = file;
    data.function = func;
    data.line = line;
    data.message = lb->buf + BUF_OFFSET;
    data.size = strlen(data.message) - 1;

    for (size_t i = (cat & DDS_LOG_MASK) ? LOG : TRACE;
                i < sizeof(sinks) / sizeof(sinks[0]);
                i++)
    {
      sinks[i].funcs[USE](sinks[i].ptr, &data);
    }

    lb->pos = BUF_OFFSET;
    lb->buf[lb->pos] = 0;
  }

  unlock_sink();
}

int
dds_log(
  uint32_t cat,
  const char *file,
  uint32_t line,
  const char *func,
  const char *fmt,
  ...)
{
  if ((dds_get_log_mask() & cat) || (cat & DDS_LC_FATAL)) {
    va_list ap;
    va_start(ap, fmt);
    vlog(cat, file, line, func, fmt, ap);
    va_end(ap);
  }
  if (cat & DDS_LC_FATAL) {
    abort();
  }

  return 0;
}
