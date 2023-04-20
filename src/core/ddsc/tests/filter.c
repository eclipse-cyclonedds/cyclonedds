// Copyright(c) 2020 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

/* NOTICE
 *
 * The interface tested here is not part of the supported interface.  It hung
 * around by accident, and because it has proven to be useful escape at times,
 * it is best to have tests.
 *
 * The (not-too-distant) future will bring content filter expressions in the
 * reader QoS that get parsed at run-time and drop these per-topic filter
 * functions.
 */

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <limits.h>
#include <stdlib.h>

#include "dds/dds.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/attributes.h"

#include "test_common.h"

#define MAXSAMPLES 20

static bool filter_long1_eq (const void *vsample, void *arg)
{
  Space_Type1 const * const sample = vsample;
  return (uintptr_t) sample->long_1 == (uintptr_t) arg;
}

static bool filter_long2_eq (const void *vsample, void *arg)
{
  Space_Type1 const * const sample = vsample;
  return (uintptr_t) sample->long_2 == (uintptr_t) arg;
}

struct xfarg {
  dds_instance_handle_t pubhandle;
  int long_1;
  // the rest is modified on-the-fly for error checking in the filter
  // function, which is *very bad form* but useful for the test, and
  // safe in *this particular case*
  // modified by test code, checked by filter if insthandle != 0:
  dds_instance_handle_t insthandle;
  dds_view_state_t viewstate;
  dds_instance_state_t inststate;
  uint32_t dispgen;
  uint32_t nowrgen;
  // modified by filter evaluation:
  dds_instance_handle_t rejhandle;
  bool toomany;
  bool invalid_fixed;
  bool invalid_dynamic;
  bool sampleinfo_called;
  bool sample_sampleinfo_called;
};

static bool filter_sampleinfo_common (const struct dds_sample_info *si, struct xfarg *arg)
{
  if (si->sample_state != DDS_SST_NOT_READ ||
      si->sample_rank != 0 ||
      si->generation_rank != 0 ||
      si->absolute_generation_rank != 0 ||
      !si->valid_data)
  {
    arg->invalid_fixed = true;
  }

  if (arg->insthandle != 0)
  {
    if (si->instance_handle != arg->insthandle ||
        si->view_state != arg->viewstate ||
        si->instance_state != arg->inststate ||
        si->disposed_generation_count != arg->dispgen ||
        si->no_writers_generation_count != arg->nowrgen)
    {
      arg->invalid_dynamic = true;
    }
  }

  if (si->publication_handle != arg->pubhandle)
  {
    if (arg->rejhandle == 0)
      arg->rejhandle = si->publication_handle;
    else if (arg->rejhandle != si->publication_handle)
      arg->toomany = true;
  }
  return si->publication_handle == arg->pubhandle;
}

static bool filter_sampleinfo (const struct dds_sample_info *si, void *varg)
{
  struct xfarg *arg = varg;
  arg->sampleinfo_called = true;
  return filter_sampleinfo_common (si, arg);
}

static bool filter_long1_eq_sampleinfo (const void *vsample, const struct dds_sample_info *si, void *varg)
{
  Space_Type1 const * const sample = vsample;
  struct xfarg *arg = varg;
  arg->sample_sampleinfo_called = true;
  if (!filter_sampleinfo_common (si, arg))
    return false;
  return sample->long_1 == arg->long_1;
}

static int cmpdata (const void *va, const void *vb)
{
  const Space_Type1 *a = va;
  const Space_Type1 *b = vb;
  if (a->long_1 < b->long_1)
    return -1;
  else if (a->long_1 > b->long_1)
    return 1;
  else if (a->long_2 < b->long_2)
    return -1;
  else if (a->long_2 > b->long_2)
    return 1;
  else if (a->long_3 < b->long_3)
    return -1;
  else if (a->long_3 > b->long_3)
    return 1;
  else
    return 0;
}

struct exp {
  int n; // number of samples expected
  const Space_Type1 *xs; // expected data, must match exactly
  const dds_instance_state_t *is; // expected instance state (or NULL), indexed by key value
};

static void checkdata (dds_entity_t rd, const struct exp *exp, const char *headerfmt, ...)
  ddsrt_attribute_format_printf(3, 4);

static void checkdata (dds_entity_t rd, const struct exp *exp, const char *headerfmt, ...)
{
  Space_Type1 data[MAXSAMPLES + 1];
  void *raw[MAXSAMPLES + 1];
  for (int i = 0; i < MAXSAMPLES + 1; i++)
    raw[i] = &data[i];
  dds_sample_info_t si[MAXSAMPLES + 1];
  dds_return_t ret;
  va_list ap;
  assert (exp->n <= MAXSAMPLES);
  va_start (ap, headerfmt);
  vprintf (headerfmt, ap);
  va_end (ap);
  ret = dds_take (rd, raw, si, MAXSAMPLES + 1, MAXSAMPLES + 1);
  for (int k = 0; k < ret; k++)
  {
    char is = '?';
    switch (si[k].instance_state)
    {
      case DDS_ALIVE_INSTANCE_STATE: is = 'a'; break;
      case DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE: is = 'u'; break;
      case DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE: is = 'd'; break;
    }
    printf (" %c{%d,%d,%d}", is, data[k].long_1, data[k].long_2, data[k].long_3);
  }
  printf ("\n");
  fflush (stdout);
  CU_ASSERT_FATAL (ret == exp->n);
  // sort because there's no ordering between instances
  qsort (data, (size_t) ret, sizeof (data[0]), cmpdata);
  for (int k = 0; k < exp->n; k++)
  {
    CU_ASSERT_FATAL (exp->xs[k].long_1 == data[k].long_1 &&
                     exp->xs[k].long_2 == data[k].long_2 &&
                     exp->xs[k].long_3 == data[k].long_3);
    CU_ASSERT_FATAL (exp->is == NULL ||
                     exp->is[data[k].long_1] == si[k].instance_state);
  }
}

CU_Test (ddsc_filter, basic)
{
  dds_entity_t dp[2], tp[2][2], rd[2][2], wr[2][2];
  dds_return_t ret;
  char topicname[100];
  create_unique_topic_name ("ddsc_filter", topicname, sizeof (topicname));
  dds_qos_t *qos = dds_create_qos ();
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
  for (int i = 0; i < 2; i++)
  {
    dp[i] = dds_create_participant (0, NULL, NULL);
    CU_ASSERT_FATAL (dp[i] > 0);
    for (int j = 0; j < 2; j++)
    {
      tp[i][j] = dds_create_topic (dp[i], &Space_Type1_desc, topicname, qos, NULL);
      CU_ASSERT_FATAL (tp[i][j] > 0);
      rd[i][j] = dds_create_reader (dp[i], tp[i][j], qos, NULL);
      CU_ASSERT_FATAL (rd[i][j] > 0);
      wr[i][j] = dds_create_writer (dp[i], tp[i][j], qos, NULL);
      CU_ASSERT_FATAL (wr[i][j] > 0);
    }
  }
  dds_delete_qos (qos);

  ret = dds_set_topic_filter_and_arg (tp[0][0], filter_long1_eq, (void *) 1);
  CU_ASSERT_FATAL (ret == 0);
  ret = dds_set_topic_filter_and_arg (tp[1][0], filter_long2_eq, (void *) 1);
  CU_ASSERT_FATAL (ret == 0);
  for (int i = 0; i < 2; i++)
  {
    for (int j = 0; j < 2; j++)
    {
      ret = dds_write (wr[i][j], &(Space_Type1){1,i,j});
      CU_ASSERT_FATAL (ret == 0);
      ret = dds_write (wr[i][j], &(Space_Type1){2,i,j});
      CU_ASSERT_FATAL (ret == 0);
    }
  }

  // expectations for each reader:
  // - write-like(data) using a filtered topic filters
  // - write-like(key) using a filtered topic doesn't filter (attributes may be garbage)
  // - RHC always filters (and is guaranteed 0's in the attributes)
  struct exp exp[][2] = {
    [0] = {
      [0] = {
        // participant 0, topic 0: reader filtered long_1 == 1
        // writer [0][0] filtering long_1 == 1, [0][1] unfiltered
        // writer [1][0] filtering long_2 == 1, [1][1] unfiltered
        .n = 4, .xs = (const Space_Type1[]) {
          {1,0,0}, {1,0,1}, {1,1,0}, {1,1,1}
        },
      },
      [1] = {
        // participant 0, topic 1: reader unfiltered
        // writer [0][0] filtering long_1 == 1, [0][1] unfiltered
        // writer [1][0] filtering long_2 == 1, [1][1] unfiltered
        .n = 7, .xs = (const Space_Type1[]) {
          {1,0,0}, {1,0,1}, {1,1,0}, {1,1,1},
          {2,0,1}, {2,1,0}, {2,1,1}
        }
      }
    },
    [1] = {
      [0] = {
        // participant 1, topic 0: reader filtered long_2 == 1
        // writer [0][0] filtering long_1 == 1, [0][1] unfiltered
        // writer [1][0] filtering long_2 == 1, [1][1] unfiltered
        .n = 4, .xs = (const Space_Type1[]) {
          {1,1,0}, {1,1,1},
          {2,1,0}, {2,1,1}
        }
      },
      [1] = {
        // participant 1, topic 1: reader unfiltered
        // writer [0][0] filtering long_1 == 1, [0][1] unfiltered
        // writer [1][0] filtering long_2 == 1, [1][1] unfiltered
        .n = 7, .xs = (const Space_Type1[]) {
          {1,0,0}, {1,0,1}, {1,1,0}, {1,1,1},
          {2,0,1}, {2,1,0}, {2,1,1}
        }
      }
    },
  };

  for (int i = 0; i < 2; i++)
    for (int j = 0; j < 2; j++)
      checkdata (rd[i][j], &exp[i][j], "rd[%d][%d]:", i, j);
  for (int i = 0; i < 2; i++)
    dds_delete (dp[i]);
}

CU_Test (ddsc_filter, ownership)
{
  dds_entity_t dp, tp[2], rd, wr[2];
  dds_return_t ret;
  char topicname[100];
  create_unique_topic_name ("ddsc_filter", topicname, sizeof (topicname));
  dds_qos_t *qos = dds_create_qos ();
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
  dds_qset_ownership (qos, DDS_OWNERSHIP_EXCLUSIVE);
  dp = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (dp > 0);
  for (int i = 0; i < 2; i++)
  {
    tp[i] = dds_create_topic (dp, &Space_Type1_desc, topicname, qos, NULL);
    CU_ASSERT_FATAL (tp[i] > 0);
  }
  ret = dds_set_topic_filter_and_arg (tp[0], filter_long2_eq, (void *) 1);
  CU_ASSERT_FATAL (ret == 0);
  rd = dds_create_reader (dp, tp[0], qos, NULL);
  CU_ASSERT_FATAL (rd > 0);
  for (int i = 0; i < 2; i++)
  {
    dds_qset_ownership_strength (qos, i);
    wr[i] = dds_create_writer (dp, tp[i], qos, NULL);
    CU_ASSERT_FATAL (wr[i] > 0);
  }
  dds_delete_qos (qos);

  // lower strength writer: create instance (and own it)
  ret = dds_write (wr[0], &(Space_Type1){1,1,0});
  CU_ASSERT_FATAL (ret == 0);
  // higher strength writer: filtered out on attribute should not affect ownership
  // instance already exists, so it still registers
  ret = dds_write (wr[1], &(Space_Type1){1,0,1});
  CU_ASSERT_FATAL (ret == 0);
  // and so a second write of the lower-strength writer should be visible
  ret = dds_write (wr[0], &(Space_Type1){1,1,2});
  CU_ASSERT_FATAL (ret == 0);

  // higher-strength writer, filtered out on non-existent instance: no effect
  ret = dds_write (wr[1], &(Space_Type1){2,0,0});
  CU_ASSERT_FATAL (ret == 0);
  // lower-strength writer creates instance, sole registered writer
  ret = dds_write (wr[0], &(Space_Type1){2,1,1});
  CU_ASSERT_FATAL (ret == 0);

  // sanity check: higher strength wins if it not filtered out
  ret = dds_write (wr[0], &(Space_Type1){3,1,0});
  CU_ASSERT_FATAL (ret == 0);
  ret = dds_write (wr[1], &(Space_Type1){3,1,1});
  CU_ASSERT_FATAL (ret == 0);
  ret = dds_write (wr[0], &(Space_Type1){3,1,2});
  CU_ASSERT_FATAL (ret == 0);

  // unregister/auto-dispose lower strength writer:
  // - key 1 -> alive, because the higher strength writer did register
  // - key 2 -> disposed, because the higher strength writer never registered
  // - key 3 -> alive, because the higher strength still owns it
  ret = dds_delete (wr[0]);
  CU_ASSERT_FATAL (ret == 0);

  struct exp exp = {
    .n = 5, .xs = (const Space_Type1[]) {
      {1,1,0}, {1,1,2},
      {2,1,1},
      {3,1,0}, {3,1,1},
    }, .is = (const dds_instance_state_t[]) {
      [1] = DDS_ALIVE_INSTANCE_STATE,
      [2] = DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE,
      [3] = DDS_ALIVE_INSTANCE_STATE
    }
  };
  checkdata (rd, &exp, "rd");
  dds_delete (dp);
}

static bool filter_long1_eq_1 (const void *vsample)
{
  Space_Type1 const * const sample = vsample;
  return sample->long_1 == 1;
}

CU_Test (ddsc_filter, getset_extended)
{
  dds_entity_t dp, tp;
  dds_return_t ret;
  char topicname[100];
  create_unique_topic_name ("ddsc_filter", topicname, sizeof (topicname));
  dp = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (dp > 0);
  tp = dds_create_topic (dp, &Space_Type1_desc, topicname, NULL, NULL);
  CU_ASSERT_FATAL (tp > 0);

  dds_topic_filter_arg_fn fn;
  void *arg;
  struct dds_topic_filter filter;

  ret = dds_get_topic_filter_and_arg (tp, NULL, NULL);
  CU_ASSERT_FATAL (ret == 0);

  memset (&filter, 0xff, sizeof (filter));
  ret = dds_get_topic_filter_and_arg (tp, &fn, &arg);
  CU_ASSERT_FATAL (ret == 0);
  CU_ASSERT (fn == 0);
  CU_ASSERT (arg == 0);

  ret = dds_get_topic_filter_extended (tp, NULL);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_BAD_PARAMETER);

  ret = dds_set_topic_filter_extended (tp, NULL);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_BAD_PARAMETER);

  // deliberately pass in bogus value for mode
  memset (&filter.mode, 0xff, sizeof (filter.mode));
  ret = dds_set_topic_filter_extended (tp, &filter);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_BAD_PARAMETER);

  filter.mode = DDS_TOPIC_FILTER_SAMPLE;
  filter.f.sample = filter_long1_eq_1;
  filter.arg = (void *) 1;
  ret = dds_set_topic_filter_extended (tp, &filter);
  CU_ASSERT_FATAL (ret == 0);

  memset (&filter, 0xff, sizeof (filter));
  ret = dds_get_topic_filter_extended (tp, &filter);
  CU_ASSERT_FATAL (ret == 0);
  CU_ASSERT (filter.mode == DDS_TOPIC_FILTER_SAMPLE);
  CU_ASSERT (filter.f.sample == filter_long1_eq_1);
  CU_ASSERT (filter.arg == 0);

  filter.mode = DDS_TOPIC_FILTER_SAMPLE_ARG;
  filter.f.sample_arg = filter_long1_eq;
  filter.arg = (void *) 1;
  ret = dds_set_topic_filter_extended (tp, &filter);
  CU_ASSERT_FATAL (ret == 0);

  memset (&filter, 0xff, sizeof (filter));
  ret = dds_get_topic_filter_extended (tp, &filter);
  CU_ASSERT_FATAL (ret == 0);
  CU_ASSERT (filter.mode == DDS_TOPIC_FILTER_SAMPLE_ARG);
  CU_ASSERT (filter.f.sample_arg == filter_long1_eq);
  CU_ASSERT (filter.arg == (void *) 1);

  ret = dds_get_topic_filter_and_arg (tp, &fn, &arg);
  CU_ASSERT_FATAL (ret == 0);
  CU_ASSERT (fn == filter_long1_eq);
  CU_ASSERT (arg == (void *) 1);

  struct xfarg xfarg = { 0, 0 };
  filter.mode = DDS_TOPIC_FILTER_SAMPLEINFO_ARG;
  filter.f.sampleinfo_arg = filter_sampleinfo;
  filter.arg = &xfarg;
  ret = dds_set_topic_filter_extended (tp, &filter);
  CU_ASSERT_FATAL (ret == 0);

  memset (&filter, 0xff, sizeof (filter));
  ret = dds_get_topic_filter_extended (tp, &filter);
  CU_ASSERT_FATAL (ret == 0);
  CU_ASSERT (filter.mode == DDS_TOPIC_FILTER_SAMPLEINFO_ARG);
  CU_ASSERT (filter.f.sampleinfo_arg == filter_sampleinfo);
  CU_ASSERT (filter.arg == &xfarg);

  ret = dds_get_topic_filter_and_arg (tp, &fn, &arg);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_PRECONDITION_NOT_MET);

  filter.mode = DDS_TOPIC_FILTER_SAMPLE_SAMPLEINFO_ARG;
  filter.f.sample_sampleinfo_arg = filter_long1_eq_sampleinfo;
  filter.arg = &xfarg;
  ret = dds_set_topic_filter_extended (tp, &filter);
  CU_ASSERT_FATAL (ret == 0);

  memset (&filter, 0xff, sizeof (filter));
  ret = dds_get_topic_filter_extended (tp, &filter);
  CU_ASSERT_FATAL (ret == 0);
  CU_ASSERT (filter.mode == DDS_TOPIC_FILTER_SAMPLE_SAMPLEINFO_ARG);
  CU_ASSERT (filter.f.sample_sampleinfo_arg == filter_long1_eq_sampleinfo);
  CU_ASSERT (filter.arg == &xfarg);

  ret = dds_get_topic_filter_and_arg (tp, &fn, &arg);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_PRECONDITION_NOT_MET);

  dds_delete (dp);
}

CU_Test (ddsc_filter, sampleinfo)
{
  dds_entity_t dp, tp[3], rd[2], wr[2];
  dds_return_t ret;
  char topicname[100];
  create_unique_topic_name ("ddsc_filter", topicname, sizeof (topicname));
  dds_qos_t *qos = dds_create_qos ();
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
  dds_qset_writer_data_lifecycle (qos, false);
  dp = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (dp > 0);
  for (int i = 0; i < 3; i++)
  {
    tp[i] = dds_create_topic (dp, &Space_Type1_desc, topicname, qos, NULL);
    CU_ASSERT_FATAL (tp[i] > 0);
  }
  // both writers on (unfiltered) tp[0]
  for (int i = 0; i < 2; i++)
  {
    wr[i] = dds_create_writer (dp, tp[0], qos, NULL);
    CU_ASSERT_FATAL (wr[i] > 0);
  }

  // reader on filtered tp[1] accepting only data from wr[0] with key value = 1
  struct xfarg xfarg;
  memset (&xfarg, 0, sizeof (xfarg));
  ret = dds_get_instance_handle (wr[0], &xfarg.pubhandle);
  CU_ASSERT_FATAL (ret == 0);
  xfarg.long_1 = 1;
  const struct dds_topic_filter filter0 = {
    .mode = DDS_TOPIC_FILTER_SAMPLE_SAMPLEINFO_ARG,
    .f = { .sample_sampleinfo_arg = filter_long1_eq_sampleinfo },
    .arg = &xfarg
  };
  ret = dds_set_topic_filter_extended (tp[1], &filter0);
  CU_ASSERT_FATAL (ret == 0);
  rd[0] = dds_create_reader (dp, tp[1], qos, NULL);
  CU_ASSERT_FATAL (rd[0] > 0);

  // reader on filtered tp[2] accepting only data from wr[0] (but with any key value)
  const struct dds_topic_filter filter1 = {
    .mode = DDS_TOPIC_FILTER_SAMPLEINFO_ARG,
    .f = { .sampleinfo_arg = filter_sampleinfo },
    .arg = &xfarg
  };
  ret = dds_set_topic_filter_extended (tp[2], &filter1);
  CU_ASSERT_FATAL (ret == 0);
  rd[1] = dds_create_reader (dp, tp[2], qos, NULL);
  CU_ASSERT_FATAL (rd[1] > 0);
  dds_delete_qos (qos);

  // register instances for the two key values we use, so we have the instance
  // handles (which we know from Cyclone's desing to be the same for the writer
  // and the reader)
  dds_instance_handle_t ih[2];
  ret = dds_register_instance (wr[0], &ih[0], &(Space_Type1){0,0,0});
  CU_ASSERT_FATAL (ret == 0);
  ret = dds_register_instance (wr[0], &ih[1], &(Space_Type1){1,0,0});
  CU_ASSERT_FATAL (ret == 0);

  // write two samples with wr[0], only one of which passes the content filter for rd[0]
  // both of which pass for rd[1]
  // disposed, no writer counts are both 0
  xfarg.viewstate = DDS_VST_NEW;
  xfarg.inststate = DDS_IST_ALIVE;
  xfarg.insthandle = ih[0];
  ret = dds_write (wr[0], &(Space_Type1){0,0,0});
  CU_ASSERT_FATAL (ret == 0);
  CU_ASSERT (!xfarg.invalid_dynamic);
  xfarg.insthandle = ih[1];
  ret = dds_write (wr[0], &(Space_Type1){1,0,0});
  CU_ASSERT_FATAL (ret == 0);
  CU_ASSERT (!xfarg.invalid_dynamic);

  // write two samples with wr[1], neither of which passes the sample info filters
  xfarg.viewstate = DDS_VST_NEW;
  xfarg.inststate = DDS_IST_ALIVE;
  xfarg.insthandle = ih[0];
  ret = dds_write (wr[1], &(Space_Type1){0,0,0});
  CU_ASSERT_FATAL (ret == 0);
  CU_ASSERT (!xfarg.invalid_dynamic);
  xfarg.viewstate = DDS_VST_NEW;
  xfarg.inststate = DDS_IST_ALIVE;
  xfarg.insthandle = ih[1];
  ret = dds_write (wr[1], &(Space_Type1){1,0,0});
  CU_ASSERT_FATAL (ret == 0);
  CU_ASSERT (!xfarg.invalid_dynamic);
  // but does affect registrations for known samples (because we currently can't
  // distinguish between filtering on key values or attributes)
  ret = dds_unregister_instance(wr[1], &(Space_Type1){1,0,0});
  CU_ASSERT_FATAL (ret == 0);

  // check data matches expectations
  // doing this now changes the view state
  const struct exp exp0 = {
    .n = 1, .xs = (const Space_Type1[]) {
      {1,0,0},
    }, .is = (const dds_instance_state_t[]) {
      [1] = DDS_ALIVE_INSTANCE_STATE
    }
  };
  const struct exp exp1 = {
    .n = 2, .xs = (const Space_Type1[]) {
      {0,0,0},
      {1,0,0},
    }, .is = (const dds_instance_state_t[]) {
      [0] = DDS_ALIVE_INSTANCE_STATE,
      [1] = DDS_ALIVE_INSTANCE_STATE
    }
  };
  checkdata (rd[0], &exp0, "rd");
  checkdata (rd[1], &exp1, "rd");

  // write it again (so we have data in the reader), then unregister
  xfarg.viewstate = DDS_VST_OLD;
  xfarg.inststate = DDS_IST_ALIVE;
  xfarg.insthandle = ih[1];
  ret = dds_write (wr[0], &(Space_Type1){1,0,1});
  CU_ASSERT_FATAL (ret == 0);
  CU_ASSERT (!xfarg.invalid_dynamic);
  // unregister doesn't trigger a filter invocation
  ret = dds_unregister_instance (wr[0], &(Space_Type1){1,0,0});
  CU_ASSERT_FATAL (ret == 0);
  // next write, it should show up in the filter as NOT_ALIVE_NO_WRITERS
  xfarg.viewstate = DDS_VST_OLD;
  xfarg.inststate = DDS_IST_NOT_ALIVE_NO_WRITERS;
  ret = dds_write (wr[0], &(Space_Type1){1,0,2});
  CU_ASSERT_FATAL (ret == 0);
  CU_ASSERT (!xfarg.invalid_dynamic);
  // new view state and alive again; bumped no writers generation
  xfarg.viewstate = DDS_VST_NEW;
  xfarg.inststate = DDS_IST_ALIVE;
  xfarg.insthandle = ih[1];
  xfarg.nowrgen++;
  ret = dds_write (wr[0], &(Space_Type1){1,0,3});
  CU_ASSERT_FATAL (ret == 0);
  CU_ASSERT (!xfarg.invalid_dynamic);

  // dispose doesn't trigger a filter invocation
  ret = dds_dispose (wr[0], &(Space_Type1){1,0,0});
  CU_ASSERT_FATAL (ret == 0);
  CU_ASSERT (!xfarg.invalid_dynamic);
  // next write, it should show up in the filter as DISPOSED
  xfarg.viewstate = DDS_VST_NEW;
  xfarg.inststate = DDS_IST_NOT_ALIVE_DISPOSED;
  ret = dds_write (wr[0], &(Space_Type1){1,0,4});
  CU_ASSERT_FATAL (ret == 0);
  CU_ASSERT (!xfarg.invalid_dynamic);
  // alive again; bumped disposed generation
  xfarg.viewstate = DDS_VST_NEW;
  xfarg.inststate = DDS_IST_ALIVE;
  xfarg.dispgen++;
  xfarg.insthandle = ih[1];
  ret = dds_write (wr[0], &(Space_Type1){1,0,5});
  CU_ASSERT_FATAL (ret == 0);
  CU_ASSERT (!xfarg.invalid_dynamic);

  // all local, so filter evaluations happen during the call to write and we can
  // safely inspect the filter argument despite it being modified by the filter
  //
  // all samples from an unacceptable writer must be from wr[1]
  dds_instance_handle_t wr1_ih;
  ret = dds_get_instance_handle (wr[1], &wr1_ih);
  CU_ASSERT_FATAL (ret == 0);
  CU_ASSERT_FATAL (xfarg.rejhandle == wr1_ih);
  CU_ASSERT_FATAL (!xfarg.toomany);
  CU_ASSERT_FATAL (!xfarg.invalid_fixed);
  CU_ASSERT_FATAL (!xfarg.invalid_dynamic);

  CU_ASSERT_FATAL (xfarg.sampleinfo_called);
  CU_ASSERT_FATAL (xfarg.sample_sampleinfo_called);

  const struct exp expX = {
    .n = 5, .xs = (const Space_Type1[]) {
      {1,0,1}, {1,0,2}, {1,0,3}, {1,0,4}, {1,0,5}
    }
  };
  checkdata (rd[0], &expX, "rd");
  checkdata (rd[1], &expX, "rd");
  dds_delete (dp);
}

