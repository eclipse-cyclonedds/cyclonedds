// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "dds/ddsrt/log.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/static_assert.h"

#define MAX_ID_LEN (10)
#define MAX_TIMESTAMP_LEN (10 + 1 + 6)
#define MAX_TID_LEN (10)
#define HDR_LEN (MAX_TIMESTAMP_LEN + 2 + MAX_ID_LEN + 2 + MAX_TID_LEN + 2)

#define BUF_OFFSET HDR_LEN

typedef struct {
  char buf[2048];
  size_t pos;
} log_buffer_t;

typedef struct {
  dds_log_write_fn_t func;
  void *ptr;
  FILE *out;
} log_sink_t;

static ddsrt_thread_local log_buffer_t log_buffer;

static ddsrt_once_t lock_inited = DDSRT_ONCE_INIT;
static ddsrt_rwlock_t lock;

struct ddsrt_log_cfg_impl {
  struct ddsrt_log_cfg_common c;
  FILE *sink_fps[2];
};

DDSRT_STATIC_ASSERT (sizeof (struct ddsrt_log_cfg_impl) <= sizeof (struct ddsrt_log_cfg));

static void default_sink (void *ptr, const dds_log_data_t *data)
{
  if (ptr)
  {
    (void) fwrite (data->message - data->hdrsize, 1, data->hdrsize + data->size + 1, (FILE *) ptr);
    fflush ((FILE *) ptr);
  }
}

#define LOG (0)
#define TRACE (1)

static struct ddsrt_log_cfg_impl logconfig = {
  .c = {
    .mask = DDS_LC_ERROR | DDS_LC_WARNING | DDS_LC_FATAL,
    .tracemask = 0,
    .domid = UINT32_MAX
  },
  .sink_fps = {
    [LOG] = NULL,
    [TRACE] = NULL
  }
};

static log_sink_t sinks[2] = {
  [LOG]   = { .func = default_sink, .ptr = NULL, .out = NULL },
  [TRACE] = { .func = default_sink, .ptr = NULL, .out = NULL }
};

uint32_t * const dds_log_mask = &logconfig.c.mask;

static void init_lock (void)
{
  ddsrt_rwlock_init (&lock);
  sinks[LOG].ptr = sinks[TRACE].ptr = stderr;
  sinks[LOG].out = sinks[TRACE].out = stderr;
  logconfig.sink_fps[LOG] = sinks[LOG].ptr;
  logconfig.sink_fps[TRACE] = sinks[TRACE].ptr;
}

enum lock_type { RDLOCK, WRLOCK };

static void lock_sink (enum lock_type type)
{
  ddsrt_once (&lock_inited, &init_lock);
  if (type == RDLOCK) {
    ddsrt_rwlock_read(&lock);
  } else {
    ddsrt_rwlock_write(&lock);
  }
}

static void unlock_sink (void)
{
  ddsrt_rwlock_unlock (&lock);
}

static void set_log_sink (log_sink_t *sink, dds_log_write_fn_t func, void *ptr)
{
  assert (sink != NULL);
  assert (sink == &sinks[0] || sink == &sinks[1]);

  /* No life cycle management is done for log sinks, the caller is
     responsible for that. Ensure this operation is deterministic and that on
     return, no thread in the DDS stack still uses the deprecated sink. */
  lock_sink (WRLOCK);
  sink->func = (func != 0) ? func : default_sink;
  sink->ptr = ptr;
  unlock_sink ();
}

/* dds_set_log_file must be considered private. */
void dds_set_log_file (FILE *file)
{
  lock_sink (WRLOCK);
  logconfig.sink_fps[LOG] = (file == NULL ? stderr : file);
  unlock_sink ();
}

void dds_set_trace_file (FILE *file)
{
  lock_sink (WRLOCK);
  logconfig.sink_fps[TRACE] = (file == NULL ? stderr : file);
  unlock_sink ();
}

void dds_set_log_sink (dds_log_write_fn_t callback, void *userdata)
{
  set_log_sink (&sinks[LOG], callback, userdata);
}

void dds_set_trace_sink (dds_log_write_fn_t callback, void *userdata)
{
  set_log_sink (&sinks[TRACE], callback, userdata);
}

extern inline uint32_t dds_get_log_mask (void);

void dds_set_log_mask (uint32_t cats)
{
  lock_sink (WRLOCK);
  logconfig.c.tracemask = cats & DDS_TRACE_MASK;
  logconfig.c.mask = (cats & (DDS_LOG_MASK | DDS_TRACE_MASK)) | DDS_LC_FATAL;
  unlock_sink ();
}

void dds_log_cfg_init (struct ddsrt_log_cfg *cfg, uint32_t domid, uint32_t tracemask, FILE *log_fp, FILE *trace_fp)
{
  struct ddsrt_log_cfg_impl *cfgimpl = (struct ddsrt_log_cfg_impl *) cfg;
  assert (domid != UINT32_MAX); /* because that's reserved for global use */
  memset (cfgimpl, 0, sizeof (*cfgimpl));
  cfgimpl->c.mask = tracemask | DDS_LOG_MASK;
  cfgimpl->c.tracemask = tracemask;
  cfgimpl->c.domid = domid;
  cfgimpl->sink_fps[LOG] = log_fp;
  cfgimpl->sink_fps[TRACE] = trace_fp;
}

static size_t print_header (char *str, uint32_t id)
{
  int cnt, off;
  char *tid, buf[MAX_TID_LEN+1] = { 0 };
  static const char fmt_no_id[] = "%10u.%06d [] %*.*s:";
  static const char fmt_with_id[] = "%10u.%06d [%"PRIu32"] %*.*s:";
  dds_time_t time;
  unsigned sec;
  int usec;

  (void) ddsrt_thread_getname (buf, sizeof (buf));
  tid = (buf[0] == '\0' ? "(anon)" : buf);
  time = dds_time ();
  sec = (unsigned) (time / DDS_NSECS_IN_SEC);
  usec = (int) ((time % DDS_NSECS_IN_SEC) / DDS_NSECS_IN_USEC);

  if (id == UINT32_MAX)
  {
    off = MAX_ID_LEN;
    cnt = snprintf (str + off, HDR_LEN, fmt_no_id, sec, usec, MAX_TID_LEN, MAX_TID_LEN, tid);
  }
  else
  {
    /* low domain ids tend to be most used from what I have seen */
    off = 9;
    if (id >= 10)
      for (uint32_t thres = 10; off > 0 && id >= thres; off--, thres *= 10);
    cnt = snprintf (str + off, HDR_LEN, fmt_with_id, sec, usec, id, MAX_TID_LEN, MAX_TID_LEN, tid);
  }
  assert (off + cnt == (HDR_LEN - 1));
  str[off + cnt] = ' '; /* Replace snprintf null byte by space. */
  return (size_t) (cnt + 1);
}

static void vlog1 (const struct ddsrt_log_cfg_impl *cfg, uint32_t cat, uint32_t domid, const char *file, uint32_t line, const char *func, const char *fmt, va_list ap)
{
  int n, trunc = 0;
  size_t nrem;
  log_buffer_t *lb;
  dds_log_data_t data;

  /* id can be used to override the id in logconfig, so that the global
     logging configuration can be used for reporting errors while inlcuding
     a domain id.  This simply verifies that the id override is only ever
     used with the global one. */
  assert (domid == cfg->c.domid || cfg == &logconfig);

  lb = &log_buffer;

  /* Thread-local buffer is always initialized with all zeroes. The pos
     member must always be greater or equal to BUF_OFFSET. */
  if (lb->pos < BUF_OFFSET) {
    lb->pos = BUF_OFFSET;
    lb->buf[lb->pos] = 0;
  }

  /* drop any prefix of new lines if there is current no data in the buffer:
     there are some tricky problems in tracing some details depending on
     enabled categories (like which subset of discovery related data gets
     traced), and it sometimes helps to be able to trace just a newline
     knowing it won't have any effect if nothing is buffered */
  if (lb->pos == BUF_OFFSET) {
    while (*fmt == '\n')
      fmt++;
  }
  if (*fmt == 0) {
    return;
  }

  nrem = sizeof (lb->buf) - lb->pos;
  if (nrem > 0) {
    n = vsnprintf (lb->buf + lb->pos, nrem, fmt, ap);
    if (n >= 0 && (size_t) n < nrem) {
      lb->pos += (size_t) n;
    } else {
      lb->pos += nrem;
      trunc = 1;
    }
    if (trunc) {
      static const char msg[] = "(trunc)\n";
      const size_t msglen = sizeof (msg) - 1;
      assert (lb->pos <= sizeof (lb->buf));
      assert (lb->pos >= msglen);
      memcpy (lb->buf + lb->pos - msglen, msg, msglen);
    }
  }

  if (fmt[strlen (fmt) - 1] == '\n' && lb->pos > BUF_OFFSET + 1) {
    assert (lb->pos > BUF_OFFSET);
    size_t hdrsize = print_header (lb->buf, domid);

    data.priority = cat;
    data.file = file;
    data.function = func;
    data.line = line;
    data.message = lb->buf + BUF_OFFSET;
    data.size = lb->pos - BUF_OFFSET - 1;
    data.hdrsize = hdrsize;

    dds_log_write_fn_t f = 0;
    void *f_arg = NULL;
    if (cat & DDS_LOG_MASK)
    {
      f = sinks[LOG].func;
      f_arg = (f == default_sink) ? cfg->sink_fps[LOG] : sinks[LOG].ptr;
      assert (f != 0);
      f (f_arg, &data);
    }
    /* if tracing is enabled, then print to trace if it matches the
       trace flags or if it got written to the log
       (mask == (tracemask | DDS_LOG_MASK)) */
    if (cfg->c.tracemask && (cat & cfg->c.mask))
    {
      dds_log_write_fn_t const g = sinks[TRACE].func;
      void * const g_arg = (g == default_sink) ? cfg->sink_fps[TRACE] : sinks[TRACE].ptr;
      assert (g != 0);
      if (g != f || g_arg != f_arg)
        g (g_arg, &data);
    }

    lb->pos = BUF_OFFSET;
    lb->buf[lb->pos] = 0;
  }
}

static void vlog (const struct ddsrt_log_cfg_impl *cfg, uint32_t cat, uint32_t domid, const char *file, uint32_t line, const char *func, const char *fmt, va_list ap)
{
  lock_sink (RDLOCK);
  vlog1 (cfg, cat, domid, file, line, func, fmt, ap);
  unlock_sink ();
  if (cat & DDS_LC_FATAL)
    abort();
}

void dds_log_cfg (const struct ddsrt_log_cfg *cfg, uint32_t cat, const char *file, uint32_t line, const char *func, const char *fmt, ...)
{
  const struct ddsrt_log_cfg_impl *cfgimpl = (const struct ddsrt_log_cfg_impl *) cfg;
  /* cfgimpl->c.mask is too weak a test because it has all DDS_LOG_MASK bits set,
     rather than just the ones in dds_get_log_mask() (so as not to cache the latter
     and have to keep them synchronized */
  if ((cfgimpl->c.mask & cat) && ((dds_get_log_mask () | cfgimpl->c.tracemask) & cat)) {
    va_list ap;
    va_start (ap, fmt);
    vlog (cfgimpl, cat, cfgimpl->c.domid, file, line, func, fmt, ap);
    va_end (ap);
  }
}

void dds_log_id (uint32_t cat, uint32_t id, const char *file, uint32_t line, const char *func, const char *fmt, ...)
{
  if (dds_get_log_mask () & cat) {
    va_list ap;
    va_start (ap, fmt);
    vlog (&logconfig, cat, id, file, line, func, fmt, ap);
    va_end (ap);
  }
}

void dds_log (uint32_t cat, const char *file, uint32_t line, const char *func, const char *fmt, ...)
{
  if (dds_get_log_mask () & cat) {
    va_list ap;
    va_start (ap, fmt);
    vlog (&logconfig, cat, UINT32_MAX, file, line, func, fmt, ap);
    va_end (ap);
  }
}
