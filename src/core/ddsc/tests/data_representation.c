// Copyright(c) 2021 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "CUnit/Theory.h"
#include "dds/dds.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "test_util.h"
#include "DataRepresentationTypes.h"

#define DDS_DOMAINID1 0
#define DDS_DOMAINID2 1
#define DDS_CONFIG "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>"

#define MAX_DR 10
#define DESC(n) DataRepresentationTypes_ ## n ## _desc
#define XCDR1 DDS_DATA_REPRESENTATION_XCDR1
#define XCDR2 DDS_DATA_REPRESENTATION_XCDR2

static dds_entity_t d1, d2, dp1, dp2;

typedef void * (*sample_init_fn) (void);
typedef void (*sample_free_fn) (void *);
typedef bool (*sample_equal_fn) (const void *a, const void *b);

static void data_representation_init (void)
{
  char * conf = ddsrt_expand_envvars (DDS_CONFIG, DDS_DOMAINID1);
  d1 = dds_create_domain (DDS_DOMAINID1, conf);
  CU_ASSERT_FATAL (d1 > 0);
  ddsrt_free (conf);
  conf = ddsrt_expand_envvars (DDS_CONFIG, DDS_DOMAINID2);
  d2 = dds_create_domain (DDS_DOMAINID2, conf);
  CU_ASSERT_FATAL (d2 > 0);
  ddsrt_free (conf);

  dp1 = dds_create_participant (DDS_DOMAINID1, NULL, NULL);
  CU_ASSERT_FATAL (dp1 > 0);
  dp2 = dds_create_participant (DDS_DOMAINID2, NULL, NULL);
  CU_ASSERT_FATAL (dp2 > 0);
}

static void data_representation_fini (void)
{
  dds_delete (d1);
  dds_delete (d2);
}

static void *sample_init_type1 (void)
{
  DataRepresentationTypes_Type1 *sample = ddsrt_malloc (sizeof (*sample));
  sample->t1.s1 = 1;
  sample->t1.s2 = 2;
  sample->t1.s3 = 3;
  sample->t2 = 100;
  sample->t3 = ddsrt_strdup ("test");
  return sample;
}
static bool sample_equal_type1 (const void *a_ptr, const void *b_ptr)
{
  DataRepresentationTypes_Type1 *a = (DataRepresentationTypes_Type1 *) a_ptr,
    *b = (DataRepresentationTypes_Type1 *) b_ptr;
  return a->t1.s1 == b->t1.s1 && a->t1.s2 == b->t1.s2 && a->t1.s3 == b->t1.s3 &&
         a->t2 == b->t2 &&
         !strcmp (a->t3, b->t3);
}
static void sample_free_type1 (void *p)
{
  DataRepresentationTypes_Type1 *sample = (DataRepresentationTypes_Type1 *) p;
  ddsrt_free (sample->t3);
  ddsrt_free (sample);
}

static void *sample_init_type2 (void)
{
  DataRepresentationTypes_Type2 *sample = ddsrt_malloc (sizeof (*sample));
  for (uint32_t n = 0; n < sizeof (sample->t1) / sizeof (*sample->t1); n++)
    sample->t1[n] = (char) n;
  sample->t2 = 100;
  return sample;
}
static bool sample_equal_type2 (const void *a_ptr, const void *b_ptr)
{
  DataRepresentationTypes_Type2 *a = (DataRepresentationTypes_Type2 *) a_ptr,
    *b = (DataRepresentationTypes_Type2 *) b_ptr;
  return !memcmp (a->t1, b->t1, sizeof (a->t1) / sizeof (*a->t1)) && a->t2 == b->t2;
}
static void sample_free_type2 (void *p)
{
  DataRepresentationTypes_Type2 *sample = (DataRepresentationTypes_Type2 *) p;
  ddsrt_free (sample);
}

static void *sample_init_type3 (void)
{
  DataRepresentationTypes_Type3 *sample = ddsrt_malloc (sizeof (*sample));
  sample->t1 = 111;
  for (uint32_t n = 0; n < sizeof (sample->t2) / sizeof (*sample->t2); n++)
    sample->t2[n] = n;
  sample->t3 = 333;
  return sample;
}
static bool sample_equal_type3 (const void *a_ptr, const void *b_ptr)
{
  DataRepresentationTypes_Type3 *a = (DataRepresentationTypes_Type3 *) a_ptr,
    *b = (DataRepresentationTypes_Type3 *) b_ptr;
  return a->t1 == b->t1 && !memcmp (a->t2, b->t2, sizeof (a->t2) / sizeof (*a->t2)) && a->t3 == b->t3;
}
static void sample_free_type3 (void *p)
{
  DataRepresentationTypes_Type3 *sample = (DataRepresentationTypes_Type3 *) p;
  ddsrt_free (sample);
}

static dds_instance_handle_t write_read_sample (dds_entity_t ws, dds_entity_t wr, dds_entity_t rd, void *sample, sample_equal_fn sample_equal)
{
  dds_attach_t triggered;
  dds_return_t ret = dds_write (wr, sample);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_waitset_wait (ws, &triggered, 1, DDS_SECS(5));
  CU_ASSERT_EQUAL_FATAL (ret, 1);
  uint32_t st = 0;
  dds_get_status_changes (rd, &st);
  void * rds[1] = { NULL };
  dds_sample_info_t si[1];
  ret = dds_read (rd, rds, si, 1, 1);
  CU_ASSERT_EQUAL_FATAL (ret, 1);
  if (sample_equal)
  {
    bool eq = sample_equal (sample, rds[0]);
    CU_ASSERT_FATAL (eq);
  }
  dds_return_loan (rd, rds, 1);
  return dds_lookup_instance (rd, sample);
}

CU_Test (ddsc_data_representation, xcdr1_xcdr2, .init = data_representation_init, .fini = data_representation_fini)
{
  static const struct {
    const dds_topic_descriptor_t *desc;
    sample_init_fn sample_init;
    sample_equal_fn sample_equal;
    sample_free_fn sample_free;
  } tests[] = {
    { &DESC(Type1), sample_init_type1, sample_equal_type1, sample_free_type1 },
    { &DESC(Type2), sample_init_type2, sample_equal_type2, sample_free_type2 },
    { &DESC(Type3), sample_init_type3, sample_equal_type3, sample_free_type3 }
  };

  dds_return_t ret;
  dds_qos_t *qos_xcdr1 = dds_create_qos (), *qos_xcdr2 = dds_create_qos (), *qos_xcdr_both = dds_create_qos ();
  dds_qset_history(qos_xcdr1, DDS_HISTORY_KEEP_ALL, DDS_LENGTH_UNLIMITED);
  dds_qset_durability(qos_xcdr1, DDS_DURABILITY_TRANSIENT_LOCAL);
  dds_qset_reliability(qos_xcdr1, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);
  dds_qset_data_representation (qos_xcdr1, 1, (dds_data_representation_id_t[]) { DDS_DATA_REPRESENTATION_XCDR1 });

  ret = dds_copy_qos (qos_xcdr2, qos_xcdr1);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  dds_qset_data_representation (qos_xcdr2, 1, (dds_data_representation_id_t[]) { DDS_DATA_REPRESENTATION_XCDR2 });

  ret = dds_copy_qos (qos_xcdr_both, qos_xcdr1);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  dds_qset_data_representation (qos_xcdr_both, 2, (dds_data_representation_id_t[]) { DDS_DATA_REPRESENTATION_XCDR2, DDS_DATA_REPRESENTATION_XCDR1 });

  for (uint32_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++)
  {
    char topicname[100];
    create_unique_topic_name ("ddsc_data_representation", topicname, sizeof topicname);

    dds_entity_t tp1 = dds_create_topic (dp1, tests[i].desc, topicname, NULL, NULL);
    CU_ASSERT_FATAL (tp1 > 0);
    dds_entity_t tp2 = dds_create_topic (dp2, tests[i].desc, topicname, NULL, NULL);
    CU_ASSERT_FATAL (tp2 > 0);

    dds_entity_t rd = dds_create_reader (dp2, tp2, qos_xcdr_both, NULL);
    CU_ASSERT_FATAL (rd > 0);

    dds_entity_t wr1 = dds_create_writer (dp1, tp1, qos_xcdr1, NULL);
    CU_ASSERT_FATAL (wr1 > 0);
    sync_reader_writer (dp2, rd, dp1, wr1);

    dds_entity_t wr2 = dds_create_writer (dp1, tp1, qos_xcdr2, NULL);
    CU_ASSERT_FATAL (wr2 > 0);
    sync_reader_writer (dp2, rd, dp1, wr2);

    ret = dds_set_status_mask (rd, DDS_DATA_AVAILABLE_STATUS);
    CU_ASSERT_FATAL (ret == 0);
    dds_entity_t ws = dds_create_waitset (dp2);
    CU_ASSERT_FATAL (ws > 0);
    ret = dds_waitset_attach (ws, rd, rd);
    CU_ASSERT_FATAL (ret == 0);

    void *sample = tests[i].sample_init ();
    dds_instance_handle_t ih1 = write_read_sample (ws, wr1, rd, sample, tests[i].sample_equal);
    dds_instance_handle_t ih2 = write_read_sample (ws, wr2, rd, sample, tests[i].sample_equal);
    tests[i].sample_free (sample);
    CU_ASSERT_EQUAL_FATAL (ih1, ih2);
  }

  dds_delete_qos (qos_xcdr1);
  dds_delete_qos (qos_xcdr2);
  dds_delete_qos (qos_xcdr_both);
}

CU_Test(ddsc_data_representation, matching, .init = data_representation_init, .fini = data_representation_fini)
{
  static const struct {
    bool match;
    const dds_data_representation_id_t ids_rd[MAX_DR];
    uint32_t n_ids_rd;
    const dds_data_representation_id_t ids_wr[MAX_DR];
    uint32_t n_ids_wr;
  } tests[] = {
    { true,  { XCDR2, XCDR1 }, 2, { XCDR2 }, 1 },
    { true,  { XCDR1, XCDR2 }, 2, { XCDR2 }, 1 },
    { true,  { XCDR2, XCDR1 }, 2, { XCDR1 }, 1 },
    { true,  { XCDR1, XCDR2 }, 2, { XCDR1 }, 1 },
    { false, { XCDR2 },        1, { XCDR1 }, 1 },
    { false, { XCDR1 },        1, { XCDR2 }, 1 },
    { true,  { -1 },           0, { -1 },    0 },
    { true,  { -1 },           0, { XCDR1 }, 1 },
    { true,  { -1 },           0, { XCDR2 }, 1 },
    { true,  { XCDR1 },        1, { -1 },    0 },
    { false, { XCDR2 },        1, { -1 },    0 },
  };

  for (uint32_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++)
  {
    dds_return_t ret;
    printf ("running test %u: %s %u/%u\n", i, tests[i].match ? "true" : "false", tests[i].n_ids_rd, tests[i].n_ids_wr);
    dds_qos_t *qos_rd = dds_create_qos (), *qos_wr = dds_create_qos ();
    dds_qset_history(qos_rd, DDS_HISTORY_KEEP_ALL, DDS_LENGTH_UNLIMITED);
    dds_qset_durability(qos_rd, DDS_DURABILITY_TRANSIENT_LOCAL);
    dds_qset_reliability(qos_rd, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);

    ret = dds_copy_qos (qos_wr, qos_rd);
    CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

    if (tests[i].n_ids_rd > 0)
      dds_qset_data_representation (qos_rd, tests[i].n_ids_rd, tests[i].ids_rd);
    if (tests[i].n_ids_wr > 0)
      dds_qset_data_representation (qos_wr, tests[i].n_ids_wr, tests[i].ids_wr);

    char topicname[100];
    create_unique_topic_name ("ddsc_data_representation", topicname, sizeof topicname);
    dds_entity_t tp1 = dds_create_topic (dp1, &DataRepresentationTypes_Type1_desc, topicname, NULL, NULL);
    CU_ASSERT_FATAL (tp1 > 0);
    dds_entity_t tp2 = dds_create_topic (dp2, &DataRepresentationTypes_Type1_desc, topicname, NULL, NULL);
    CU_ASSERT_FATAL (tp2 > 0);

    dds_entity_t rd = dds_create_reader (dp2, tp2, qos_rd, NULL);
    CU_ASSERT_FATAL (rd > 0);

    dds_entity_t wr = dds_create_writer (dp1, tp1, qos_wr, NULL);
    CU_ASSERT_FATAL (wr > 0);

    if (tests[i].match)
    {
      sync_reader_writer (dp2, rd, dp1, wr);

      ret = dds_set_status_mask (rd, DDS_DATA_AVAILABLE_STATUS);
      CU_ASSERT_FATAL (ret == 0);
      dds_entity_t ws = dds_create_waitset (dp2);
      CU_ASSERT_FATAL (ws > 0);
      ret = dds_waitset_attach (ws, rd, rd);
      CU_ASSERT_FATAL (ret == 0);
      DataRepresentationTypes_Type1 sample = { { 1, 2, 3 }, "test", 4 };
      (void) write_read_sample (ws, wr, rd, &sample, NULL);

      dds_attach_t triggered;
      dds_sample_info_t si[1];
      void * rds[1] = { NULL };
      ret = dds_dispose (wr, &sample);
      CU_ASSERT_EQUAL_FATAL (ret, 0);
      ret = dds_waitset_wait (ws, &triggered, 1, DDS_SECS(5));
      CU_ASSERT_EQUAL_FATAL (ret, 1);
      ret = dds_read (rd, rds, si, 1, 1);
      CU_ASSERT_EQUAL_FATAL (ret, 1);
      CU_ASSERT_EQUAL_FATAL (si->instance_state, DDS_IST_NOT_ALIVE_DISPOSED);
      ret = dds_return_loan (rd, rds, 1);
      CU_ASSERT_FATAL (ret == 0);
    }
    else
      no_sync_reader_writer (dp2, rd, dp1, wr, DDS_MSECS (200));

    dds_delete_qos (qos_rd);
    dds_delete_qos (qos_wr);
  }
}

typedef struct datarep_ids {
    const dds_data_representation_id_t d[MAX_DR];
    uint32_t n;
} datarep_ids_t;

typedef struct datarep_qos_exp {
    datarep_ids_t set;
    bool valid;
    datarep_ids_t exp;
} datarep_qos_exp_t;

static dds_qos_t *get_qos (const datarep_qos_exp_t *d)
{
  dds_qos_t *qos = dds_create_qos ();
  if (d->set.n > 0)
    dds_qset_data_representation (qos, d->set.n, d->set.d);
  return qos;
}

static void exp_qos (dds_entity_t ent, const datarep_qos_exp_t *d)
{
  uint32_t n = 0;
  dds_data_representation_id_t *values;
  dds_qos_t *qos = dds_create_qos ();
  dds_return_t ret = dds_get_qos (ent, qos);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  bool qget = dds_qget_data_representation (qos, &n, &values);
  CU_ASSERT_FATAL (qget);
  CU_ASSERT_EQUAL_FATAL (n, d->exp.n);
  for (uint32_t i = 0; i < n; i++)
    CU_ASSERT_EQUAL_FATAL (values[i], d->exp.d[i]);
  dds_free (values);
  dds_delete_qos (qos);
}

CU_Test(ddsc_data_representation, extensibility, .init = data_representation_init, .fini = data_representation_fini)
{
#define X_ { { -1 }, 0 }
#define X1 { { XCDR1 }, 1 }
#define X1_2 { { XCDR1, XCDR2 }, 2 }
#define X2 { { XCDR2 }, 1 }
#define X2_1 { { XCDR2, XCDR1 }, 2 }
  static const struct {
    const dds_topic_descriptor_t *desc;
    datarep_qos_exp_t tp;
    datarep_qos_exp_t rd;
    datarep_qos_exp_t wr;
  } tests[] = {
    //Descriptor                   /tp                   /rd                   /wr
    { &DESC(TypeFinal),            { X_,   true, X1_2 }, { X_,   true, X1_2 }, { X_,   true, X1_2 } },
    { &DESC(TypeFinal),            { X1,   true, X1   }, { X1,   true, X1   }, { X1,   true, X1   } },
    { &DESC(TypeFinal),            { X2,   true, X2   }, { X2,   true, X2   }, { X2,   true, X2   } },
    { &DESC(TypeFinal),            { X2,   true, X2   }, { X_,   true, X2   }, { X_,   true, X2   } },
    { &DESC(TypeFinal),            { X1_2, true, X1_2 }, { X_,   true, X1_2 }, { X_,   true, X1_2 } },
    { &DESC(TypeFinal),            { X1_2, true, X1_2 }, { X2_1, true, X2_1 }, { X_,   true, X1_2 } },
    { &DESC(TypeFinal),            { X_,   true, X1_2 }, { X2,   true, X2   }, { X1,   true, X1   } },

    { &DESC(TypeAppendable),       { X_,   true,  X2 },  { X_,   true,  X2 },  { X_,   true,  X2 } },
    { &DESC(TypeAppendable),       { X1,   false, X_ },  { X_,   false, X_ },  { X_,   false, X_ } },
    { &DESC(TypeAppendable),       { X2_1, false, X_ },  { X_,   false, X_ },  { X_,   false, X_ } },
    { &DESC(TypeAppendable),       { X2,   true,  X2 },  { X_,   true,  X2 },  { X_,   true,  X2 } },
    { &DESC(TypeAppendable),       { X2,   true,  X2 },  { X1,   false, X_ },  { X2,   true,  X2 } },
    { &DESC(TypeAppendable),       { X2,   true,  X2 },  { X2_1, false, X_ },  { X2_1, false, X_ } },

    { &DESC(TypeMutable),          { X_,   true,  X2 },  { X_,   true,  X2 },  { X_,   true,  X2 } },
    { &DESC(TypeMutable),          { X2_1, false, X_ },  { X_,   false, X_ },  { X_,   false, X_ } },
    { &DESC(TypeMutable),          { X2,   true,  X2 },  { X_,   true,  X2 },  { X_,   true,  X2 } },

    { &DESC(TypeNestedAppendable), { X_,   true,  X2 },  { X_,   true,  X2 },  { X_,   true,  X2 } },
    { &DESC(TypeNestedAppendable), { X2_1, false, X_ },  { X_,   false, X_ },  { X_,   false, X_ } },
    { &DESC(TypeNestedAppendable), { X2,   true,  X2 },  { X_,   true,  X2 },  { X_,   true,  X2 } },

    { &DESC(TypeNestedMutable),    { X_,   true,  X2 },  { X_,   true,  X2 },  { X_,   true,  X2 } },
    { &DESC(TypeNestedMutable),    { X2_1, false, X_ },  { X_,   false, X_ },  { X_,   false, X_ } },
    { &DESC(TypeNestedMutable),    { X2,   true,  X2 },  { X_,   true,  X2 },  { X_,   true,  X2 } },

    { &DESC(TypeNestedMutableArr), { X_,   true,  X2 },  { X_,   true,  X2 },  { X_,   true,  X2 } },
    { &DESC(TypeNestedMutableArr), { X1,   false, X_ },  { X_,   false, X_ },  { X_,   false, X_ } },
    { &DESC(TypeNestedMutableSeq), { X_,   true,  X2 },  { X_,   true,  X2 },  { X_,   true,  X2 } },
    { &DESC(TypeNestedMutableSeq), { X1,   false, X_ },  { X_,   false, X_ },  { X_,   false, X_ } },
    { &DESC(TypeNestedMutableUni), { X_,   true,  X2 },  { X_,   true,  X2 },  { X_,   true,  X2 } },
    { &DESC(TypeNestedMutableUni), { X1,   false, X_ },  { X_,   false, X_ },  { X_,   false, X_ } }

  };
#undef X_
#undef X1
#undef X1_2
#undef X2
#undef X2_1

  char topicname[100];

  for (uint32_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++)
  {
    printf ("running test %u for type %s: ", i, tests[i].desc->m_typename);

    create_unique_topic_name ("ddsc_data_representation", topicname, sizeof topicname);

    printf ("tp ");
    dds_qos_t *qos_tp = get_qos (&tests[i].tp);
    dds_entity_t tp = dds_create_topic (dp1, tests[i].desc, topicname, qos_tp, NULL);
    CU_ASSERT_EQUAL_FATAL (tp > 0, tests[i].tp.valid);
    if (tests[i].tp.valid)
      exp_qos (tp, &tests[i].tp);
    dds_delete_qos (qos_tp);

    if (tests[i].tp.valid)
    {
      printf ("rd ");
      dds_qos_t *qos_rd = get_qos (&tests[i].rd);
      dds_entity_t rd = dds_create_reader (dp1, tp, qos_rd, NULL);
      CU_ASSERT_EQUAL_FATAL (rd > 0, tests[i].rd.valid);
      if (tests[i].rd.valid)
        exp_qos (rd, &tests[i].rd);
      dds_delete_qos (qos_rd);

      printf ("wr ");
      dds_qos_t *qos_wr = get_qos (&tests[i].wr);
      dds_entity_t wr = dds_create_writer (dp1, tp, qos_wr, NULL);
      CU_ASSERT_EQUAL_FATAL (wr > 0, tests[i].wr.valid);
      if (tests[i].wr.valid)
        exp_qos (wr, &tests[i].wr);
      dds_delete_qos (qos_wr);
    }
    printf ("\n");
  }
}

CU_Test (ddsc_data_representation, update_qos, .init = data_representation_init, .fini = data_representation_fini)
{
  dds_return_t ret;

  char topicname[100];
  create_unique_topic_name ("ddsc_data_representation", topicname, sizeof topicname);
  dds_entity_t tp1 = dds_create_topic (dp1, &DESC(TypeFinal), topicname, NULL, NULL);
  CU_ASSERT_FATAL (tp1 > 0);

  enum { RD, WR, TP } tests[] = { RD, WR, TP };
  for (uint32_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++)
  {
    dds_entity_t ent = 0;
    switch (tests[i]) {
      case RD: printf("RD\n"); ent = dds_create_reader (dp1, tp1, NULL, NULL); break;
      case WR: printf("WR\n"); ent = dds_create_writer (dp1, tp1, NULL, NULL); break;
      case TP: printf("TP\n"); ent = tp1; break;
    }
    CU_ASSERT_FATAL (ent > 0);

    {
      // data representation should be implicitly set to XCDR1, XCDR2
      datarep_qos_exp_t exp = { .exp = { { XCDR1, XCDR2 }, 2 } };
      exp_qos (ent, &exp);

      // change a mutable qos: allowed, and implicit data representation should remain unchanged
      dds_qos_t *qos = dds_create_qos ();
      dds_qset_userdata (qos, "test", 5);
      ret = dds_set_qos (ent, qos);
      CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
      exp_qos (ent, &exp);

      // change data representation: not allowed
      dds_qset_data_representation (qos, 1, (dds_data_representation_id_t[]) { XCDR2 });
      ret = dds_set_qos (ent, qos);
      CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_IMMUTABLE_POLICY);
      dds_delete_qos (qos);
    }

    {
      // get qos from entity, update mutable qos policy and set qos to entity
      dds_qos_t *qos = dds_create_qos ();
      ret = dds_get_qos (ent, qos);
      CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
      dds_qset_partition1 (qos, "test1");
      ret = dds_set_qos (ent, qos);
      CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
      dds_delete_qos (qos);
    }
  }
}


CU_Test(ddsc_data_representation, qos_annotation, .init = data_representation_init, .fini = data_representation_fini)
{
#define X_ { { -1 }, 0 }
#define X1 { { XCDR1 }, 1 }
#define X1_2 { { XCDR1, XCDR2 }, 2 }
#define X2 { { XCDR2 }, 1 }
#define X2_1 { { XCDR2, XCDR1 }, 2 }
  static const struct {
    const dds_topic_descriptor_t *desc;
    datarep_qos_exp_t tp[4];
  } tests[] = {
    { &DESC(TypeXcdr1),       { { X_, true,  X1 },   { X1, true,  X1 }, { X2, false, X_ }, { X1_2, false, X_   } } },
    { &DESC(TypeXcdr2),       { { X_, true,  X2 },   { X1, false, X_ }, { X2, true,  X2 }, { X1_2, false, X_   } } },
    { &DESC(TypeXcdr1_2),     { { X_, true,  X1_2 }, { X1, true,  X1 }, { X2, true,  X2 }, { X1_2, true,  X1_2 } } },
    { &DESC(TypeXcdr1_xml_2), { { X_, true,  X1_2 }, { X1, true,  X1 }, { X2, true,  X2 }, { X1_2, true,  X1_2 } } },
    { &DESC(TypeXcdr1_other), { { X_, true,  X1 },   { X1, true,  X1 }, { X2, false, X_ }, { X1_2, false, X_   } } },
    { &DESC(TypeXcdr2_other), { { X_, true,  X2 },   { X1, false, X_ }, { X2, true,  X2 }, { X1_2, false, X_   } } },
    //{ &DESC(TypeXcdrA1),      { { X_, false, X_ },   { X1, false, X_ }, { X2, false, X_ }, { X1_2, false, X_   } } },
    { &DESC(TypeXcdrA2),      { { X_, true,  X2 },   { X1, false, X_ }, { X2, true,  X2 }, { X1_2, false, X_   } } },
    { &DESC(TypeXcdrA1_2),    { { X_, true,  X2 },   { X1, false, X_ }, { X2, true,  X2 }, { X1_2, false, X_   } } },
  };
#undef X_
#undef X1
#undef X1_2
#undef X2
#undef X2_1

  char topicname[100];
  for (uint32_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++)
  {
    printf ("running tests for type %s \n", tests[i].desc->m_typename);
    for (uint32_t t = 0; t < 4; t++)
    {
      dds_qos_t *qos_tp = get_qos (&tests[i].tp[t]);
      create_unique_topic_name ("ddsc_data_representation", topicname, sizeof topicname);
      dds_entity_t tp = dds_create_topic (dp1, tests[i].desc, topicname, qos_tp, NULL);
      CU_ASSERT_EQUAL_FATAL (tp > 0, tests[i].tp[t].valid);
      if (tests[i].tp[t].valid)
        exp_qos (tp, &tests[i].tp[t]);
      dds_delete_qos (qos_tp);
    }
  }
}
