// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/dds.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/random.h"
#include "dds/ddsrt/countargs.h"

#include "test_common.h"

#define MAX_SAMPLES                 7

/*
 * By writing, disposing, unregistering, reading and re-writing, the following data will
 * be available in the reader history and thus available for the condition that is under
 * test.
 *
 * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
 * ----------------------------------------------------------
 * |    0   |    0   |    0   |     read | old | alive      |
 * |    1   |    0   |    0   |     read | old | disposed   |
 * |    2   |    1   |    0   |     read | old | no_writers |
 * |    3   |    1   |    1   | not_read | old | alive      |
 * |    4   |    2   |    1   | not_read | new | disposed   |
 * |    5   |    2   |    1   | not_read | new | no_writers |
 * |    6   |    3   |    2   | not_read | new | alive      |
 */
#define SAMPLE_ALIVE_IST_CNT      (3)
#define SAMPLE_DISPOSED_IST_CNT   (2)
#define SAMPLE_NO_WRITER_IST_CNT  (2)
#define SAMPLE_LAST_READ_SST      (2)
#define SAMPLE_LAST_OLD_VST       (3)
#define SAMPLE_IST(idx)           (((idx % 3) == 0) ? DDS_IST_ALIVE              : \
                                   ((idx % 3) == 1) ? DDS_IST_NOT_ALIVE_DISPOSED : \
                                                      DDS_IST_NOT_ALIVE_NO_WRITERS )
#define SAMPLE_VST(idx)           ((idx <= SAMPLE_LAST_OLD_VST ) ? DDS_VST_OLD  : DDS_VST_NEW)
#define SAMPLE_SST(idx)           ((idx <= SAMPLE_LAST_READ_SST) ? DDS_SST_READ : DDS_SST_NOT_READ)


static dds_entity_t g_participant;
static dds_entity_t g_topic;
static dds_entity_t g_reader;
static dds_entity_t g_writer;
static dds_entity_t g_waitset;

static void *g_samples[MAX_SAMPLES];
static Space_Type1 g_data[MAX_SAMPLES];
static dds_sample_info_t g_info[MAX_SAMPLES];

static void readcondition_init (void)
{
  Space_Type1 sample = { 0, 0, 0 };
  dds_qos_t *qos = dds_create_qos ();
  dds_attach_t triggered;
  dds_return_t ret;
  char name[100];

  g_participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL (g_participant > 0);

  g_waitset = dds_create_waitset (g_participant);
  CU_ASSERT_FATAL (g_waitset > 0);

  g_topic = dds_create_topic (g_participant, &Space_Type1_desc, create_unique_topic_name ("ddsc_readcondition_test", name, 100), NULL, NULL);
  CU_ASSERT_FATAL (g_topic > 0);

  /* Create a reader that keeps last sample of all instances. */
  dds_qset_history (qos, DDS_HISTORY_KEEP_LAST, 1);
  g_reader = dds_create_reader (g_participant, g_topic, qos, NULL);
  CU_ASSERT_FATAL (g_reader > 0);

  /* Create a reader that will not automatically dispose unregistered samples. */
  dds_qset_writer_data_lifecycle (qos, false);
  g_writer = dds_create_writer (g_participant, g_topic, qos, NULL);
  CU_ASSERT_FATAL (g_writer > 0);

  /* Sync g_reader to g_writer. */
  ret = dds_set_status_mask (g_reader, DDS_SUBSCRIPTION_MATCHED_STATUS);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_waitset_attach (g_waitset, g_reader, g_reader);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_waitset_wait (g_waitset, &triggered, 1, DDS_SECS (1));
  CU_ASSERT_EQUAL_FATAL (ret, 1);
  CU_ASSERT_EQUAL_FATAL (g_reader, (dds_entity_t) triggered);
  ret = dds_waitset_detach (g_waitset, g_reader);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

  /* Sync g_writer to g_reader. */
  ret = dds_set_status_mask (g_writer, DDS_PUBLICATION_MATCHED_STATUS);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_waitset_attach (g_waitset, g_writer, g_writer);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_waitset_wait (g_waitset, &triggered, 1, DDS_SECS (1));
  CU_ASSERT_EQUAL_FATAL (ret, 1);
  CU_ASSERT_EQUAL_FATAL (g_writer, (dds_entity_t) triggered);
  ret = dds_waitset_detach (g_waitset, g_writer);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

  /* Initialize reading buffers. */
  memset (g_data, 0, sizeof (g_data));
  for (int i = 0; i < MAX_SAMPLES; i++)
  {
    g_samples[i] = &g_data[i];
  }

  /* Write all samples. */
  for (int i = 0; i < MAX_SAMPLES; i++)
  {
    dds_instance_state_t ist = SAMPLE_IST (i);
    sample.long_1 = i;
    sample.long_2 = i/2;
    sample.long_3 = i/3;

    ret = dds_write (g_writer, &sample);
    CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

    if (ist == DDS_IST_NOT_ALIVE_DISPOSED)
    {
      ret = dds_dispose (g_writer, &sample);
      CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
    }
    if (ist == DDS_IST_NOT_ALIVE_NO_WRITERS)
    {
      ret = dds_unregister_instance (g_writer, &sample);
      CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
    }
  }

  /* Read samples to get read&old_view states. */
  ret = dds_read (g_reader, g_samples, g_info, MAX_SAMPLES, SAMPLE_LAST_OLD_VST + 1);
  CU_ASSERT_EQUAL_FATAL (ret, SAMPLE_LAST_OLD_VST + 1);
#ifdef VERBOSE_INIT
  for (int i = 0; i < ret; i++)
  {
    Space_Type1 *s = (Space_Type1*)g_samples[i];
  }
#endif

  /* Re-write the samples that should be not_read&old_view. */
  for (int i = SAMPLE_LAST_READ_SST + 1; i <= SAMPLE_LAST_OLD_VST; i++)
  {
    dds_instance_state_t ist = SAMPLE_IST (i);
    sample.long_1 = i;
    sample.long_2 = i/2;
    sample.long_3 = i/3;

    ret = dds_write (g_writer, &sample);
    CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

    if ((ist == DDS_IST_NOT_ALIVE_DISPOSED) && (i != 4))
    {
      ret = dds_dispose (g_writer, &sample);
      CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
    }
    if (ist == DDS_IST_NOT_ALIVE_NO_WRITERS)
    {
      ret = dds_unregister_instance (g_writer, &sample);
      CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
    }
  }

  dds_delete_qos (qos);
}

static void readcondition_fini (void)
{
  dds_return_t ret = dds_delete (g_participant);
  CU_ASSERT_FATAL (ret == 0);
}

CU_Test(ddsc_readcondition_create, second, .init=readcondition_init, .fini=readcondition_fini)
{
  uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
  dds_entity_t cond1;
  dds_entity_t cond2;
  dds_return_t ret;

  cond1 = dds_create_readcondition (g_reader, mask);
  CU_ASSERT_FATAL (cond1 > 0);
  cond2 = dds_create_readcondition (g_reader, mask);
  CU_ASSERT_FATAL (cond2 > 0);

  /* Also, we should be able to delete both. */
  ret = dds_delete (cond1);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_delete (cond2);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
}

CU_Test(ddsc_readcondition_create, deleted_reader, .init=readcondition_init, .fini=readcondition_fini)
{
  uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
  dds_entity_t cond;
  dds_delete (g_reader);
  cond = dds_create_readcondition (g_reader, mask);
  CU_ASSERT_EQUAL_FATAL (cond, DDS_RETCODE_BAD_PARAMETER);
}

CU_TheoryDataPoints(ddsc_readcondition_create, invalid_readers) = {
  CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t rdr), ddsc_readcondition_create, invalid_readers, .init=readcondition_init, .fini=readcondition_fini)
{
  uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
  dds_entity_t cond;
  cond = dds_create_readcondition (rdr, mask);
  CU_ASSERT_EQUAL_FATAL (cond, DDS_RETCODE_BAD_PARAMETER);
}

CU_TheoryDataPoints(ddsc_readcondition_create, non_readers) = {
  CU_DataPoints(dds_entity_t *, &g_topic, &g_participant),
};
CU_Theory((dds_entity_t *rdr), ddsc_readcondition_create, non_readers, .init=readcondition_init, .fini=readcondition_fini)
{
  uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
  dds_entity_t cond;
  cond = dds_create_readcondition (*rdr, mask);
  CU_ASSERT_EQUAL_FATAL (cond, DDS_RETCODE_ILLEGAL_OPERATION);
}

CU_Test(ddsc_readcondition_get_mask, deleted, .init=readcondition_init, .fini=readcondition_fini)
{
  uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
  dds_entity_t condition;
  dds_return_t ret;
  condition = dds_create_readcondition (g_reader, mask);
  CU_ASSERT_FATAL (condition > 0);
  dds_delete (condition);
  mask = 0;
  ret = dds_get_mask (condition, &mask);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_readcondition_get_mask, null, .init=readcondition_init, .fini=readcondition_fini)
{
  uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
  dds_entity_t condition;
  dds_return_t ret;
  condition = dds_create_readcondition (g_reader, mask);
  CU_ASSERT_FATAL (condition > 0);
  DDSRT_WARNING_MSVC_OFF (6387); /* Disable SAL warning on intentional misuse of the API */
  ret = dds_get_mask (condition, NULL);
  DDSRT_WARNING_MSVC_ON (6387);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_delete (condition);
}

CU_TheoryDataPoints(ddsc_readcondition_get_mask, invalid_conditions) = {
  CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t cond), ddsc_readcondition_get_mask, invalid_conditions, .init=readcondition_init, .fini=readcondition_fini)
{
  dds_return_t ret;
  uint32_t mask;
  ret = dds_get_mask (cond, &mask);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
}

CU_TheoryDataPoints(ddsc_readcondition_get_mask, non_conditions) = {
  CU_DataPoints(dds_entity_t*, &g_reader, &g_topic, &g_participant),
};
CU_Theory((dds_entity_t *cond), ddsc_readcondition_get_mask, non_conditions, .init=readcondition_init, .fini=readcondition_fini)
{
  dds_return_t ret;
  uint32_t mask;
  ret = dds_get_mask (*cond, &mask);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_ILLEGAL_OPERATION);
}

CU_TheoryDataPoints(ddsc_readcondition_get_mask, various_masks) = {
  CU_DataPoints(uint32_t, DDS_ANY_SAMPLE_STATE,  DDS_READ_SAMPLE_STATE,     DDS_NOT_READ_SAMPLE_STATE),
  CU_DataPoints(uint32_t, DDS_ANY_VIEW_STATE,     DDS_NEW_VIEW_STATE,       DDS_NOT_NEW_VIEW_STATE),
  CU_DataPoints(uint32_t, DDS_ANY_INSTANCE_STATE, DDS_ALIVE_INSTANCE_STATE, DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE, DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE),
};
CU_Theory((uint32_t ss, uint32_t vs, uint32_t is), ddsc_readcondition_get_mask, various_masks, .init=readcondition_init, .fini=readcondition_fini)
{
  uint32_t maskIn  = ss | vs | is;
  uint32_t maskOut = 0xFFFFFFFF;
  dds_entity_t condition;
  dds_return_t ret;

  condition = dds_create_readcondition (g_reader, maskIn);
  CU_ASSERT_FATAL (condition > 0);

  ret = dds_get_mask (condition, &maskOut);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL (maskIn, maskOut);

  ret = dds_delete (condition);
  CU_ASSERT_FATAL (ret == 0);
}


CU_Test(ddsc_readcondition_read, already_deleted, .init=readcondition_init, .fini=readcondition_fini)
{
  dds_entity_t condition;
  dds_return_t ret;

  condition = dds_create_readcondition (g_reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE);
  CU_ASSERT_FATAL (condition > 0);

  ret = dds_delete (condition);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

  ret = dds_read (condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_readcondition_take, already_deleted, .init=readcondition_init, .fini=readcondition_fini)
{
  dds_entity_t condition;
  dds_return_t ret;

  condition = dds_create_readcondition (g_reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE);
  CU_ASSERT_FATAL (condition > 0);

  ret = dds_delete (condition);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

  ret = dds_take (condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
}

static void check_expected_long_1 (int expected_long_1, const Space_Type1 *s, const dds_sample_info_t *si)
{
  dds_sample_state_t expected_sst = SAMPLE_SST (expected_long_1);
  dds_view_state_t expected_vst = SAMPLE_VST (expected_long_1);
  dds_instance_state_t expected_ist = SAMPLE_IST (expected_long_1);

  CU_ASSERT_EQUAL_FATAL (s->long_1, expected_long_1);
  CU_ASSERT_EQUAL_FATAL (s->long_2, expected_long_1/2);
  CU_ASSERT_EQUAL_FATAL (s->long_3, expected_long_1/3);

  CU_ASSERT_EQUAL_FATAL (si->valid_data, true);
  CU_ASSERT_EQUAL_FATAL (si->sample_state, expected_sst);
  CU_ASSERT_EQUAL_FATAL (si->view_state, expected_vst);
  CU_ASSERT_EQUAL_FATAL (si->instance_state, expected_ist);
}


#define F DDS_NOT_READ_SAMPLE_STATE // "fresh" = not read
#define S DDS_READ_SAMPLE_STATE // "stale" = read
#define N DDS_NEW_VIEW_STATE
#define O DDS_NOT_NEW_VIEW_STATE // "old" = not-new
#define A DDS_ALIVE_INSTANCE_STATE
#define D DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE
#define U DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE // "unregistered"

#define X(...) (int) DDSRT_COUNT_ARGS (__VA_ARGS__), (int[]){ __VA_ARGS__ }

/* | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
 * ----------------------------------------------------------
 * |    0   |    0   |    0   |     read | old | alive      |
 * |    1   |    0   |    0   |     read | old | disposed   |
 * |    2   |    1   |    0   |     read | old | no_writers |
 * |    3   |    1   |    1   | not_read | old | alive      |
 * |    4   |    2   |    1   | not_read | new | disposed   |
 * |    5   |    2   |    1   | not_read | new | no_writers |
 * |    6   |    3   |    2   | not_read | new | alive      |
 */
static const struct readtake_tab {
  uint32_t sst, vst, ist;
  uint32_t mask;
  int expn;
  int *exp;
} readtake_tab[] = {
  { 0,0,0, 0,       X(0,1,2,3,4,5,6) },
  { F,0,0, 0,       X(3,4,5,6) },
  { S,0,0, 0,       X(0,1,2) },
  { 0,N,0, 0,       X(4,5,6) },
  { 0,O,0, 0,       X(0,1,2,3) },
  { 0,0,A, 0,       X(0,3,6) },
  { 0,0,D, 0,       X(1,4) },
  { 0,0,U, 0,       X(2,5) },
  { F,O,A, 0,       X(3) },
  { S,N,0, 0,       0, NULL },
  { F,N,A, (F|O|D), X(3,4,6) }
};

#undef X
#undef U
#undef D
#undef A
#undef O
#undef N
#undef S
#undef F

static const char *sst_str (uint32_t sst)
{
  switch (sst)
  {
    case DDS_ANY_SAMPLE_STATE: return "any";
    case DDS_READ_SAMPLE_STATE: return "read";
    case DDS_NOT_READ_SAMPLE_STATE: return "not_read";
    default: CU_ASSERT_FATAL (0); return "INVALID";
  }
}

static const char *vst_str (uint32_t vst)
{
  switch (vst)
  {
    case DDS_ANY_VIEW_STATE: return "any";
    case DDS_NEW_VIEW_STATE: return "new";
    case DDS_NOT_NEW_VIEW_STATE: return "old";
    default: CU_ASSERT_FATAL (0); return "INVALID";
  }
}

static const char *ist_str (uint32_t ist)
{
  switch (ist)
  {
    case DDS_ANY_INSTANCE_STATE: return "any";
    case DDS_ALIVE_INSTANCE_STATE: return "alive";
    case DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE: return "nowriters";
    case DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE: return "disposed";
    case DDS_ALIVE_INSTANCE_STATE | DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE:
      return "!disposed";
    case DDS_ALIVE_INSTANCE_STATE | DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE:
      return "!nowriters";
    case DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE | DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE:
      return "!alive";
    default: CU_ASSERT_FATAL (0); return "INVALID";
  }
}

static void readcondition_readtake (
        const char *opname,
        dds_return_t (*op) (dds_entity_t, void **, dds_sample_info_t *, size_t, uint32_t),
        dds_return_t (*op_mask) (dds_entity_t, void **, dds_sample_info_t *, size_t, uint32_t, uint32_t),
        uint32_t sst, uint32_t vst, uint32_t ist, uint32_t mask, int expn, int exp[])
{
  dds_entity_t condition;
  dds_return_t ret;

  if (sst == 0)
    sst = DDS_ANY_SAMPLE_STATE;
  if (vst == 0)
    vst = DDS_ANY_VIEW_STATE;
  if (ist == 0)
    ist = DDS_ANY_INSTANCE_STATE;

  condition = dds_create_readcondition (g_reader, sst | vst | ist);
  CU_ASSERT_FATAL (condition > 0);

  if (mask == 0)
  {
    ret = op (condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    printf ("readcondition_readtake: %s: check %s/%s/%s expecting %d samples, got %d\n",
            opname, sst_str (sst), vst_str (vst), ist_str (ist), expn, (int) ret);
  }
  else
  {
    ret = op_mask (condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    printf ("readcondition_readtake: %s_mask: check %s/%s/%s + %s/%s/%s expecting %d samples, got %d\n",
            opname, sst_str (sst), vst_str (vst), ist_str (ist),
            sst_str (mask & DDS_ANY_SAMPLE_STATE),
            vst_str (mask & DDS_ANY_VIEW_STATE),
            ist_str (mask & DDS_ANY_INSTANCE_STATE),
            expn, (int) ret);
  }
  for (int i = 0; i < ret; i++)
  {
    const Space_Type1 *s = g_samples[i];
    const dds_sample_info_t *si = &g_info[i];
    printf ("- %d %s/%s/%s\n", (int) s->long_1, sst_str (si->sample_state), vst_str (si->view_state), ist_str (si->instance_state));
  }

  CU_ASSERT_EQUAL_FATAL (ret, expn);
  for (int i = 0; i < expn; i++)
  {
    // sanity check the expectation itself, then the data
    CU_ASSERT_FATAL ((SAMPLE_SST (exp[i]) & (sst | mask)) != 0);
    CU_ASSERT_FATAL ((SAMPLE_VST (exp[i]) & (vst | mask)) != 0);
    CU_ASSERT_FATAL ((SAMPLE_IST (exp[i]) & (ist | mask)) != 0);
    check_expected_long_1 (exp[i], g_samples[i], &g_info[i]);
  }
}

CU_Test(ddsc_readcondition, read)
{
  for (size_t i = 0; i < sizeof (readtake_tab) / sizeof (readtake_tab[0]); i++)
  {
    const struct readtake_tab *x = &readtake_tab[i];
    readcondition_init ();
    readcondition_readtake ("read", dds_read, dds_read_mask, x->sst, x->vst, x->ist, x->mask, x->expn, x->exp);
    readcondition_fini ();
  }
}

CU_Test(ddsc_readcondition, take)
{
  for (size_t i = 0; i < sizeof (readtake_tab) / sizeof (readtake_tab[0]); i++)
  {
    const struct readtake_tab *x = &readtake_tab[i];
    readcondition_init ();
    readcondition_readtake ("take", dds_take, dds_take_mask, x->sst, x->vst, x->ist, x->mask, x->expn, x->exp);
    readcondition_fini ();
  }
}

struct writethread_arg {
  dds_entity_t wr;
  ddsrt_atomic_uint32_t stop;
};

static uint32_t writethread (void *varg)
{
  struct writethread_arg * const arg = varg;
  Space_Type1 data = { 0, 0, 0 };
  dds_return_t ret = 0;
  while (!ddsrt_atomic_ld32 (&arg->stop) && ret == 0)
  {
    data.long_3++;
    ret = dds_write (arg->wr, &data);
  }
  ddsrt_atomic_or32 (&arg->stop, (ret != 0) ? 2 : 0);
  printf ("nwrites: %d\n", (int) data.long_3);
  return 0;
}

CU_Test(ddsc_readcondition, stress)
{
#define NCONDS 10
  const dds_duration_t duration = DDS_SECS (3);
  dds_return_t rc;

  const dds_entity_t pp = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (pp > 0);

  char tpname[100];
  create_unique_topic_name ("ddsc_data_avail_stress_delete_reader", tpname, sizeof (tpname));

  dds_qos_t * const qos = dds_create_qos ();
  CU_ASSERT_FATAL (qos != NULL);
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS (1));
  dds_qset_writer_data_lifecycle (qos, false);
  const dds_entity_t tp = dds_create_topic (pp, &Space_Type1_desc, tpname, qos, NULL);
  CU_ASSERT_FATAL (tp > 0);
  dds_delete_qos (qos);

  const dds_entity_t wr = dds_create_writer (pp, tp, NULL, NULL);
  CU_ASSERT_FATAL (wr > 0);
  struct writethread_arg wrarg = {
    .wr = wr,
    .stop = DDSRT_ATOMIC_UINT32_INIT (0)
  };
  ddsrt_threadattr_t tattr;
  ddsrt_thread_t wrtid;
  ddsrt_threadattr_init (&tattr);
  rc = ddsrt_thread_create (&wrtid, "writer", &tattr, writethread, &wrarg);
  CU_ASSERT_FATAL (rc == 0);

  const dds_entity_t rd = dds_create_reader (pp, tp, NULL, NULL);
  CU_ASSERT_FATAL (rd > 0);
  const dds_entity_t ws = dds_create_waitset (pp);
  CU_ASSERT_FATAL (ws > 0);

  const dds_time_t tend = dds_time () + duration;
  uint32_t nconds = 0;
  dds_entity_t conds[NCONDS] = { 0 };
  while (!ddsrt_atomic_ld32 (&wrarg.stop) && dds_time () < tend)
  {
    const uint32_t condidx = ddsrt_random () % NCONDS;
    if (conds[condidx])
    {
      rc = dds_delete (conds[condidx]);
      CU_ASSERT_FATAL (rc == 0);
      conds[condidx] = 0;
    }

    conds[condidx] = dds_create_readcondition (rd, DDS_ANY_STATE);
    CU_ASSERT_FATAL (conds[condidx] > 0);

    // the fact that read conditions get updated even when not attached to a waitset is
    // probably a bug, so let's attach it to a waitset for good measure
    rc = dds_waitset_attach (ws, conds[condidx], conds[condidx]);
    CU_ASSERT_FATAL (rc == 0);

    // take whatever sample happens to be present: we want the read condition to be triggered
    // by the _arrival_ of a sample
    {
      Space_Type1 sample;
      void *sampleptr = &sample;
      dds_sample_info_t si;
      rc = dds_take (rd, &sampleptr, &si, 1, 1);
      CU_ASSERT_FATAL (rc == 0 || rc == 1);
    }

    while (!ddsrt_atomic_ld32 (&wrarg.stop) && dds_time () < tend && !dds_triggered (conds[condidx]))
    {
      // spin to maximize number of conditions created/deleted
    }

    nconds++;
  }
  ddsrt_atomic_or32 (&wrarg.stop, 1);
  ddsrt_thread_join (wrtid, NULL);

  printf ("nconds %"PRIu32"\n", nconds);
  printf ("stop %"PRIu32"\n", ddsrt_atomic_ld32 (&wrarg.stop));

  CU_ASSERT_FATAL (nconds > 100); // sanity check
  CU_ASSERT_FATAL (!(ddsrt_atomic_ld32 (&wrarg.stop) & 2));

  rc = dds_delete (pp);
  CU_ASSERT_FATAL (rc == 0);
#undef NCONDS
}
