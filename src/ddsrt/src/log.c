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
  dds_log_write_fn_t funcs[2];
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
  fwrite (data->message - data->hdrsize, 1, data->hdrsize + data->size + 1, (FILE *) ptr);
  fflush ((FILE *) ptr);
}

static void nop_sink (void *ptr, const dds_log_data_t *data)
{
  (void) ptr;
  (void) data;
  return;
}

#define LOG (0)
#define TRACE (1)
#define USE (0)
#define SET (1)

static struct ddsrt_log_cfg_impl logconfig = {
  .c = {
    .mask = DDS_LC_ERROR | DDS_LC_WARNING,
    .domid = UINT32_MAX
  },
  .sink_fps = {
    [LOG] = NULL,
    [TRACE] = NULL
  }
};

static log_sink_t sinks[2] = {
  [LOG]   = { .funcs = { [USE] = default_sink, [SET] = default_sink }, .ptr = NULL, .out = NULL },
  [TRACE] = { .funcs = { [USE] = default_sink, [SET] = default_sink }, .ptr = NULL, .out = NULL }
};

uint32_t *const dds_log_mask = &logconfig.c.mask;

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

static void set_active_log_sinks (void)
{
  sinks[LOG].funcs[USE] = sinks[LOG].funcs[SET];
  sinks[TRACE].funcs[USE] = sinks[TRACE].funcs[SET];
  if (sinks[LOG].funcs[USE] == sinks[TRACE].funcs[USE])
  {
    if (sinks[LOG].funcs[USE] != default_sink && sinks[LOG].ptr == sinks[TRACE].ptr)
      sinks[LOG].funcs[USE] = nop_sink;
  }
}

static void set_log_sink (log_sink_t *sink, dds_log_write_fn_t func, void *ptr)
{
  assert (sink != NULL);
  assert (sink == &sinks[0] || sink == &sinks[1]);

  /* No life cycle management is done for log sinks, the caller is
     responsible for that. Ensure this operation is deterministic and that on
     return, no thread in the DDS stack still uses the deprecated sink. */
  lock_sink (WRLOCK);
  sink->funcs[SET] = (func != 0) ? func : default_sink;
  sink->ptr = ptr;
  set_active_log_sinks ();
  unlock_sink ();
}

/* dds_set_log_file must be considered private. */
void dds_set_log_file (FILE *file)
{
  lock_sink (WRLOCK);
  logconfig.sink_fps[LOG] = (file == NULL ? stderr : file);
  set_active_log_sinks ();
  unlock_sink ();
}

void dds_set_trace_file (FILE *file)
{
  lock_sink (WRLOCK);
  logconfig.sink_fps[TRACE] = (file == NULL ? stderr : file);
  set_active_log_sinks ();
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
  *dds_log_mask = (cats & (DDS_LOG_MASK | DDS_TRACE_MASK));
  set_active_log_sinks ();
  unlock_sink ();
}

void dds_log_cfg_init (struct ddsrt_log_cfg *cfg, uint32_t domid, uint32_t mask, FILE *log_fp, FILE *trace_fp)
{
  struct ddsrt_log_cfg_impl *cfgimpl = (struct ddsrt_log_cfg_impl *) cfg;
  assert (domid != UINT32_MAX); /* because that's reserved for global use */
  memset (cfgimpl, 0, sizeof (*cfgimpl));
  cfgimpl->c.mask = mask;
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
  return (size_t) cnt;
}

static void vlog (const struct ddsrt_log_cfg_impl *cfg, uint32_t cat, uint32_t domid, const char *file, uint32_t line, const char *func, const char *fmt, va_list ap)
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

  if (*fmt == 0) {
    return;
  }

  lb = &log_buffer;

  /* Thread-local buffer is always initialized with all zeroes. The pos
     member must always be greater or equal to BUF_OFFSET. */
  if (lb->pos < BUF_OFFSET) {
      lb->pos = BUF_OFFSET;
      lb->buf[lb->pos] = 0;
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

  if (fmt[strlen (fmt) - 1] == '\n') {
    size_t hdrsize = print_header (lb->buf, domid);

    data.priority = cat;
    data.file = file;
    data.function = func;
    data.line = line;
    data.message = lb->buf + BUF_OFFSET;
    data.size = strlen(data.message) - 1;
    data.hdrsize = hdrsize;

    dds_log_write_fn_t f = 0;
    void *f_arg = NULL;
    for (size_t i = (cat & DDS_LOG_MASK) ? LOG : TRACE;
                i < sizeof (sinks) / sizeof (sinks[0]);
                i++)
    {
      if (sinks[i].funcs[USE] != default_sink) {
        if (sinks[i].funcs[USE] != f || sinks[i].ptr != f_arg) {
          assert (sinks[i].funcs[USE]);
          sinks[i].funcs[USE] (sinks[i].ptr, &data);
          f = sinks[i].funcs[USE]; f_arg = sinks[i].ptr;
        }
      } else if (cfg->sink_fps[i]) {
        if (default_sink != f || cfg->sink_fps[i] != f_arg) {
          default_sink (cfg->sink_fps[i], &data);
          f = default_sink; f_arg = cfg->sink_fps[i];
        }
      } else if (logconfig.sink_fps[i]) {
        if (default_sink != f || logconfig.sink_fps[i] != f_arg) {
          default_sink (logconfig.sink_fps[i], &data);
          f = default_sink; f_arg = logconfig.sink_fps[i];
        }
      }
    }

    lb->pos = BUF_OFFSET;
    lb->buf[lb->pos] = 0;
  }
}

int dds_log_cfg (const struct ddsrt_log_cfg *cfg, uint32_t cat, const char *file, uint32_t line, const char *func, const char *fmt, ...)
{
  const struct ddsrt_log_cfg_impl *cfgimpl = (const struct ddsrt_log_cfg_impl *) cfg;
  if ((cfgimpl->c.mask & cat) || (cat & DDS_LC_FATAL)) {
    va_list ap;
    va_start (ap, fmt);
    lock_sink (RDLOCK);
    vlog (cfgimpl, cat, cfgimpl->c.domid, file, line, func, fmt, ap);
    unlock_sink ();
    va_end (ap);
  }
  if (cat & DDS_LC_FATAL) {
    abort();
  }
  return 0;
}

int dds_log_id (uint32_t cat, uint32_t id, const char *file, uint32_t line, const char *func, const char *fmt, ...)
{
  if ((dds_get_log_mask () & cat) || (cat & DDS_LC_FATAL)) {
    va_list ap;
    va_start (ap, fmt);
    lock_sink (RDLOCK);
    vlog (&logconfig, cat, id, file, line, func, fmt, ap);
    unlock_sink ();
    va_end (ap);
  }
  if (cat & DDS_LC_FATAL) {
    abort ();
  }
  return 0;
}

int dds_log (uint32_t cat, const char *file, uint32_t line, const char *func, const char *fmt, ...)
{
  if ((dds_get_log_mask () & cat) || (cat & DDS_LC_FATAL)) {
    va_list ap;
    va_start (ap, fmt);
    lock_sink (RDLOCK);
    vlog (&logconfig, cat, UINT32_MAX, file, line, func, fmt, ap);
    unlock_sink ();
    va_end (ap);
  }
  if (cat & DDS_LC_FATAL) {
    abort ();
  }
  return 0;
}
