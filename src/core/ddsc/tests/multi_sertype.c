// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <limits.h>

#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_endpoint_match.h"
#include "dds/ddsi/ddsi_endpoint.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "ddsi__endpoint_match.h"
#include "dds/dds.h"
#include "dds/version.h"
#include "dds__entity.h"

#include "config_env.h"
#include "test_common.h"

#define DDS_DOMAINID_PUB 0
#define DDS_DOMAINID_SUB 1
#ifdef DDS_HAS_SHM
#define DDS_CONFIG_NO_PORT_GAIN "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery><Domain id=\"any\"><SharedMemory><Enable>false</Enable></SharedMemory></Domain>"
#define DDS_CONFIG_NO_PORT_GAIN_LOG "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Tracing><OutputFile>cyclonedds_multi_sertype_tests.${CYCLONEDDS_DOMAIN_ID}.${CYCLONEDDS_PID}.log</OutputFile><Verbosity>finest</Verbosity></Tracing><Discovery><ExternalDomainId>0</ExternalDomainId></Discovery><Domain id=\"any\"><SharedMemory><Enable>false</Enable></SharedMemory></Domain>"
#else
#define DDS_CONFIG_NO_PORT_GAIN "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>"
#define DDS_CONFIG_NO_PORT_GAIN_LOG "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Tracing><OutputFile>cyclonedds_multi_sertype_tests.${CYCLONEDDS_DOMAIN_ID}.${CYCLONEDDS_PID}.log</OutputFile><Verbosity>finest</Verbosity></Tracing><Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>"
#endif

/* IDL preprocessing is not really friendly towards creating multiple descriptors
   for the same type name with different definitions, so we do it by hand. */
struct uint32_seq {
  uint32_t _maximum;
  uint32_t _length;
  uint32_t *_buffer;
  bool _release;
};

struct two_uint32 {
  uint32_t v[2];
};

struct two_uint32_seq {
  uint32_t _maximum;
  uint32_t _length;
  struct two_uint32 *_buffer;
  bool _release;
};

struct type_seq {
  struct uint32_seq x;
};

struct type_ary {
  uint32_t x[4];
};

struct type_uni {
  uint32_t _d;
  union
  {
    struct two_uint32_seq a;
    uint32_t b[4];
  } _u;
};

static const dds_topic_descriptor_t type_seq_desc =
{
  .m_size = sizeof (struct type_seq),
  .m_align = sizeof (void *),
  .m_flagset = 0u,
  .m_nkeys = 0,
  .m_typename = "multi_sertype_type",
  .m_keys = NULL,
  .m_nops = 2,
  .m_ops = (const uint32_t[]) {
    DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_4BY, offsetof (struct type_seq, x),
    DDS_OP_RTS
  },
  .m_meta = "" /* this is on its way out anyway */
};

static const dds_topic_descriptor_t type_ary_desc =
{
  .m_size = sizeof (struct type_ary),
  .m_align = 4u,
  .m_flagset = 0u,
  .m_nkeys = 0,
  .m_typename = "multi_sertype_type",
  .m_keys = NULL,
  .m_nops = 2,
  .m_ops = (const uint32_t[]) {
    DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_4BY, offsetof (struct type_ary, x), 4,
    DDS_OP_RTS
  },
  .m_meta = "" /* this is on its way out anyway */
};

static const dds_topic_descriptor_t type_uni_desc =
{
  .m_size = sizeof (struct type_uni),
  .m_align = sizeof (void *),
  .m_flagset = DDS_TOPIC_CONTAINS_UNION,
  .m_nkeys = 0,
  .m_typename = "multi_sertype_type",
  .m_keys = NULL,
  .m_nops = 8,
  .m_ops = (const uint32_t[]) {
    DDS_OP_ADR | DDS_OP_TYPE_UNI | DDS_OP_SUBTYPE_4BY | DDS_OP_FLAG_DEF, offsetof (struct type_uni, _d), 2u, (23u << 16) + 4u,
    DDS_OP_JEQ | DDS_OP_TYPE_SEQ | 6, 3, offsetof (struct type_uni, _u.a),
    DDS_OP_JEQ | DDS_OP_TYPE_ARR | 12, 0, offsetof (struct type_uni, _u.b),
    DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_STU, 0u,
    sizeof (struct two_uint32), (8u << 16u) + 4u,
    DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_4BY, offsetof (struct two_uint32, v), 2,
    DDS_OP_RTS,
    DDS_OP_RTS,
    DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_4BY, 0u, 4,
    DDS_OP_RTS,
    DDS_OP_RTS
  },
  .m_meta = "" /* this is on its way out anyway */
};

/* The slow delivery path has a switchover at 4 sertypes (well, today it has ...) so it is better to
   to test with > 4 different sertypes.  That path (again, today) iterates over GUIDs in increasing
   order, and as all readers are created in the participant and the entity ids are strictly
   monotonically increasing for the first ~ 16M entities (again, today), creating additional
   readers for these topics at the end means that "ary2" is the one that ends up in > 4 case.
   Calling takecdr */
static const dds_topic_descriptor_t type_ary1_desc =
{
  .m_size = sizeof (struct type_ary),
  .m_align = 1u,
  .m_flagset = 0u,
  .m_nkeys = 0,
  .m_typename = "multi_sertype_type",
  .m_keys = NULL,
  .m_nops = 2,
  .m_ops = (const uint32_t[]) {
    DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_1BY, offsetof (struct type_ary, x), 16,
    DDS_OP_RTS
  },
  .m_meta = "" /* this is on its way out anyway */
};

static const dds_topic_descriptor_t type_ary2_desc =
{
  .m_size = sizeof (struct type_ary),
  .m_align = 2u,
  .m_flagset = 0u,
  .m_nkeys = 0,
  .m_typename = "multi_sertype_type",
  .m_keys = NULL,
  .m_nops = 2,
  .m_ops = (const uint32_t[]) {
    DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_2BY, offsetof (struct type_ary, x), 8,
    DDS_OP_RTS
  },
  .m_meta = "" /* this is on its way out anyway */
};

static dds_entity_t g_pub_domain = 0;
static dds_entity_t g_pub_participant = 0;
static dds_entity_t g_pub_publisher = 0;

static dds_entity_t g_sub_domain = 0;
static dds_entity_t g_sub_participant = 0;
static dds_entity_t g_sub_subscriber = 0;

static void multi_sertype_init (void)
{
  /* Domains for pub and sub use a different domain id, but the portgain setting
   * in configuration is 0, so that both domains will map to the same port number.
   * This allows to create two domains in a single test process. */
  char *conf_pub = ddsrt_expand_envvars (DDS_CONFIG_NO_PORT_GAIN, DDS_DOMAINID_PUB);
  char *conf_sub = ddsrt_expand_envvars (DDS_CONFIG_NO_PORT_GAIN, DDS_DOMAINID_SUB);
  g_pub_domain = dds_create_domain (DDS_DOMAINID_PUB, conf_pub);
  g_sub_domain = dds_create_domain (DDS_DOMAINID_SUB, conf_sub);
  dds_free (conf_pub);
  dds_free (conf_sub);

  g_pub_participant = dds_create_participant(DDS_DOMAINID_PUB, NULL, NULL);
  CU_ASSERT_FATAL (g_pub_participant > 0);
  g_sub_participant = dds_create_participant(DDS_DOMAINID_SUB, NULL, NULL);
  CU_ASSERT_FATAL (g_sub_participant > 0);

  g_pub_publisher = dds_create_publisher(g_pub_participant, NULL, NULL);
  CU_ASSERT_FATAL (g_pub_publisher > 0);
  g_sub_subscriber = dds_create_subscriber(g_sub_participant, NULL, NULL);
  CU_ASSERT_FATAL (g_sub_subscriber > 0);
}

static void multi_sertype_fini (void)
{
  dds_delete (g_sub_subscriber);
  dds_delete (g_pub_publisher);
  dds_delete (g_sub_participant);
  dds_delete (g_pub_participant);
  dds_delete (g_sub_domain);
  dds_delete (g_pub_domain);
}

static bool get_and_check_writer_status (size_t nwr, const dds_entity_t *wrs, size_t nrd)
{
  dds_return_t rc;
  struct dds_publication_matched_status x;
  for (size_t i = 0; i < nwr; i++)
  {
    rc = dds_get_publication_matched_status (wrs[i], &x);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
    if (x.current_count != nrd)
      return false;
  }
  return true;
}

static bool get_and_check_reader_status (size_t nrd, const dds_entity_t *rds, size_t nwr)
{
  dds_return_t rc;
  struct dds_subscription_matched_status x;
  for (size_t i = 0; i < nrd; i++)
  {
    rc = dds_get_subscription_matched_status (rds[i], &x);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
    if (x.current_count != nwr)
      return false;
  }
  return true;
}

static void waitfor_or_reset_fastpath (dds_entity_t rdhandle, bool fastpath, size_t nwr)
{
  dds_return_t rc;
  struct dds_entity *x;

  rc = dds_entity_pin (rdhandle, &x);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  CU_ASSERT_FATAL (dds_entity_kind (x) == DDS_KIND_READER);

  struct ddsi_reader * const rd = ((struct dds_reader *) x)->m_rd;
  struct ddsi_rd_pwr_match *m;
  ddsi_guid_t cursor;
  size_t wrcount = 0;
  ddsi_thread_state_awake (ddsi_lookup_thread_state (), rd->e.gv);
  ddsrt_mutex_lock (&rd->e.lock);

  memset (&cursor, 0, sizeof (cursor));
  while ((m = ddsrt_avl_lookup_succ (&ddsi_rd_writers_treedef, &rd->writers, &cursor)) != NULL)
  {
    cursor = m->pwr_guid;
    ddsrt_mutex_unlock (&rd->e.lock);
    struct ddsi_proxy_writer * const pwr = ddsi_entidx_lookup_proxy_writer_guid (rd->e.gv->entity_index, &cursor);
    ddsrt_mutex_lock (&pwr->rdary.rdary_lock);
    if (!fastpath)
      pwr->rdary.fastpath_ok = false;
    else
    {
      while (!pwr->rdary.fastpath_ok)
      {
        ddsrt_mutex_unlock (&pwr->rdary.rdary_lock);
        dds_sleepfor (DDS_MSECS (10));
        ddsrt_mutex_lock (&pwr->rdary.rdary_lock);
      }
    }
    wrcount++;
    ddsrt_mutex_unlock (&pwr->rdary.rdary_lock);
    ddsrt_mutex_lock (&rd->e.lock);
  }

  memset (&cursor, 0, sizeof (cursor));
  while ((m = ddsrt_avl_lookup_succ (&ddsi_rd_local_writers_treedef, &rd->local_writers, &cursor)) != NULL)
  {
    cursor = m->pwr_guid;
    ddsrt_mutex_unlock (&rd->e.lock);
    struct ddsi_writer * const wr = ddsi_entidx_lookup_writer_guid (rd->e.gv->entity_index, &cursor);
    ddsrt_mutex_lock (&wr->rdary.rdary_lock);
    if (!fastpath)
      wr->rdary.fastpath_ok = fastpath;
    else
    {
      while (!wr->rdary.fastpath_ok)
      {
        ddsrt_mutex_unlock (&wr->rdary.rdary_lock);
        dds_sleepfor (DDS_MSECS (10));
        ddsrt_mutex_lock (&wr->rdary.rdary_lock);
      }
    }
    wrcount++;
    ddsrt_mutex_unlock (&wr->rdary.rdary_lock);
    ddsrt_mutex_lock (&rd->e.lock);
  }
  ddsrt_mutex_unlock (&rd->e.lock);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
  dds_entity_unpin (x);

  CU_ASSERT_FATAL (wrcount == nwr);
}

static const struct ddsi_sertype *get_sertype_from_reader (dds_entity_t reader)
{
  /* not refcounting the sertopic: so this presumes it is kept alive for other reasons */
  dds_return_t rc;
  struct dds_entity *x;
  struct dds_reader *rd;
  const struct ddsi_sertype *sertype;
  rc = dds_entity_pin (reader, &x);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  CU_ASSERT_FATAL (dds_entity_kind (x) == DDS_KIND_READER);
  rd = (struct dds_reader *) x;
  sertype = rd->m_topic->m_stype;
  dds_entity_unpin (x);
  return sertype;
}

static void logsink (void *arg, const dds_log_data_t *msg)
{
  ddsrt_atomic_uint32_t *deser_fail = arg;
  fputs (msg->message - msg->hdrsize, stderr);
  if (strstr (msg->message, "deserialization") && strstr (msg->message, "failed"))
    ddsrt_atomic_inc32 (deser_fail);
}

enum multi_sertype_mode {
  MSM_FASTPATH,
  MSM_SLOWPATH,
  MSM_TRANSLOCAL
};

static const char *multi_sertype_modestr (enum multi_sertype_mode mode)
{
  switch (mode)
  {
    case MSM_FASTPATH: return "fastpath";
    case MSM_SLOWPATH: return "slowpath";
    case MSM_TRANSLOCAL: return "transient-local";
  }
  return "?";
}

static void create_readers (dds_entity_t pp_sub, size_t nrds, dds_entity_t *rds, size_t ntps, const dds_entity_t *tps, const dds_qos_t *qos, dds_entity_t waitset)
{
  assert (nrds >= ntps && (nrds % ntps) == 0);
  for (size_t i = 0; i < ntps; i++)
  {
    rds[i] = dds_create_reader (pp_sub, tps[i], qos, NULL);
    CU_ASSERT_FATAL (rds[i] > 0);
  }
  for (size_t i = ntps; i < nrds; i++)
  {
    rds[i] = dds_create_reader (pp_sub, tps[(i - ntps) / (nrds / ntps - 1)], qos, NULL);
    CU_ASSERT_FATAL (rds[i] > 0);
  }

  for (size_t i = 0; i < nrds; i++)
  {
    dds_return_t rc;
    rc = dds_set_status_mask (rds[i], DDS_SUBSCRIPTION_MATCHED_STATUS | DDS_DATA_AVAILABLE_STATUS);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
    rc = dds_waitset_attach (waitset, rds[i], (dds_attach_t)i);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  }
}

static void ddsc_multi_sertype_impl (dds_entity_t pp_pub, dds_entity_t pp_sub, enum multi_sertype_mode mode)
{
#define SEQ_IDX 0
#define ARY_IDX 1
#define UNI_IDX 2
  char name[100];
  static const dds_topic_descriptor_t *descs[] = {
    &type_seq_desc, &type_ary_desc, &type_uni_desc,
    &type_ary1_desc, &type_ary2_desc
  };
  dds_entity_t pub_topics[3], writers[3];
  dds_entity_t sub_topics[5];
  dds_entity_t readers[15];
  dds_entity_t waitset;
  dds_qos_t *qos;
  dds_return_t rc;

  printf ("multi_sertype: %s %s\n", (pp_pub == pp_sub) ? "local" : "remote", multi_sertype_modestr (mode));

  /* Transient-local mode is for checking the local historical data delivery path (for remote, there
     is nothing special about it), and knowing it is local means we don't have to wait for historical
     data to arrive.  So check. */
  assert (pp_pub == pp_sub || mode != MSM_TRANSLOCAL);

  waitset = dds_create_waitset (DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL (waitset > 0);

  qos = dds_create_qos ();
  CU_ASSERT_FATAL (qos != NULL);
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);
  dds_qset_destination_order (qos, DDS_DESTINATIONORDER_BY_RECEPTION_TIMESTAMP);
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
  dds_qset_type_consistency (qos, DDS_TYPE_CONSISTENCY_ALLOW_TYPE_COERCION, false, false, false, false, false);
  if (mode == MSM_TRANSLOCAL)
  {
    dds_qset_durability (qos, DDS_DURABILITY_TRANSIENT_LOCAL);
    dds_qset_durability_service (qos, 0, DDS_HISTORY_KEEP_ALL, 0, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED);
  }

  create_unique_topic_name ("ddsc_multi_sertype_lease_duration_zero", name, sizeof name);

  for (size_t i = 0; i < sizeof (pub_topics) / sizeof (pub_topics[0]); i++)
  {
    pub_topics[i] = dds_create_topic (pp_pub, descs[i], name, qos, NULL);
    CU_ASSERT_FATAL (pub_topics[i] > 0);
  }
  for (size_t i = 0; i < sizeof (writers) / sizeof (writers[0]); i++)
  {
    writers[i] = dds_create_writer (pp_pub, pub_topics[i], qos, NULL);
    CU_ASSERT_FATAL (writers[i] > 0);
  }
  for (size_t i = 0; i < sizeof (sub_topics) / sizeof (sub_topics[0]); i++)
  {
    sub_topics[i] = dds_create_topic (pp_sub, descs[i], name, qos, NULL);
    CU_ASSERT_FATAL (sub_topics[i] > 0);
  }

  if (mode != MSM_TRANSLOCAL)
  {
    create_readers (pp_sub, sizeof (readers) / sizeof (readers[0]), readers, sizeof (sub_topics) / sizeof (sub_topics[0]), sub_topics, qos, waitset);

    for (size_t i = 0; i < sizeof (writers) / sizeof (writers[0]); i++)
    {
      rc = dds_set_status_mask (writers[i], DDS_PUBLICATION_MATCHED_STATUS);
      CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
      rc = dds_waitset_attach (waitset, writers[i], -(dds_attach_t)i - 1);
      CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
    }

    printf ("wait for discovery, fastpath_ok; delete & recreate readers\n");
    while (!(get_and_check_writer_status (sizeof (writers) / sizeof (writers[0]), writers, sizeof (readers) / sizeof (readers[0])) &&
             get_and_check_reader_status (sizeof (readers) / sizeof (readers[0]), readers, sizeof (writers) / sizeof (writers[0]))))
    {
      rc = dds_waitset_wait (waitset, NULL, 0, DDS_SECS(5));
      CU_ASSERT_FATAL (rc >= 1);
    }

    /* we want to check both the fast path and the slow path ... so first wait
       for it to be set on all (proxy) writers, then possibly reset it */
    for (size_t i = 0; i < sizeof (readers) / sizeof (readers[0]); i++)
      waitfor_or_reset_fastpath (readers[i], true, sizeof (writers) / sizeof (writers[0]));
    if (mode == MSM_SLOWPATH)
    {
      printf ("clear fastpath_ok\n");
      for (size_t i = 0; i < sizeof (readers) / sizeof (readers[0]); i++)
        waitfor_or_reset_fastpath (readers[i], false, sizeof (writers) / sizeof (writers[0]));
    }
  }

  /* check the log output for deserialization failures */
  ddsrt_atomic_uint32_t deser_fail = DDSRT_ATOMIC_UINT32_INIT (0);
  dds_set_log_sink (logsink, &deser_fail);

  /* Write one of each type: all of these samples result in the same serialised
     form but interpreting the memory layout for type X as-if it were of type Y
     wreaks havoc. */
  {
    struct type_seq s = {
      .x = {
        ._length = 3, ._maximum = 3, ._release = false, ._buffer = (uint32_t[]) { 1, 4, 2 }
      }
    };
    struct type_ary a = {
      .x = { 3, 1, 4, 2 }
    };
    struct type_uni u = {
      ._d = 3,
      ._u = { .a = {
        ._length = 1, ._maximum = 1, ._release = false, ._buffer = (struct two_uint32[]) { { { 4, 2 } } }
      } }
    };
    printf ("writing ...\n");
    rc = dds_write_ts (writers[SEQ_IDX], &s, 1);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
    rc = dds_write_ts (writers[ARY_IDX], &a, 2);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
    rc = dds_write_ts (writers[UNI_IDX], &u, 3);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);

    /* Also write a sample that can't be deserialised by the other types */
    struct type_seq s1 = {
      .x = {
        ._length = 1, ._maximum = 1, ._release = false, ._buffer = (uint32_t[]) { 1 }
      }
    };
    rc = dds_write_ts (writers[SEQ_IDX], &s1, 4);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  }

  if (mode == MSM_TRANSLOCAL)
  {
    create_readers (pp_sub, sizeof (readers) / sizeof (readers[0]), readers, sizeof (sub_topics) / sizeof (sub_topics[0]), sub_topics, qos, waitset);
  }

  /* All readers should have received three samples, and those that are of type seq
     should have received one extra (whereas the others should cause deserialization
     failure warnings) */
  printf ("reading\n");
  const size_t nexp = ((sizeof (writers) / sizeof (writers[0])) *
                         (sizeof (readers) / sizeof (readers[0])) +
                         ((sizeof (readers) / sizeof (readers[0])) / (sizeof (sub_topics) / sizeof (sub_topics[0]))));
  /* For the volatile case, expecting exactly as many deserialization failures as there
     are topics other than seq because the conversion is done only once for each topic,
     even if there are multiple readers.  For transient-local data, the data set is
     converted for each new reader (of a different topic) and there will therefore be more
     conversion failures. */
  const size_t nexp_fail =
    (sizeof (sub_topics) / sizeof (sub_topics[0]) - 1) *
    (mode != MSM_TRANSLOCAL ? 1 : (sizeof (readers) / sizeof (readers[0])) / (sizeof (sub_topics) / sizeof (sub_topics[0])));
  uint32_t nseen = 0;
  while (nseen < nexp)
  {
    dds_sample_info_t si;

    rc = dds_waitset_wait (waitset, NULL, 0, DDS_SECS (5));
    CU_ASSERT_FATAL (rc >= 1);

    {
      struct type_seq s = { .x = { 0 } };
      void *raws[] = { &s };
      while (dds_take (readers[SEQ_IDX], raws, &si, 1, 1) == 1)
      {
        if (!si.valid_data)
          continue;
        printf ("recv: seq %"PRId64"\n", si.source_timestamp);
        if (si.source_timestamp == 4)
        {
          CU_ASSERT_FATAL (s.x._length == 1);
          CU_ASSERT_FATAL (s.x._buffer[0] == 1);
        }
        else
        {
          CU_ASSERT_FATAL (si.source_timestamp >= 1 && si.source_timestamp <= 3);
          CU_ASSERT_FATAL (s.x._length == 3);
          CU_ASSERT_FATAL (s.x._buffer[0] == 1);
          CU_ASSERT_FATAL (s.x._buffer[1] == 4);
          CU_ASSERT_FATAL (s.x._buffer[2] == 2);
        }
        nseen++;
      }
      dds_free (s.x._buffer);
    }

    {
      struct type_ary a;
      void *rawa[] = { &a };
      while (dds_take (readers[ARY_IDX], rawa, &si, 1, 1) == 1)
      {
        if (!si.valid_data)
          continue;
        printf ("recv: ary %"PRId64"\n", si.source_timestamp);
        CU_ASSERT_FATAL (si.source_timestamp >= 1 && si.source_timestamp <= 3);
        CU_ASSERT_FATAL (a.x[0] == 3);
        CU_ASSERT_FATAL (a.x[1] == 1);
        CU_ASSERT_FATAL (a.x[2] == 4);
        CU_ASSERT_FATAL (a.x[3] == 2);
        nseen++;
      }
    }

    {
      struct type_uni u = { ._u.a = { 0 } };
      void *rawu[] = { &u };
      while (dds_take (readers[UNI_IDX], rawu, &si, 1, 1) == 1)
      {
        if (!si.valid_data)
          continue;
        printf ("recv: uni %"PRId64"\n", si.source_timestamp);
        CU_ASSERT_FATAL (si.source_timestamp >= 1 && si.source_timestamp <= 3);
        CU_ASSERT_FATAL (u._d == 3);
        CU_ASSERT_FATAL (u._u.a._length == 1);
        assert (u._u.a._buffer != NULL); /* for Clang static analyzer */
        CU_ASSERT_FATAL (u._u.a._buffer[0].v[0] == 4);
        CU_ASSERT_FATAL (u._u.a._buffer[0].v[1] == 2);
        dds_free (u._u.a._buffer);
        u._u.a._buffer = NULL;
        nseen++;
      }
    }

    DDSRT_STATIC_ASSERT (((1u << SEQ_IDX) | (1u << ARY_IDX) | (1u << UNI_IDX)) == 7);
    for (size_t i = 3; i < sizeof (readers) / sizeof (readers[0]); i++)
    {
      struct ddsi_serdata *sample;
      while (dds_takecdr (readers[i], &sample, 1, &si, DDS_ANY_STATE) == 1)
      {
        if (!si.valid_data)
          continue;
        printf ("recv: reader %zu %"PRId64"\n", i, si.source_timestamp);
        CU_ASSERT_FATAL (sample->type == get_sertype_from_reader (readers[i]));
        ddsi_serdata_unref (sample);
        nseen++;
      }
    }
  }
  CU_ASSERT_FATAL (nseen == nexp);

  /* data from remote writers can cause a deserialization failure after all
     expected samples have been seen (becasue it is written last); so wait
     for them */
  while (ddsrt_atomic_ld32 (&deser_fail) < nexp_fail)
    dds_sleepfor (DDS_MSECS (10));
  CU_ASSERT_FATAL (ddsrt_atomic_ld32 (&deser_fail) == nexp_fail);

  /* deleting the waitset is important: it is bound to the library rather than to
     a domain and consequently won't be deleted simply because all domains are */
  rc = dds_delete (waitset);
  dds_delete_qos (qos);

  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  dds_set_log_sink (0, NULL);
}

CU_Test(ddsc_multi_sertype, local, .init = multi_sertype_init, .fini = multi_sertype_fini)
{
  ddsc_multi_sertype_impl (g_pub_participant, g_pub_participant, MSM_FASTPATH);
}

CU_Test(ddsc_multi_sertype, remote, .init = multi_sertype_init, .fini = multi_sertype_fini)
{
  ddsc_multi_sertype_impl (g_pub_participant, g_sub_participant, MSM_FASTPATH);
}

CU_Test(ddsc_multi_sertype, local_slowpath, .init = multi_sertype_init, .fini = multi_sertype_fini)
{
  ddsc_multi_sertype_impl (g_pub_participant, g_pub_participant, MSM_SLOWPATH);
}

CU_Test(ddsc_multi_sertype, remote_slowpath, .init = multi_sertype_init, .fini = multi_sertype_fini)
{
  ddsc_multi_sertype_impl (g_pub_participant, g_sub_participant, MSM_SLOWPATH);
}

CU_Test(ddsc_multi_sertype, transient_local, .init = multi_sertype_init, .fini = multi_sertype_fini)
{
  ddsc_multi_sertype_impl (g_pub_participant, g_pub_participant, MSM_TRANSLOCAL);
}
