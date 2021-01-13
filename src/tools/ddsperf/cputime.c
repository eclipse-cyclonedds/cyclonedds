/*
 * Copyright(c) 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#define _ISOC99_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "dds/dds.h"

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/rusage.h"

#include "cputime.h"
#include "ddsperf_types.h"

static void print_one (FILE *fp, char *line, size_t sz, size_t *pos, const char *name, double du, double ds)
{
  if (*pos < sz)
  {
    if (fp == NULL)
    {
      *pos += (size_t) snprintf (line + *pos, sz - *pos, " %s:%.0f%%+%.0f%%", name, 100.0 * du, 100.0 * ds);
    }
    else
    {
      *pos += (size_t) snprintf (line + *pos, sz - *pos, ",%s,%.0f,%.0f", name, 100.0 * du, 100.0 * ds);
    }
  }
}

static void print_one_empty_entry (char *line, size_t sz, size_t *pos)
{
  *pos += (size_t) snprintf (line + *pos, sz - *pos, ",,,");
}

#if DDSRT_HAVE_RUSAGE && DDSRT_HAVE_THREAD_LIST

struct record_cputime_state_thr {
  ddsrt_thread_list_id_t tid;
  char name[32];
  double ut, st;
};

struct record_cputime_state {
  bool supported;
  dds_time_t tprev;
  uint32_t vcswprev;
  uint32_t ivcswprev;
  size_t nthreads;
  struct record_cputime_state_thr *threads;
  dds_entity_t wr;
  struct CPUStats s;
};

static void update (double *ut_old, double *st_old, double dt, double ut_new, double st_new, double *du, double *ds)
{
  *du = (ut_new - *ut_old) / dt;
  *ds = (st_new - *st_old) / dt;
  *ut_old = ut_new;
  *st_old = st_new;
}

static bool above_threshold (double *max, double *du_skip, double *ds_skip, double du, double ds)
{
  if (*max < du) *max = du;
  if (*max < ds) *max = ds;
  if (du >= 0.005 || ds >= 0.005)
    return true;
  else if (du_skip == NULL || ds_skip == NULL)
    return false;
  else
  {
    *du_skip += du;
    *ds_skip += ds;
    return false;
  }
}

bool record_cputime (FILE *fp, struct record_cputime_state *state, const char *prefix, dds_time_t tnow, bool data_changed, bool *cputime_changed)
{
  if (state == NULL)
    return false;

  ddsrt_rusage_t usage;
  if (ddsrt_getrusage (DDSRT_RUSAGE_SELF, &usage) < 0)
  {
    usage.maxrss = 0;
    usage.nvcsw = usage.nivcsw = 0;
  }
  double max = 0;
  double du_skip = 0.0, ds_skip = 0.0;
  const double dt = (double) (tnow - state->tprev) / 1e9;
  bool some_above = false;

  state->s.maxrss = (double) usage.maxrss;
  state->s.vcsw = (uint32_t) ((double) (usage.nvcsw - state->vcswprev) / dt + 0.5);
  state->s.ivcsw = (uint32_t) ((double) (usage.nivcsw - state->ivcswprev) / dt + 0.5);
  state->vcswprev = (uint32_t) usage.nvcsw;
  state->ivcswprev = (uint32_t) usage.nivcsw;
  state->s.cpu._length = 0;
  for (size_t i = 0; i < state->nthreads; i++)
  {
    struct record_cputime_state_thr * const thr = &state->threads[i];
    if (ddsrt_getrusage_anythread (thr->tid, &usage) < 0)
    {
      continue;
    }

    const double ut = (double) usage.utime / 1e9;
    const double st = (double) usage.stime / 1e9;
    double du, ds;
    update (&thr->ut, &thr->st, dt, ut, st, &du, &ds);
    if (above_threshold (&max, &du_skip, &ds_skip, du, ds))
    {
      some_above = true;
      /* Thread names are often set by thread itself immediately after creation,
         and so it depends on the scheduling whether there is still a default
         name or the name we are interested in.  Lazily retrieving the name the
         first time the thread pops up in the CPU usage works around the timing
         problem. */
      if (thr->name[0] == 0)
        if (ddsrt_thread_getname_anythread (thr->tid, thr->name, sizeof (thr->name)) < 0)
        {
          du_skip += du;
          ds_skip += ds;
          continue;
        }

      struct CPUStatThread * const x = &state->s.cpu._buffer[state->s.cpu._length++];
      x->name = thr->name;
      x->tid = (uint32_t) thr->tid;
      x->u_pct = (int) (100.0 * du + 0.5);
      x->s_pct = (int) (100.0 * ds + 0.5);
    }
  }
  if (above_threshold (&max, NULL, NULL, du_skip, ds_skip))
  {
    struct CPUStatThread * const x = &state->s.cpu._buffer[state->s.cpu._length++];
    some_above = true;
    x->name = "others";
    x->u_pct = (int) (100.0 * du_skip + 0.5);
    x->s_pct = (int) (100.0 * ds_skip + 0.5);
  }
  state->tprev = tnow;
  state->s.some_above = some_above;
  (void) dds_write (state->wr, &state->s);
  return print_cputime (fp, &state->s, state, prefix, false, true, data_changed, cputime_changed, false);
}

double record_cputime_read_rss (const struct record_cputime_state *state)
{
  return state->s.maxrss;
}

struct record_cputime_state *record_cputime_new (dds_entity_t wr)
{
  ddsrt_thread_list_id_t tids[100];
  dds_return_t n;
  if ((n = ddsrt_thread_list (tids, sizeof (tids) / sizeof (tids[0]))) <= 0)
  {
    fprintf (stderr, "ddsrt_thread_list returned %d\n", n);
    return NULL;
  }
  else if (n > (dds_return_t) (sizeof (tids) / sizeof (tids[0])))
  {
    fprintf (stderr, "way more threads than expected\n");
    return NULL;
  }

  printf("%s: number of threads: %d\n", __func__, (int)n);
  struct record_cputime_state *state = malloc (sizeof (*state));
  ddsrt_rusage_t usage;
  if (ddsrt_getrusage (DDSRT_RUSAGE_SELF, &usage) < 0)
    usage.nvcsw = usage.nivcsw = 0;
  state->tprev = dds_time ();
  state->wr = wr;
  state->vcswprev = (uint32_t) usage.nvcsw;
  state->ivcswprev = (uint32_t) usage.nivcsw;
  state->threads = malloc ((size_t) n * sizeof (*state->threads));
  state->nthreads = 0;
  for (int32_t i = 0; i < n; i++)
  {
    struct record_cputime_state_thr * const thr = &state->threads[state->nthreads];
    if (ddsrt_getrusage_anythread (tids[i], &usage) < 0)
      continue;
    thr->tid = tids[i];
    thr->name[0] = 0;
    thr->ut = (double) usage.utime / 1e9;
    thr->st = (double) usage.stime / 1e9;
    state->nthreads++;
  }

  char hostname[128];
  if (ddsrt_gethostname (hostname, sizeof (hostname)) != DDS_RETCODE_OK)
    strcpy (hostname, "?");
  state->s.hostname = ddsrt_strdup (hostname);
  state->s.pid = (uint32_t) ddsrt_getpid ();
  state->s.cpu._length = 0;
  state->s.cpu._maximum = (uint32_t) state->nthreads;
  state->s.cpu._buffer = malloc (state->s.cpu._maximum * sizeof (*state->s.cpu._buffer));
  state->s.cpu._release = false;
  return state;
}

void record_cputime_free (struct record_cputime_state *state)
{
  if (state)
  {
    free (state->threads);
    ddsrt_free (state->s.hostname);
    /* we alias thread names in state->s->cpu._buffer, so no need to free */
    free (state->s.cpu._buffer);
    free (state);
  }
}

#else

bool record_cputime (struct record_cputime_state *state, const char *prefix, dds_time_t tnow)
{
  (void) state;
  (void) prefix;
  (void) tnow;
  return false;
}

double record_cputime_read_rss (const struct record_cputime_state *state)
{
  (void) state;
  return 0.0;
}

struct record_cputime_state *record_cputime_new (dds_entity_t wr)
{
  (void) wr;
  return NULL;
}

void record_cputime_free (struct record_cputime_state *state)
{
  (void) state;
}

#endif

bool print_cputime (FILE *fp, const struct CPUStats *s, struct record_cputime_state *state, const char *prefix, bool print_host, bool is_fresh, bool data_changed, bool *cputime_changed, bool read)
{
  *cputime_changed = false;
  if (!s->some_above)
    return false;
  else
  {
    char line[512];
    char line_fp[512];
    size_t pos = 0;
    size_t pos_fp = 0;
    const size_t init_pos = pos;
    const size_t init_pos_fp = pos_fp;
    pos += (size_t) snprintf (line + pos, sizeof (line) - pos, "%s", prefix);
    if (!is_fresh)
      pos += (size_t) snprintf (line + pos, sizeof (line) - pos, " (stale)");
    if (print_host)
    {
      data_changed = true;
      int n = (int) strlen (s->hostname);
      if (n > 100) n = 100;
      pos += (size_t) snprintf (line + pos, sizeof (line) - pos, " @%-*.*s:%"PRIu32, n, n, s->hostname, s->pid);
      if (fp != NULL)
      {
        if (read)
        {
          dds_time_t tnow = dds_time ();
          int64_t tsec = (int64_t)(tnow / 1000000000);
          int64_t nsec = (int64_t)(tnow % 1000000000);
          char hostname[64];
          ddsrt_pid_t pid = ddsrt_getpid ();
          if (ddsrt_gethostname (hostname, sizeof (hostname)) != DDS_RETCODE_OK)
          {
            snprintf (hostname, sizeof (hostname), "%s", "<unknown>");
          }
          pos_fp += (size_t) snprintf (line_fp + pos_fp, sizeof (line_fp) - pos_fp,
                                       "%"PRId64".%06"PRId64",%s,%"PRIdPID",,,,,,,,,,",
                                       /* col01: epoch      */tsec, nsec / 1000,
                                       /* col02: hostname   */hostname,
                                       /* col03: pid        */pid
                                       /* col04: (empty)    */
                                       /* col05: (empty)    */
                                       /* col06: (empty)    */
                                       /* col07: (empty)    */
                                       /* col08: (empty)    */
                                       /* col09: (empty)    */
                                       /* col10: (empty)    */
                                       /* col11: (empty)    */
                                       /* col12: (empty)    */
                                       /* col12: (empty)    */
                                       /* col13: (empty)    */);
        }
        pos_fp += (size_t) snprintf (line_fp + pos_fp, sizeof (line_fp) - pos_fp,
                                     ",@%-*.*s:%"PRIu32, n, n,
                                     /* col14: hostname:pid */s->hostname, s->pid);
      }
    }
    if (s->maxrss > 1048576)
    {
      pos += (size_t) snprintf (line + pos, sizeof (line) - pos, " rss:%.1fMB", s->maxrss / 1048576.0);
    }
    else if (s->maxrss > 1024)
    {
      pos += (size_t) snprintf (line + pos, sizeof (line) - pos, " rss:%.0fkB", s->maxrss / 1024.0);
    }
    else
    {
      /* non-sensical value -- presumably maxrss is not available */
    }
    if (fp != NULL)
    {
      pos_fp += (size_t) snprintf (line_fp + pos_fp, sizeof (line_fp) - pos_fp,
                                   ",%.0f",
                                   /* col15: maxrss    */s->maxrss);
    }
    pos += (size_t) snprintf (line + pos, sizeof (line) - pos, " vcsw:%"PRIu32" ivcsw:%"PRIu32, s->vcsw, s->ivcsw);
    if (fp != NULL)
    {
      pos_fp += (size_t) snprintf (line_fp + pos_fp, sizeof (line_fp) - pos_fp,
                                   ",%"PRIu32",%"PRIu32",%d,%d",
                                   /* col16: vcsw        */s->vcsw,
                                   /* col17: ivcsw       */s->ivcsw,
                                   /* col18: cpu.maximum */(int)s->cpu._maximum,
                                   /* col19: cpu.length  */(int)s->cpu._length);
    }

    for (uint32_t i = 0; i < s->cpu._length; i++)
    {
      struct CPUStatThread * const thr_i = &s->cpu._buffer[i];
      print_one (NULL, line, sizeof (line), &pos, thr_i->name, thr_i->u_pct / 100.0, thr_i->s_pct / 100.0);
    }

    {
      uint32_t i = 0;
      for (i = 0; i < state->nthreads && i < 10; i++)
      {
        struct record_cputime_state_thr * const thr_i = &state->threads[i];
        bool found = false;
        for (uint32_t j = 0; j < s->cpu._length && j < 10; j++)
        {
          struct CPUStatThread * const thr_j = &s->cpu._buffer[j];
          if ((ddsrt_thread_list_id_t)thr_j->tid == thr_i->tid)
          {
            print_one (NULL, line, sizeof (line), &pos, thr_j->name, thr_j->u_pct / 100.0, thr_j->s_pct / 100.0);
            print_one (fp, line_fp, sizeof (line_fp), &pos_fp,
                       /* col20+(j*3)  : name        */thr_j->name,
                       /* col21+(j*3)+1: du          */thr_j->u_pct / 100.0,
                       /* col22+(j*3)+2: ds          */thr_j->s_pct / 100.0);
            found = true;
            break;
          }
        }
        if (!found)
        {
          /* col20+(j*3)  : (empty)     */
          /* col21+(j*3)+1: (empty)     */
          /* col22+(j*3)+2: (empty)     */
          print_one_empty_entry (line_fp, sizeof (line_fp), &pos_fp);
        }
      }
      for (; i < 10; i++)
      {
          /* col20+(j*3)  : (empty)     */
          /* col21+(j*3)+1: (empty)     */
          /* col22+(j*3)+2: (empty)     */
          print_one_empty_entry (line_fp, sizeof (line_fp), &pos_fp);
      }
    }

    if (pos > init_pos)
    {
      puts (line);
    }
    if (pos_fp > init_pos_fp)
    {
      if (data_changed)
      {
        if (fp != NULL)
        {
          fprintf (fp, "%s%s", line_fp, read ? "\n" : "");
          fflush (fp);
          *cputime_changed = true;
        }
      }
    }
    return true;
  }
}
